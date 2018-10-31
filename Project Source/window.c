#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <ShObjIdl.h>
#include "dthread.h"

#ifdef _DEBUG
#	pragma comment(lib, "libcurld.lib")
#else 
#	pragma comment(lib, "libcurl.lib")
#endif // _DEBUG

static inline void putws(const wchar_t *IP_text); // fputws를 자주 사용하기에 좀 더 효율적으로 처리하기 위하여 inline으로 선언

extern BOOL dctrl_monitor(DCTRL *pCtl, HANDLE *phFile); //평균속도와 순간속도를 모니터링 하는 함수
extern BOOL check_disk_space(DCTRL *pCtl, OLECHAR *pFilename); //다운받은 파일이 저장되는 디렉토리가 포함된 디스크의 남은 용량을 표시

extern BOOL open_filedlg(OLECHAR **ppFilename, URLINFO *pUrlinfo, HANDLE *phFile); // 파일 저장 하는 window 창 띄우는 함수
extern BOOL open_file(HANDLE *phFile, OLECHAR *pFilename); //파일포인터 열기, 가져오기

static void Remove_New_Line(char *buffer); // 받아온 링크의 맨 마지막에 위치한 \n을 삭제하는 함수
static void Error_Box(const wchar_t * IP_Text);; //실패시 에러 메시지 출력하는 함수
static void Print_Speed(double speed); // 평균속도 + 순간속도 출력하는 함수
static void Print_Bytes(unsigned long long bytes); // 다운받을 경로에서 남은 용량을 알려주는 함수

//---------------------------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]) {
	//메인 함수가 위치한 파일입니다.
	char buf[1024] = { NULL }; // url 주소를 받는 버퍼. 크기가 충분하도록 1024로 선언
	DCTRL ctl;
	DCTRL *pCtl = &ctl;
	OLECHAR *pFilename = NULL;
	
HANDLE hFile = INVALID_HANDLE_VALUE;

	//FAIL OCCUR
	if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY))) {
		Error_Box(L"COM Initialization failed.");
		return 1;
	}
	if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
		Error_Box(L"CURL Initialization failed.");
		CoUninitialize();
		return 2;
	}
	if (!create_dctrl(pCtl, 64)) { //스레드를 더 많이 생성하고 싶다면, 64를 더 큰 숫자, 또는 더 작은 숫자로 수정
		Error_Box(L"CTRL Initialization failed.");
		curl_global_cleanup();
		CoUninitialize();
		return 3;
	}

	while (1) {
		putws(L"If you want to exit, type c. Copy your URL link : ");
		//fgets(buf, 1024, argv[1]); //link대신 원래 buf였습니다.
		for (int i = 0; i < strlen(argv[1]);i++)
		{
			buf[i] = argv[1][i];
		}
		printf("%s\n", buf);
		if (buf[0] == '\n' || strcmp("c\n", buf, 2) == 0) { //c를 입력하거나 엔터를 입력하면 프로그램 종료
			break;
		}
		//Sanity check
		if (strncmp("https://", buf, 8) != 0 && strncmp("http://", buf, 7) != 0) {
			putws(L"ERROR : Hyperlink not valid.\n");
			continue;
		}
		Remove_New_Line(buf);

		putws(L"Getting contents size...\n");
		dctrl_get_csize(pCtl, buf, INFINITE);
		if (ctl.urlInfo.csize == 0ULL) {//컨텐츠의 size가 0인경우, 파일이 존재하지 않는 경우
			putws(L"ERROR : Server not responding.\n");
			continue;
		}
		else if (!ctl.urlInfo.bAvailable) {//웹서버에서 분할 다운로드를 막아놓은 경우
			putws(L"ERROR: SERVER does not allow partitial request.\n");
			continue;
		}

		putws(L"Content Size: ");
		Print_Bytes(ctl.urlInfo.csize);
		if (ctl.urlInfo.ctype)
			putws(L"\nContent Type: %s \n", ctl.urlInfo.ctype);
		else
			putws(L"\n");

		if (pFilename) {
			CoTaskMemFree(pFilename);
			pFilename = NULL;
		}
		if (!open_filedlg(&pFilename, &ctl.urlInfo, NULL)) { //에러메시지 출력하는 박스가 작동하지 않을 경우
			Error_Box(L"Can't Open dialog box.");
			CoTaskMemFree(pFilename);
			pFilename = NULL;
			break;
		}
		if (!check_disk_space(pCtl, pFilename)) {
			CoTaskMemFree(pFilename);
			pFilename = NULL;
			continue;
		}
		if (hFile != INVALID_HANDLE_VALUE)
			CloseHandle(hFile);
		if (!open_file(&hFile, pFilename)) {
			putws(L"Error : Unable to write the file '");
			putws(pFilename);
			putws(L"'.\n");
			CoTaskMemFree(pFilename);
			pFilename = NULL;
			continue;
		}
		if (!dctrl_start(&ctl, hFile)) {
			putws(L"Error : Failed to launch dctrl.\n");
			CoTaskMemFree(pFilename);
			pFilename = NULL;
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
			continue;
		}
		if (!dctrl_monitor(&ctl, &hFile)) {
			dctrl_commit(pCtl);
			break;
		}
		dctrl_commit(pCtl);
	}

	if (pFilename) 
		CoTaskMemFree(pFilename);

	destroy_dctrl(pCtl);
	curl_global_cleanup();
	
	return 0;
}

//---------------------------------------------------------------------------------------------------------------------

static inline void putws(const wchar_t * IP_Text) {
	fputws(IP_Text, stdout); // 
}

extern BOOL dctrl_monitor(DCTRL *pCtl, HANDLE *phFile) { //현재 파일의 용량 차이를 이용해서 속도를 계산
	HANDLE hFile = *phFile;
	unsigned long long prevDLBytes = 0ULL;
	unsigned long long dlBytes;
	size_t i;
	int nSleepingThrds;
	DWORD dwStartTick = GetTickCount();
	DWORD dwPrevTick = dwStartTick;
	DWORD dwNowTick;
	double instant_speed;
	double average_speed;//workshop

	for (; ; prevDLBytes = dlBytes, dwPrevTick = dwNowTick) {
		/*
		if (!dctrl_cache_flush(pCtl, hFile, 300) || pCtl->shutdown.val) {
		ErrorBox(L"Fatal Error : Failed to flush the cache onto disk.");
		return FALSE;
		}
		*/
		if (dctrl_wait(pCtl, TRUE, 300) == WAIT_OBJECT_0)
			break;
		dwNowTick = GetTickCount();
		dlBytes = 0ULL;
		for (i = 0; i < pCtl->nThrd; i++)
			dlBytes += pCtl->pThrdInfo[i].dlBytes;
		if (dwNowTick == dwPrevTick)
			instant_speed = 0;
		else
			instant_speed = (double)(dlBytes - prevDLBytes) * (double)1000 / (double)(dwNowTick - dwPrevTick);
		putws(L"Instantaneous speed = ");
		Print_Speed(instant_speed);
		putws(L"\nAverage speed = ");
		if (dwNowTick == dwStartTick)
			average_speed = 0;
		else
			average_speed = (double)dlBytes * (double)1000 / (double)(dwNowTick - dwStartTick);
		Print_Speed(average_speed);
		wprintf(L" (%.2lf%c)\n", (double)dlBytes / (double)pCtl->urlInfo.csize * (double)100, '%');
		//fputwc(L'\n', stdout);

		nSleepingThrds = 0;
		for (i = 0; i < pCtl->nThrd; i++) {
			nSleepingThrds += (pCtl->pThrdInfo[i].bSleeping == TRUE);
		}
		if (nSleepingThrds)
			wprintf(L"%d out of %d threads are waiting.\n", nSleepingThrds, (int)pCtl->nThrd);

		if (nSleepingThrds == pCtl->nThrd)
			break;
		//wprintf(L"wait returns %d, error = %d\n", (int)dctrl_wait(pCtl, TRUE, 0), GetLastError());
	}
	putws(L"--------------------");
	putws(L"Done downloading!\n");
	putws(L"Average speed = ");
	dwNowTick = GetTickCount();
	average_speed = (double)pCtl->urlInfo.csize * (double)1000 / (double)(dwNowTick - dwStartTick);
	Print_Speed(average_speed);
	putws(L"\nElapsed time = ");
	wprintf(L"%.2lf", (double)(dwNowTick - dwStartTick) / (double)1000);
	putws(L" [sec]\n");
	CloseHandle(hFile);
	*phFile = INVALID_HANDLE_VALUE;
	return TRUE;
}

extern BOOL check_disk_space(DCTRL *pCtl, OLECHAR *pFilename) {
	OLECHAR *pBackslash;
	OLECHAR temp;
	ULARGE_INTEGER total_NumberOfBytes;
	ULARGE_INTEGER free_BytesAvailable;
	ULARGE_INTEGER total_NumberOfFreeBytes;

	pBackslash = wcschr(pFilename, L'\\');//\\탐색
	if (pBackslash) {
		temp = pBackslash[1];
		pBackslash[1] = L'\0';
	}

	if (GetDiskFreeSpaceExW(pFilename, &free_BytesAvailable, &total_NumberOfBytes, &total_NumberOfFreeBytes)) {
		putws(L"Available Memory Space : ");
		Print_Bytes(free_BytesAvailable.QuadPart);
		fputwc(L'\n', stdout);
		if (pCtl->urlInfo.csize > free_BytesAvailable.QuadPart) {
			Error_Box(L"Not enough space for download!");
			if (pBackslash)
				pBackslash[1] = temp;
			return FALSE;
		}

		putws(L"Memory left after downloading the contents : ");
		Print_Bytes(free_BytesAvailable.QuadPart - pCtl->urlInfo.csize);
		fputwc(L'\n', stdout);
	}

	if (pBackslash)
		pBackslash[1] = temp;
	return TRUE;
}

extern BOOL open_filedlg(OLECHAR **ppFilename, URLINFO *pUrlinfo, HANDLE *phFile) {
	HRESULT hr;
	COMDLG_FILTERSPEC rgSaveTypes[] = {
		(L"All files", L"*.*")
	};
	IFileSaveDialog *pDlg;
	IShellItem *pItem;
	wchar_t *pDefaultName;

	hr = CoCreateInstance( //다른 이름으로 저장 누르면 뜨는 그 창에 대한 것들 
		&CLSID_FileSaveDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IFileSaveDialog,
		&pDlg);
	if (FAILED(hr))
		return FALSE;

	pDefaultName = NULL; //(이유를 알 수 없는 C2040 Error) url2filename(pUrlinfo->url); //curl에서 넘겨받은 url 정보

	if (pDefaultName) {
		pDlg->lpVtbl->SetFileName(pDlg, pDefaultName);
		free(pDefaultName);
	}
	pDlg->lpVtbl->SetFileTypes(pDlg, _countof(rgSaveTypes), rgSaveTypes);
	pDlg->lpVtbl->SetFileTypeIndex(pDlg, 0);
	pDlg->lpVtbl->SetTitle(pDlg, L"Specify the filename and path you want to save to");
	hr = pDlg->lpVtbl->Show(pDlg, NULL);
	if (FAILED(hr))
		goto release_dlg;
	hr = pDlg->lpVtbl->GetResult(pDlg, &pItem);
	if (FAILED(hr))
		goto release_dlg;
	hr = pItem->lpVtbl->GetDisplayName(pItem, SIGDN_FILESYSPATH, ppFilename);
	if (FAILED(hr))
		goto release_item;
	pItem->lpVtbl->Release(pItem);
	pDlg->lpVtbl->Release(pDlg);
	return TRUE;
release_item:
	pItem->lpVtbl->Release(pItem);
release_dlg:
	pDlg->lpVtbl->Release(pDlg);
	return FALSE;
}

extern BOOL open_file(HANDLE *phFile, OLECHAR *pFilename) {
	*phFile = CreateFileW(
		pFilename,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	return (*phFile != INVALID_HANDLE_VALUE);
}

static void Remove_New_Line(char *buffer) {
	char *newline = strchr(buffer, (int)'\n'); //버퍼에 들어있는 URL 링크에서 마지막 \n의 위치를 넣음

	if (newline)
		*newline = '\0'; //\n을 삭제, 올바른 URL로 접속하기 위함
}

static void Error_Box(const wchar_t * IP_Text) {
	MessageBoxW(NULL, IP_Text, L"Error", MB_OK | MB_ICONERROR);//message box contains one push button:OK, Stop sign error icon appears in message box
}

static void Print_Speed(double speed) {
	const double tb = (double)1099511627776; //2^40 TB
	const double gb = (double)1073741824; //2^30 GB
	const double mb = (double)1048576; //2^20 MB
	const double kb = (double)1024; //2^10 KB

	if (speed >= tb) {
		speed /= tb; //한글과 같은 표준에서 규정되지 않은 시스템 고유의 확장 문자 세트를 이용하려면 와이드 문자를 사용한다. 와이드 문자는 1문자의 표현이 2바이트 이상의 문자이다. 와이드 문자 stddef.h 헤더 파일에 정의된 wchar_t 형으로 표시된다.
		wprintf(L"%.2lf [TB/s]", speed); //wprintf() 함수는 전송된 와이드 문자 수를 리턴합니다. 출력 오류가 발생하면 wprintf() 함수는 음수 값을 리턴합니다.
	}
	else if (speed >= gb) {
		speed /= gb;
		wprintf(L"%.2lf [GB/s]", speed);
	}
	else if (speed >= mb) {
		speed /= mb;
		wprintf(L"%.2lf [MB/s]", speed);
	}
	else if (speed >= kb) {
		speed /= kb;
		wprintf(L"%.2lf [KB/s]", speed);
	}
	else {
		wprintf(L".2fl [B/s]", speed);
	}
}

static void Print_Bytes(unsigned long long bytes) {
	const double tb = (double)1099511627776; //2^40 TB
	const double gb = (double)1073741824; //2^30 GB
	const double mb = (double)1048576; //2^20 MB
	const double kb = (double)1024; //2^10 KB
	double dbytes = (double)bytes;

	if (dbytes >= tb) {
		dbytes /= tb;
		wprintf(L"%.2fl [TB]", dbytes);
	}
	else if (dbytes >= gb) {
		dbytes /= gb;
		wprintf(L"%.2fl [GB]", dbytes);
	}
	else if (dbytes >= mb) {
		dbytes /= mb;
		wprintf(L"%.2fl [MB]", dbytes);
	}
	else if (dbytes >= kb) {
		dbytes /= kb;
		wprintf(L"%.2fl [KB]", dbytes);
	}
	else {
		wprintf(L"%.2fl [B]", dbytes);
	}
}

static wchar_t *url2filename(const char *pUrl) {
	char *pStart;
	size_t mblen = strlen(pUrl);
	wchar_t *pFilename;
	int wclen;
	int ret;

	if (mblen < 4)
		return NULL;

	if (pUrl[mblen - 4] == '.' || pUrl[mblen - 3] == '.') {
		pStart = strrchr(pUrl, (int)'/');
		if (pStart) {
			pStart++;
			wclen = MultiByteToWideChar(
				CP_UTF8,
				MB_PRECOMPOSED,
				pStart,
				-1,
				NULL,
				0);
			if (wclen == 0)
				return NULL;
			pFilename = (wchar_t *)malloc(wclen * sizeof(wchar_t));
			if (pFilename == NULL)
				return NULL;
			ret = MultiByteToWideChar(
				CP_UTF8,
				MB_PRECOMPOSED,
				pStart,
				-1,
				pFilename,
				wclen);
			if (ret == 0) {
				free(pFilename);
				return NULL;
			}
			return pFilename;
		}
		return NULL;
	}
	else {
		return NULL;
	}
}
#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <ShObjIdl.h>
#include "dthread.h"

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

int main(int argc, char* argv[]) {
	//메인 함수가 위치한 파일입니다.
	char buf[1024]; // url 주소를 받는 버퍼
	DCTRL ctl;
	DCTRL *pCtl = &ctl;
	OLECHAR *pFilename = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	strncpy(buf, (const char*)argv, strlen((const char*)argv));
	printf("RCV LINK: %s\n", buf);
	//FAIL시 들어가야 할 코드가 위치할 곳

	while (1) {
		putws(L"If you want to exit, type c. Copy your URL link : ");
		//fgets(buf, 1024, stdin);
		if (buf[0] == '\n' || strcmp("c\n", buf, 2) == 0) { //c를 입력하거나 엔터를 입력하면 프로그램 종료
			break;
		}
		//Sanity check
		if (strcmp("https://", buf, 8) != 0 && strncmp("http://", buf, 7) != 0) {
			putws(L"ERROR : Hyperlink not valid.\n");
			continue;
		}
		Remove_New_Line(buf);

		putws(L"Getting contents size...\n");
		dctrl_get_csize(pCtl, buf, INFINITE);
		if (ctl.urlinfo.csize == 0ULL) {//컨텐츠의 size가 0인경우, 파일이 존재하지 않는 경우
			putws(L"ERROR : Server not responding.\n");
			continue;
		}
		else if (!ctl.urlinfo.bAvailable) {//웹서버에서 분할 다운로드를 막아놓은 경우
			putws(L"ERROR: SERVER does not allow partitial request.\n");
			continue;
		}

		putws(L"Content Size: ");
		Print_Bytes(ctl.urlinfo.csize);
		if (ctl.urlinfo.ctype)
			putws(L"\nContent Type: %s\n", ctl.urlinfo.ctype);
		else
			putws(L"\n");

		if (pFilename) {
			CoTaskMemFree(pFilename);
			pFilename = NULL;
		}
		if (!open_filedlg(&pFilename, &ctl.urlinfo, NULL)) { //에러메시지 출력하는 박스가 작동하지 않을 경우
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
		if (pCtl->urlinfo.csize > free_BytesAvailable.QuadPart) {
			Error_Box(L"Not enough space for download!");
			if (pBackslash)
				pBackslash[1] = temp;
			return FALSE;
		}

		putws(L"Memory left after downloading the contents : ");
		Print_Bytes(free_BytesAvailable.QuadPart - pCtl->urlinfo.csize);
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

	pDefaultName = NULL;//url2filename(pUrlinfo->url); //curl에서 넘겨받은 url 정보
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

static wchar_t *url2filename(const char *pUrl) { //파일 이름을 자동으로 생성해 주는 함수
	char *pStart;
	size_t mblen = strlen(pUrl);
	wchar_t *pFilename;
	int wclen;
	int ret;

	if (mblen < 4)
		return NULL;

	if (pUrl[mblen - 4] == '.' || pUrl[mblen] - 3 == '.') {

	}
}
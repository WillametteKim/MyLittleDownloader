#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <ShObjIdl.h>
#include "dthread.h"

static inline void putws(const wchar_t *IP_text); // fputws�� ���� ����ϱ⿡ �� �� ȿ�������� ó���ϱ� ���Ͽ� inline���� ����

extern BOOL dctrl_monitor(DCTRL *pCtl, HANDLE *phFile); //��ռӵ��� �����ӵ��� ����͸� �ϴ� �Լ�
extern BOOL check_disk_space(DCTRL *pCtl, OLECHAR *pFilename); //�ٿ���� ������ ����Ǵ� ���丮�� ���Ե� ��ũ�� ���� �뷮�� ǥ��

extern BOOL open_filedlg(OLECHAR **ppFilename, URLINFO *pUrlinfo, HANDLE *phFile); // ���� ���� �ϴ� window â ���� �Լ�
extern BOOL open_file(HANDLE *phFile, OLECHAR *pFilename); //���������� ����, ��������

static void Remove_New_Line(char *buffer); // �޾ƿ� ��ũ�� �� �������� ��ġ�� \n�� �����ϴ� �Լ�
static void Error_Box(const wchar_t * IP_Text);; //���н� ���� �޽��� ����ϴ� �Լ�
static void Print_Speed(double speed); // ��ռӵ� + �����ӵ� ����ϴ� �Լ�
static void Print_Bytes(unsigned long long bytes); // �ٿ���� ��ο��� ���� �뷮�� �˷��ִ� �Լ�

//---------------------------------------------------------------------------------------------------------------------

int main(int argc, char* argv[]) {
	//���� �Լ��� ��ġ�� �����Դϴ�.
	char buf[1024]; // url �ּҸ� �޴� ����
	DCTRL ctl;
	DCTRL *pCtl = &ctl;
	OLECHAR *pFilename = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;

	strncpy(buf, (const char*)argv, strlen((const char*)argv));
	printf("RCV LINK: %s\n", buf);
	//FAIL�� ���� �� �ڵ尡 ��ġ�� ��

	while (1) {
		putws(L"If you want to exit, type c. Copy your URL link : ");
		//fgets(buf, 1024, stdin);
		if (buf[0] == '\n' || strcmp("c\n", buf, 2) == 0) { //c�� �Է��ϰų� ���͸� �Է��ϸ� ���α׷� ����
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
		if (ctl.urlinfo.csize == 0ULL) {//�������� size�� 0�ΰ��, ������ �������� �ʴ� ���
			putws(L"ERROR : Server not responding.\n");
			continue;
		}
		else if (!ctl.urlinfo.bAvailable) {//���������� ���� �ٿ�ε带 ���Ƴ��� ���
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
		if (!open_filedlg(&pFilename, &ctl.urlinfo, NULL)) { //�����޽��� ����ϴ� �ڽ��� �۵����� ���� ���
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

extern BOOL dctrl_monitor(DCTRL *pCtl, HANDLE *phFile) { //���� ������ �뷮 ���̸� �̿��ؼ� �ӵ��� ���
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

	pBackslash = wcschr(pFilename, L'\\');//\\Ž��
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

	hr = CoCreateInstance( //�ٸ� �̸����� ���� ������ �ߴ� �� â�� ���� �͵� 
		&CLSID_FileSaveDialog,
		NULL,
		CLSCTX_INPROC_SERVER,
		&IID_IFileSaveDialog,
		&pDlg);

	if (FAILED(hr))
		return FALSE;

	pDefaultName = NULL;//url2filename(pUrlinfo->url); //curl���� �Ѱܹ��� url ����
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
	char *newline = strchr(buffer, (int)'\n'); //���ۿ� ����ִ� URL ��ũ���� ������ \n�� ��ġ�� ����

	if (newline)
		*newline = '\0'; //\n�� ����, �ùٸ� URL�� �����ϱ� ����
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
		speed /= tb; //�ѱ۰� ���� ǥ�ؿ��� �������� ���� �ý��� ������ Ȯ�� ���� ��Ʈ�� �̿��Ϸ��� ���̵� ���ڸ� ����Ѵ�. ���̵� ���ڴ� 1������ ǥ���� 2����Ʈ �̻��� �����̴�. ���̵� ���� stddef.h ��� ���Ͽ� ���ǵ� wchar_t ������ ǥ�õȴ�.
		wprintf(L"%.2lf [TB/s]", speed); //wprintf() �Լ��� ���۵� ���̵� ���� ���� �����մϴ�. ��� ������ �߻��ϸ� wprintf() �Լ��� ���� ���� �����մϴ�.
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

static wchar_t *url2filename(const char *pUrl) { //���� �̸��� �ڵ����� ������ �ִ� �Լ�
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
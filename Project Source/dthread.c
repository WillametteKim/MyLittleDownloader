//��Ƽ�����带 �����ϰ� �̸� �����ϱ� ���� �����Դϴ�.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <curl/curl.h>
#include "dcache.h"
#include "dthread.h"

#define CS_LOOP_COUNT 4000
#define USER_AGENT "Mozilla / 5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit / 537.36 (KHTML, like Gecko) Chrome / 41.0.2228.0 Safari / 537.36"

static inline void destroy_handles(HANDLE *phHandles, size_t nHandles) {
	size_t i;

	for (i = 0; i < nHandles; i++)
		CloseHandle(phHandles[i]);
	ZeroMemory(phHandles, nHandles * sizeof(HANDLE));
}

static inline BOOL create_events(HANDLE *phEvents, size_t nEvents) {
	size_t i;

	for (i = 0; i < nEvents; i++) {
		//CreateEvent()
		phEvents[i] = CreateEventW(
			NULL,
			FALSE,
			FALSE,
			NULL);//NULL �̺�Ʈ�� �������� �ʴ´ٴ� ��. Ŀ�� ��ü �̸��� ������ ȣ��� �ٸ� ���μ����� ������ ����
		if (phEvents[i] == NULL) {
			destroy_handles(phEvents, i);
			return FALSE;
		}
	}
	return TRUE;
}

static inline void destroy_thrdinfo(DTHREAD *pThrdInfo, size_t nThrd) {//������ ���̴� ��
	size_t i;

	for (i = 0; i < nThrd; i++) {
		destroy_dcache(pThrdInfo[i].cache);
	}
	ZeroMemory(pThrdInfo, nThrd * sizeof(DTHREAD));
}

static inline BOOL create_thrdinfo(DCTRL *pCtl, size_t nThreads) {//������ ����� ��
	size_t i;
	DTHREAD *pInfo;

	pCtl->pThrdInfo = (DTHREAD *)calloc(nThreads, sizeof(DTHREAD));
	for (i = 0; i < nThreads; i++) {
		pInfo = &pCtl->pThrdInfo[i];
		if (!create_dcache(&pInfo->cache)) {
			destroy_thrdinfo(pCtl->pThrdInfo, i);
			return FALSE;
		}
		pInfo->thrdno = i;
		pInfo->ctrl = pCtl;
		pInfo->urlinfo = &pCtl->urlInfo;
		pInfo->pShare = pCtl->dsh.pShare;
		pInfo->hResume = pCtl->hResume[i];
		pInfo->pShutdown = &pCtl->shutdown;
		pInfo->hWaiting[0] = pInfo->hResume;
		pInfo->hWaiting[1] = pCtl->shutdown.hEvent;
		pInfo->hDone = pCtl->hDone[i];
		pInfo->pSchedCs = &pCtl->schedCs;
	}
	return TRUE;
}

static inline void delete_cs(CRITICAL_SECTION *pCs, size_t nCs) {
	size_t i;

	for (i = 0; i < nCs; i++) {
		DeleteCriticalSection(&pCs[i]);
	}
}

static inline void destroy_dshare(DSHARE *psh) {
	curl_share_cleanup(psh->pShare);
	delete_cs(psh->cs, 3);
}

static void share_lock(CURL *pCurl, curl_lock_data data, curl_lock_access access, void *userptr) {
	CRITICAL_SECTION *pCs = (CRITICAL_SECTION *)userptr;
	(void)pCurl;//�����Ϸ��� ������ �ț��ٰ� ���׶����� �վ, ���� �־��ذ�. �̰� �ٲܼ��� ������ ������ �ʰڴ�. �ǵ������� �Ⱦ��Ŷ�� ǥ���صа�. �����Ϸ����� �ǵ��Ǿ��ٰ� �˷��ִ� ��
	(void)access;//��������

	switch (data) {//�ÿ��� ������ ǥ���� ����. DNS������ �����Ҷ� ���� �ʿ��ϸ� 1�� ���� 2������ 3������
	case CURL_LOCK_DATA_DNS:
		EnterCriticalSection(&pCs[0]);
		break;
	case CURL_LOCK_DATA_SSL_SESSION:
		EnterCriticalSection(&pCs[1]);
		break;
	case CURL_LOCK_DATA_CONNECT:
		EnterCriticalSection(&pCs[2]);
		break;
	}//����(lock)
}

static void share_unlock(CURL *pCurl, curl_lock_data data, void *userptr) {
	CRITICAL_SECTION *pCs = (CRITICAL_SECTION *)userptr;
	(void)pCurl;

	switch (data) {
	case CURL_LOCK_DATA_DNS:
		LeaveCriticalSection(&pCs[0]);
		break;
	case CURL_LOCK_DATA_SSL_SESSION:
		LeaveCriticalSection(&pCs[1]);
		break;
	case CURL_LOCK_DATA_CONNECT:
		LeaveCriticalSection(&pCs[2]);
		break;
	}//ũ��Ƽ�� ���ǿ��� ���Ӵ� . unlock
}

static inline BOOL create_dshare(DSHARE *psh) {
	size_t i;
	CURLSH *pShare = curl_share_init();

	if (pShare == NULL)
		return FALSE;
	psh->pShare = pShare;

	for (i = 0; i < 3; i++) {
		//InitializeCriticalSectionAndSpinCount()  ũ��Ƽ�ü����� Ŀ�ο������� ����� �����Ǿ����� Ȯ���ϱ� ����
		if (!InitializeCriticalSectionAndSpinCount(&psh->cs[i], CS_LOOP_COUNT)) {
			curl_share_cleanup(pShare);
			delete_cs(psh->cs, i);
			ZeroMemory(psh, sizeof(DSHARE));
			return FALSE;
		}
	}
	curl_share_setopt(pShare, CURLSHOPT_USERDATA, psh->cs);//�ɼǼ���, �����б�
	curl_share_setopt(pShare, CURLSHOPT_LOCKFUNC, share_lock);
	curl_share_setopt(pShare, CURLSHOPT_UNLOCKFUNC, share_unlock);
	curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
	curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
	curl_share_setopt(pShare, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
	return TRUE;
}

static inline void init_urlinfo(URLINFO *pInfo) {
	pInfo->bAvailable = FALSE;
	pInfo->csize = 0ULL;
	pInfo->ctype = NULL;
	pInfo->url = NULL;
}

static inline void set_shutdown(SHUTDOWN *pShutdown) {
	SetEvent(pShutdown->hEvent);
	pShutdown->val = TRUE;
}

static inline void shutdown_threads(DCTRL *pCtl, size_t nThreads) {
	if (nThreads == 0)
		return;

	set_shutdown(&pCtl->shutdown);
	WaitForMultipleObjects((DWORD)nThreads, pCtl->hThrd, TRUE, INFINITE);
}

static inline BOOL init_shutdown(DCTRL *pCtl) {
	pCtl->shutdown.hEvent = CreateEventW(
		NULL,
		TRUE,
		FALSE,
		NULL);
	if (pCtl->shutdown.hEvent == NULL)
		return FALSE;
	pCtl->shutdown.val = FALSE;
	return TRUE;
}

static inline BOOL need_shutdown(const DTHREAD *pArg) {
	return (pArg->pShutdown->val);
}

#pragma warning(disable:4996)//�׷��ϱ� ������ ��������
static inline void alloc_ctype(char **ppCtype, const char *pBuffer) {//������ �����۸�ũ ����� �Ҷ� ����� �մµ� ���⼭ ����ũ�⳪ partial ���������� �� �� �ִ�. �̸� ������ üũ�� �� �ֵ��� �����ִ� ��
	const char *ptr = pBuffer;
	size_t nBytes;

	while (!isspace((int)*ptr) && *ptr != ';') {
		ptr++;
	}
	nBytes = ptr - pBuffer;
	*ppCtype = (char *)malloc((nBytes + 1) * sizeof(char));
	if (*ppCtype) {
		strncpy(*ppCtype, pBuffer, nBytes);//��� ���� �����÷ο츦 üũ���� �����Ƿ� ������������ϱ� ���ȹ����� ����. ����ũ�μ���Ʈ������ �ڿ� _s�� ���� �Լ��� ���� ����� ���α׷��� �ٷ� ����. �ٵ� �׷��� �����Ƽ�, ��Ŀ�� ������ �� ��� ���� �������� ���� �ʴ� �̻� �� �ڿ������� �帧������ ������� �����Ƿ�.
		ppCtype[0][nBytes] = '\0';//(*ppCtype)[nBytes]�̰Ŷ� ���� ��
	}
}
#pragma warning(default:4996)//���⼭���ʹ� �ٽ� ����� ���� ���޶�

static inline void header_evaluate(URLINFO *pInfo, const char *pBuffer, size_t nBytes) {
	// Get the content size.
	if (pInfo->csize == 0ULL && nBytes > 16 && strncmp("Content-Length", pBuffer, 14) == 0) {
		pInfo->csize = strtoull(&pBuffer[16], NULL, 10);
		return;
	}

	// Get the content type.
	if (nBytes > 14 && strncmp("Content-Type", pBuffer, 12) == 0) {
		alloc_ctype(&pInfo->ctype, &pBuffer[14]);
		return;
	}

	// Check if partial request is available. partial request�� �����̾�ȸ���� �����ϵ��� ���������� �����ްų� ���� �ٿ�ε带 ����Ұ����� ���������� ������ �ϴµ� �̸� �˷��ִ� ��. �߰����� ���� ���� �ִٸ� �˷��ִ� ��
	if (nBytes > 15 && strncmp("Accept-Ranges", pBuffer, 13) == 0) {
		pInfo->bAvailable = (strncmp("bytes", &pBuffer[15], 5) == 0);
		return;
	}
}

static size_t header_callback(char *buffer, size_t size, size_t nmemb, void *userdata) {
	size_t nBytes = size * nmemb;

	header_evaluate((URLINFO *)userdata, buffer, nBytes);
	return nBytes;
}

static size_t dummy_write_callback(char *buffer, size_t size, size_t nmemb, void *userdata) {//����� ���� ��ũ�� ���� �ʿ䰡 �����Ƿ� �̸� ������ ����. ���̷� �̷��� ���� �����ٴ�
	DTHREAD *pInfo = (DTHREAD *)userdata;
	(void)buffer;

	return (need_shutdown(pInfo)) ? 0 : size * nmemb;
}

static BOOL dthread_insert_dcache(DTHREAD *pInfo, const char *buffer, size_t nBytes) {//�����忡 �մ� ĳ�ÿ� �����͸� �ִ� ��
	if (pInfo->fpEnd <= pInfo->fpNow.QuadPart)
		return FALSE;
	if (!dcache_insert(pInfo->cache, buffer, nBytes, pInfo->fpNow))
		return FALSE;
	pInfo->fpNow.QuadPart += (long long)nBytes;//���� �����͸� �����ϴ� ��
	pInfo->dlBytes += (unsigned long long)nBytes;//���ݱ��� �ٿ�ε��� �������� ����Ʈ���� ũ��
	return TRUE;
}

static size_t write_callback(char *buffer, size_t size, size_t nmemb, void *userdata) {
	DTHREAD *pInfo = (DTHREAD *)userdata;
	size_t nBytes = size * nmemb;

	if (need_shutdown(pInfo))
		return 0;
	if (!dthread_insert_dcache(pInfo, buffer, nBytes))
		return 0;
	return nBytes;//ĳ�ÿ��ٰ� �ÿ��� ���� �����͸� �ִ� ��. �ٷ� �� �Լ����� ����ϴ� ��
}

static void curl_my_perform(CURL *pCurl, DTHREAD *pArg) {//�ÿ��� �����͸� �ޱ� �������ּ��䰡 ���������ε�, ���۸� ���� ��
	pArg->rc = curl_easy_perform(pCurl);
}

static void curl_my_perform2(CURLM *pMulti, CURL *pCurl, DTHREAD *pArg) {//���� �Ⱦ��� ��
	int still_running = 1;
	//������ ���ϰ� �� ���߿� ó���Ұ� ������ �̰� ����϶�� ����� �� �� ���� curlm�� �ʿ��ϴ�.
	do {
		struct timeval timeout = {
			.tv_sec = 0L,
			.tv_usec = 100000L
		};
		int rc;  // select() return code
		CURLMcode mc;  // curl_multi_fdset() return code
		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;
		int maxfd = -1;

		if (need_shutdown(pArg))
			return;

		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		mc = curl_multi_fdset(pMulti, &fdread, &fdwrite, &fdexcep, &maxfd);
		if (mc != CURLM_OK)
			return;

		/*
		* On success the value of maxfd is guaranteed to be >= -1. We call
		* select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
		* no fds ready yet so we call Sleep(100), which is the minimum suggested
		* value in the curl_multi_fdset() doc.
		*/
		if (maxfd == -1) {
			Sleep(100);
			rc = 0;
		}
		else {
			rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
		}
		if (need_shutdown(pArg))
			return;
		switch (rc) {
		case SOCKET_ERROR:
			return;
		default:
			curl_multi_perform(pMulti, &still_running);
			break;
		}
	} while (still_running);
}

static unsigned download_proc_commit(DTHREAD *pArg, CURL *pCurl) {
	if (pArg->rc != CURLE_OK)
		set_shutdown(pArg->pShutdown);//������ �ݰ�
	if (pCurl)
		curl_easy_cleanup(pCurl);
	return 0;
}//������ 64���� ���������� �����Ҷ� �ϴ� ��

static inline void adjust_range(DTHREAD *pArg) {//ó���� ������ 1����Ʈ ���̸� ������ �߻��ϹǷ�, 2����Ʈ�� ������ִ� ��
	const long long diff = pArg->fpEnd - pArg->fpNow.QuadPart;

	if (diff == 1LL) {
		if (pArg->fpNow.QuadPart == 0LL)
			(pArg->fpEnd)++;
		else
			(pArg->fpNow.QuadPart)--;
	}
}

#pragma warning(disable:4996)
static inline void print_range(DTHREAD *pArg) {//accept range�� �������� ����� �����´� �� ���ڿ��� ����� ��. ������ ���� ��
	_snprintf(pArg->range, 42, "%lld-%lld", pArg->fpNow.QuadPart, pArg->fpEnd - 1);
}
#pragma warning(default:4996)

static inline void set_range(DTHREAD *pArg, long long start, long long end) {//���������� �� �����尡 ���� �������� ����� ���۰� ���� ���ϴ� ��
	pArg->fpNow.QuadPart = start;
	pArg->fpEnd = end;
	if (start == 0LL && end == 0LL)//���۰� ���� �Ѵ� 0�϶��� ��ü�� �� �ް� ���� ��. ����� �ҷ��ö� ���⼭ �ɸ�
		pArg->range[0] = '\0';
	else
		print_range(pArg);
}

static void reschedule(CURL *pCurl, DTHREAD *pArg) {//�����尡 �ڱ� ���� �� ���� �� ���� �����ִ� ��
	size_t i;
	const DCTRL *pCtl = pArg->ctrl;
	DTHREAD *pSelected;
	DTHREAD *pThis;
	long long bytesLeftMax;
	long long bytesLeft;
	double avgSpeed;

	while (!need_shutdown(pArg)) {
		while (pArg->fpNow.QuadPart < pArg->fpEnd) {
			if (need_shutdown(pArg))
				return;
			adjust_range(pArg);
			print_range(pArg);
			if (curl_easy_setopt(pCurl, CURLOPT_RANGE, pArg->range) != CURLE_OK) {
				set_shutdown(pArg->pShutdown);
				return;
			}
			curl_my_perform(pCurl, pArg);
		}
		if (need_shutdown(pArg))
			return;

		EnterCriticalSection(pArg->pSchedCs);
		pSelected = NULL;
		bytesLeftMax = 0LL;
		for (i = 0; i < pCtl->nThrd; i++) {
			pThis = &pCtl->pThrdInfo[i];
			if (pThis == pArg || pThis->bSleeping)
				continue;
			bytesLeft = pThis->fpEnd - pThis->fpNow.QuadPart;
			if (bytesLeft > bytesLeftMax) {
				pSelected = pThis;
				bytesLeftMax = bytesLeft;
			}
		}
		// No more rescheduling required.
		if (bytesLeftMax <= 2048) {
			goto leave;
		}
		if (pSelected->fpEnd > pSelected->fpNow.QuadPart) {
			bytesLeft = pSelected->fpEnd - pSelected->fpNow.QuadPart;
			avgSpeed = (double)(pSelected->dlBytes * CLOCKS_PER_SEC) / (double)(GetTickCount() - pSelected->tickStart);
			if ((double)bytesLeft > avgSpeed) {
				set_range(pArg, pSelected->fpNow.QuadPart + bytesLeft / 2, pSelected->fpEnd);
				if (curl_easy_setopt(pCurl, CURLOPT_RANGE, pArg->range) != CURLE_OK) {
					set_range(pArg, 0LL, 0LL);
					goto leave;
				}
				pSelected->fpEnd = pArg->fpNow.QuadPart;
				LeaveCriticalSection(pArg->pSchedCs);
				wprintf(L"[Thread %zd] interrupts Thread %zd (%lld bytes)\n", pArg->thrdno, pSelected->thrdno, bytesLeft);
				curl_my_perform(pCurl, pArg);
			}
			else {
				goto leave;
			}
		}
		else {
			goto leave;
		}
	}
leave:
	LeaveCriticalSection(pArg->pSchedCs);
	return;
}

static BOOL dthread_set_url(CURL *pCurl, DTHREAD *pArg) {
	URLINFO *pUrlInfo = pArg->urlinfo;
	char *pDecoded;

	// Sometimes intentionally set the url to NULL.
	if (pUrlInfo->url == NULL)
		return TRUE;

	pDecoded = curl_easy_unescape(pCurl, pUrlInfo->url, 0, NULL);
	if (pDecoded == NULL) {
		pUrlInfo->url = NULL;
		return FALSE;
	}
	pUrlInfo->url = pDecoded;
	return (curl_easy_setopt(pCurl, CURLOPT_URL, pDecoded) == CURLE_OK);
}

static unsigned __stdcall dthread_proc(void *_pArg) {//������ ������� �� ���۵Ǵ� ��? 
	DTHREAD *pArg = (DTHREAD *)_pArg;
	CURL *pCurl;

	pCurl = curl_easy_init();
	if (pCurl == NULL)
		goto failed_init;

#define SETOPT_HEAPCHECK(pCurl, option, val) \
			do { if (curl_easy_setopt(pCurl, option, val) != CURLE_OK) { \
				set_shutdown(pArg->pShutdown); \
				goto no_memory; } \
			} while (0)

	curl_easy_setopt(pCurl, CURLOPT_SHARE, pArg->pShare);
	SETOPT_HEAPCHECK(pCurl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(pCurl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(pCurl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(pCurl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(pCurl, CURLOPT_WRITEDATA, pArg);
	curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(pCurl, CURLOPT_FOLLOWLOCATION, 1L);

	for (; !need_shutdown(pArg); SetEvent(pArg->hDone)) {
		pArg->mode = THRDMODE_PERFORM;
		pArg->bSleeping = TRUE;
		WaitForMultipleObjects(2, pArg->hWaiting, FALSE, INFINITE);
		if (need_shutdown(pArg))
			goto done;
		pArg->bSleeping = FALSE;
		switch (pArg->mode) {
		case THRDMODE_GET_SIZE:
			if (!dthread_set_url(pCurl, pArg))
				goto no_memory;
			curl_easy_setopt(pCurl, CURLOPT_RANGE, NULL);
			pArg->urlinfo->csize = 0ULL;
			curl_easy_setopt(pCurl, CURLOPT_HEADER, 1L);
			curl_easy_setopt(pCurl, CURLOPT_NOBODY, 1L);
			curl_easy_setopt(pCurl, CURLOPT_HEADERDATA, pArg->urlinfo);
			curl_easy_setopt(pCurl, CURLOPT_HEADERFUNCTION, header_callback);
			curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, dummy_write_callback);
			curl_my_perform(pCurl, pArg);
			if (pArg->urlinfo->csize != 0ULL)
				pArg->rc = CURLE_OK;
			curl_easy_setopt(pCurl, CURLOPT_HEADER, 0L);
			curl_easy_setopt(pCurl, CURLOPT_NOBODY, 0L);
			curl_easy_setopt(pCurl, CURLOPT_HEADERDATA, NULL);
			curl_easy_setopt(pCurl, CURLOPT_HEADERFUNCTION, NULL);
			curl_easy_setopt(pCurl, CURLOPT_WRITEFUNCTION, write_callback);
			break;
		case THRDMODE_CONNECT_ONLY:
			SETOPT_HEAPCHECK(pCurl, CURLOPT_URL, pArg->urlinfo->url);
			curl_easy_setopt(pCurl, CURLOPT_CONNECT_ONLY, 1L);
			curl_my_perform(pCurl, pArg);
			curl_easy_setopt(pCurl, CURLOPT_CONNECT_ONLY, 0L);
			break;
		case THRDMODE_UPDATE_URL:
			SETOPT_HEAPCHECK(pCurl, CURLOPT_URL, pArg->urlinfo->url);
		default:
			if (pArg->range[0] != '\0')
				SETOPT_HEAPCHECK(pCurl, CURLOPT_RANGE, pArg->range);
			else
				curl_easy_setopt(pCurl, CURLOPT_RANGE, NULL);
			pArg->tickStart = GetTickCount();
			curl_my_perform(pCurl, pArg);
			if (need_shutdown(pArg))
				goto done;
			reschedule(pCurl, pArg);
			break;
		}
	}
#undef SETOPT_HEAPCHECK
	done :
		 pArg->rc = CURLE_OK;//rc�� return code
		 return download_proc_commit(pArg, pCurl);
	 failed_init:
		 pArg->rc = CURLE_FAILED_INIT;
		 return download_proc_commit(pArg, pCurl);
	 no_memory:
		 pArg->rc = CURLE_OUT_OF_MEMORY;
		 return download_proc_commit(pArg, pCurl);
}

static inline void destroy_shutdown(SHUTDOWN *pShutdown) {
	CloseHandle(pShutdown->hEvent);
}

static inline BOOL start_threads(DCTRL *pCtl, size_t nThreads) {
	size_t i;
	//CreateThread�� �ձ�� �ϴµ� ����ũ�ν���Ʈ���� �϶�� �س��� �ǵ� crt ��Ÿ�ӿ��� ���𰡸� �ʱ�ȭ������ϴ� �Լ���(��Ʈ����ũ����������) , ��Ÿ�ӿ��� �޸𸮸� �ǵ帮�� �Լ����� �̰� ����ϸ� �޸� ������ �߻���
	for (i = 0; i < nThreads; i++) {
		//_beginthreadex()
		pCtl->hThrd[i] = (HANDLE)_beginthreadex(//thread�� ����� ��, �����ϸ� 
			NULL,
			0,
			dthread_proc,
			&pCtl->pThrdInfo[i],
			0,
			NULL);
		if (pCtl->hThrd[i] == (HANDLE)0) {
			shutdown_threads(pCtl, i);
			return FALSE;
		}
	}
	return TRUE;
}

static void init_cparam(DCTRL *pCtl) {//ĳ���� �Լ��߿� nFile�� �䱸�ϴ� ��찡 �մ�. ��ũ�� ���� ���� �ڵ��� �䱸�ϴ� ��. �����ڵ��� ĳ�ÿ��� ������ ���ϹǷ� �Ķ���Ϳ� �����صδ� ��
	pCtl->cp.pCtl = pCtl;
	pCtl->cp.hCache = NULL;
	pCtl->cp.bCache = FALSE;
	pCtl->cp.hFile = INVALID_HANDLE_VALUE;
	pCtl->cp.pShutdown = &pCtl->shutdown;
}

static BOOL create_sched_cs(DCTRL *pCtl) {
	return InitializeCriticalSectionAndSpinCount(&pCtl->schedCs, CS_LOOP_COUNT);
}

static void delete_sched_cs(DCTRL *pCtl) {
	DeleteCriticalSection(&pCtl->schedCs);
}

extern BOOL create_dctrl(DCTRL *pCtl, size_t nThreads) {//n���� �����带 ��Ʈ���ϴ� ��Ʈ�� ��ü�� ����� ������ ����� ��
	pCtl->nThrd = nThreads;
	pCtl->hThrd = (HANDLE *)malloc(3 * nThreads * sizeof(HANDLE));
	if (pCtl->hThrd == NULL)
		return FALSE;

	pCtl->hResume = pCtl->hThrd + nThreads;
	pCtl->hDone = pCtl->hResume + nThreads;
	if (!create_events(pCtl->hResume, 2 * nThreads))
		goto free_handles;

	if (!init_shutdown(pCtl))
		goto free_events;

	if (!create_dshare(&pCtl->dsh))
		goto free_shutdown;

	if (!create_thrdinfo(pCtl, nThreads))
		goto free_dshare;

	init_urlinfo(&pCtl->urlInfo);
	init_cparam(pCtl);

	if (!create_sched_cs(pCtl))
		goto free_thrdinfo;

	if (!start_threads(pCtl, nThreads))
		goto free_schedcs;

	return TRUE;
free_schedcs:
	delete_sched_cs(pCtl);
free_thrdinfo:
	destroy_thrdinfo(pCtl->pThrdInfo, nThreads);
free_dshare:
	destroy_dshare(&pCtl->dsh);
free_shutdown:
	destroy_shutdown(&pCtl->shutdown);
free_events:
	destroy_handles(pCtl->hResume, nThreads);
	destroy_handles(pCtl->hDone, nThreads);
free_handles:
	free(pCtl->hThrd);
	return FALSE;
}

extern void destroy_dctrl(DCTRL *pCtl) {
	const size_t nThreads = pCtl->nThrd;

	if (pCtl->urlInfo.ctype)
		free(pCtl->urlInfo.ctype);
	shutdown_threads(pCtl, nThreads);
	dctrl_commit(pCtl);
	delete_sched_cs(pCtl);
	destroy_thrdinfo(pCtl->pThrdInfo, nThreads);
	destroy_dshare(&pCtl->dsh);
	destroy_shutdown(&pCtl->shutdown);
	destroy_handles(pCtl->hResume, nThreads);
	destroy_handles(pCtl->hDone, nThreads);
	free(pCtl->hThrd);
	ZeroMemory(pCtl, sizeof(DCTRL));
}

#pragma warning(disable:4090)
extern DWORD dctrl_get_csize(DCTRL *pCtl, const char *pUrl, DWORD dwTimeoutMilliseconds) {//���� ������ ����� �޾ƿ��°�. ��Ʈ�ѷ��� �޾ƿ��� ��
	DTHREAD *pInfo = &pCtl->pThrdInfo[0];
	URLINFO *pUrlInfo = &pCtl->urlInfo;

	if (pUrl == NULL)
		return WAIT_FAILED;
	if (pUrlInfo->url)
		curl_free(pUrlInfo->url);
	pUrlInfo->url = pUrl;
	pUrlInfo->bAvailable = FALSE;
	pUrlInfo->csize = 0ULL;
	if (pUrlInfo->ctype) {
		free(pUrlInfo->ctype);
		pUrlInfo->ctype = NULL;
	}
	pInfo->mode = THRDMODE_GET_SIZE;
	SetEvent(pInfo->hResume);
	return WaitForSingleObject(pInfo->hDone, dwTimeoutMilliseconds);
}
#pragma warning(default:4090)

static unsigned __stdcall cache_proc(void *_cp) {//fluch�� �ϳ��� �������ε� �� flush �����带 �����ϴ� �ּ�
	CACHEPARAM *cp = (CACHEPARAM *)_cp;

	while (!cp->pShutdown->val && cp->bCache) {//flush�� ���̰� ������ bCache�� false�� ����� ��
		if (!dctrl_cache_flush(cp->pCtl, cp->hFile, INFINITE)) {
			set_shutdown(cp->pShutdown);
			return 0;
		}
		Sleep(10);//�Ƚ��� CPU�� ��ģ���� ���ư�
	}
	return 0;
}

extern void dctrl_commit(DCTRL *pCtl) {//��ü������ �� �޾����� Ŀ��
	if (pCtl->cp.bCache) {
		pCtl->cp.bCache = FALSE;//flush �����带 ����
		WaitForSingleObject(pCtl->cp.hCache, INFINITE);
		CloseHandle(pCtl->cp.hCache);
		pCtl->cp.hCache = NULL;
	}
}

static BOOL cparam_create_thread(DCTRL *pCtl, HANDLE hFile) {//flush �����带 ����� ��
	pCtl->cp.bCache = TRUE;
	pCtl->cp.hFile = hFile;
	pCtl->cp.hCache = (HANDLE)_beginthreadex(NULL, 0, cache_proc, &pCtl->cp, 0, NULL);
	return (pCtl->cp.hCache != NULL);
}

extern BOOL dctrl_start(DCTRL *pCtl, HANDLE hFile) {//������ũ�⵵ �ƴϱ� �̸� ������ ������ŭ �ɰ��� �۾��� �����ϴ� ��
	URLINFO *pUrlInfo = &pCtl->urlInfo;
	DTHREAD *pThis;
	DTHREAD *pPrev = 0;//������ =0 ������ �̰� ������ �ʱ�ȭ ���� ���� ������ ���� ���� �־��
	size_t i;
	const unsigned long long denominator = (unsigned long long)pCtl->nThrd;
	const unsigned long long quotient = pUrlInfo->csize / denominator;
	const unsigned long long remainder = pUrlInfo->csize % denominator;

	for (i = 0; i < pCtl->nThrd; i++) {
		if (!pCtl->pThrdInfo[i].bSleeping)
			return FALSE;
	}
	if (!cparam_create_thread(pCtl, hFile))
		return FALSE;
	for (i = 0; i < pCtl->nThrd; i++, pPrev = pThis) {
		pThis = &pCtl->pThrdInfo[i];
		if (i != 0) {
			set_range(pThis, pPrev->fpEnd, pPrev->fpEnd + (long long)quotient);
			pThis->mode = THRDMODE_UPDATE_URL;
		}
		else {
			set_range(pThis, 0LL, (long long)(quotient + remainder));
			pThis->mode = THRDMODE_PERFORM;
		}
		pThis->rc = CURLE_OK;
		pThis->dlBytes = 0ULL;
		SetEvent(pThis->hResume);
	}
	return TRUE;
}

extern BOOL dctrl_all_sleeping(DCTRL *pCtl) {//�����尡 �� ���� �ִ��� Ȯ�� �ϴ� ��
	size_t nSleepingThrds = 0;
	size_t i;

	for (i = 0; i < pCtl->nThrd; i++)
		nSleepingThrds += (pCtl->pThrdInfo[i].bSleeping == TRUE);
	return (nSleepingThrds == pCtl->nThrd);
}

extern DWORD dctrl_wait(DCTRL *pCtl, BOOL bWaitAll, DWORD dwMilliseconds) {
	return WaitForMultipleObjects((DWORD)pCtl->nThrd, pCtl->hDone, bWaitAll, dwMilliseconds);
}//��ü �������� �۾��� ��������� �� ��ٸ��� ��(bWaitALL�� true), �ϳ��� ���� wait�� ������. (bWaitAll�� false) 

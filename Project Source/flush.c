//버퍼에 다운받아진 데이터를 disk로 flush하는 역할을 하는 함수입니다.

#include <Windows.h>
#include "dcache.h"
#include "dthread.h"

static inline BOOL flush_proc(DCTRL *pCtl, HANDLE hFile, BOOL *pbLoop) {
	DCACHE *pThis;
	size_t i;

	for (i = 0; i < pCtl->nThrd; i++) {
		pThis = pCtl->pThrdInfo[i].cache;
		if (!dcache_empty(pThis)) {
			if (!dcache_flush(pThis, hFile)) {
				return FALSE;
			}
			*pbLoop = TRUE;
		}
	}
	return TRUE;
}

extern BOOL dctrl_cache_flush(DCTRL *pCtl, HANDLE hFile, DWORD dwMilliseconds) {
	DWORD dwTickStart;
	DWORD dwTickNow;
	BOOL bLoop;

	if (dwMilliseconds == INFINITE) {
		do {
			bLoop = FALSE;
			if (!flush_proc(pCtl, hFile, &bLoop))
				return FALSE;
		} while (bLoop);
	}
	else {
		dwTickStart = GetTickCount();
		while (1) {
			bLoop = FALSE;
			if (!flush_proc(pCtl, hFile, &bLoop))
				return FALSE;
			dwTickNow = GetTickCount();
			if ((dwTickNow - dwTickStart) < dwMilliseconds)
				return TRUE;
			if (bLoop)
				continue;
			Sleep(10);
		}
	}
	return TRUE;
}
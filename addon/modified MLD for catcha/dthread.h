//dthread의 헤더파일입니다.

#ifndef _DTHREAD_H_
#define _DTHREAD_H_

#if _MSC_VER > 1000
#	pragma once
#endif //MSC_VER

#include <Windows.h>
#include <curl/curl.h>
#include "dcache.h"

typedef struct {
	char *url; //URL 링크
	char *ctype; // Content type
	unsigned long long csize; //Content Size
	BOOL bAvailable; //웹 서버가 partial request를 허용하는지 하지 않는지
}URLINFO;

typedef struct { //스레드 종료를 위한 이벤트를 전해주기 위한 구조체
	HANDLE hEvent;
	BOOL val;
}SHUTDOWN;

typedef struct { //스레드끼리 서로 공유해야할 정보를 위한 구조체. 예를 들면 IP 주소
	CURLSH *pShare;
	CRITICAL_SECTION cs[3];
}DSHARE;

typedef enum {
	THRDMODE_PERFORM = 0,
	THRDMODE_GET_SIZE,
	THRDMODE_CONNECT_ONLY,
	THRDMODE_UPDATE_URL
}THRDMODE;

typedef struct _dctrl DCTRL;

typedef struct {
	HANDLE hCache;
	BOOL bCache;
	HANDLE hFile;
	SHUTDOWN *pShutdown;
	struct _dctrl *pCtl;
}CACHEPARAM;

typedef struct {
	size_t thrdno; // 리스케쥴링을 위한 변수
	DCTRL *ctrl;

	URLINFO *urlinfo;
	CURLSH *pShare;
	CURLcode rc;
	THRDMODE mode;

	char range[42];
	DCACHE *cache;
	LARGE_INTEGER fpNow;
	long long fpEnd;
	DWORD tickStart;
	unsigned long long dlBytes;

	HANDLE hResume;
	SHUTDOWN *pShutdown;
	HANDLE hWaitng[2];
	HANDLE hDone;
	BOOL bSleeping;

	CRITICAL_SECTION *pShedCs;
}DTHREAD;

struct _dctrl {
	DTHREAD *pThrdInfo; // 스레드의 info
	HANDLE *hThrd; // 스레드 커널 객체 핸들
	size_t nThrd; //스레드 info array의 크기
	HANDLE *hResume; // 링크 주소를 입력했을 때 스레드에게 일을 시작하라고 이벤트를 보내기 위함. event가 일어날 때까지 스레드는 sleep
	HANDLE *hDone; // 스레드가 처음 받은 분량의 파일을 다운로드를 완료하면 이벤트로 이를 알려주기 위함.
	URLINFO urlinfo; // 다운받을 주소의 정보

	SHUTDOWN shutdown; // 스레드를 강제 종료. shutdown할때 필요한 정보를 가지고 있음.

	DSHARE dsh; //컬 공유 자원. 컬 마다 서로 다른 IP로 접속을 하려고 하는 등에 필요한 정보를 하나로 통일 시키기 위한 자원을 DSHARE로 구현
	CACHEPARAM cp; //cache parameter
	CRITICAL_SECTION schedCs; //다운로드를 일찍 마친 스레드가 다른 스레드를 도와주기 위한 크리티컬 섹션
};
#endif //_DTHREAD_H_
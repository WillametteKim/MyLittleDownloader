//dthread�� ��������Դϴ�.

#ifndef _DTHREAD_H_
#define _DTHREAD_H_

#if _MSC_VER > 1000
#	pragma once
#endif //MSC_VER

#include <Windows.h>
#include <curl/curl.h>
#include "dcache.h"

typedef struct {
	char *url; //URL ��ũ
	char *ctype; // Content type
	unsigned long long csize; //Content Size
	BOOL bAvailable; //�� ������ partial request�� ����ϴ��� ���� �ʴ���
}URLINFO;

typedef struct { //������ ���Ḧ ���� �̺�Ʈ�� �����ֱ� ���� ����ü
	HANDLE hEvent;
	BOOL val;
}SHUTDOWN;

typedef struct { //�����峢�� ���� �����ؾ��� ������ ���� ����ü. ���� ��� IP �ּ�
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
	size_t thrdno; // �������층�� ���� ����
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
	DTHREAD *pThrdInfo; // �������� info
	HANDLE *hThrd; // ������ Ŀ�� ��ü �ڵ�
	size_t nThrd; //������ info array�� ũ��
	HANDLE *hResume; // ��ũ �ּҸ� �Է����� �� �����忡�� ���� �����϶�� �̺�Ʈ�� ������ ����. event�� �Ͼ ������ ������� sleep
	HANDLE *hDone; // �����尡 ó�� ���� �з��� ������ �ٿ�ε带 �Ϸ��ϸ� �̺�Ʈ�� �̸� �˷��ֱ� ����.
	URLINFO urlinfo; // �ٿ���� �ּ��� ����

	SHUTDOWN shutdown; // �����带 ���� ����. shutdown�Ҷ� �ʿ��� ������ ������ ����.

	DSHARE dsh; //�� ���� �ڿ�. �� ���� ���� �ٸ� IP�� ������ �Ϸ��� �ϴ� � �ʿ��� ������ �ϳ��� ���� ��Ű�� ���� �ڿ��� DSHARE�� ����
	CACHEPARAM cp; //cache parameter
	CRITICAL_SECTION schedCs; //�ٿ�ε带 ���� ��ģ �����尡 �ٸ� �����带 �����ֱ� ���� ũ��Ƽ�� ����
};
#endif //_DTHREAD_H_
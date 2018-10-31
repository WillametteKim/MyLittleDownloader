//dcache의 헤더파일입니다.
#ifndef _DCACHE_H_
#define _DCACHE_H_

#if _MSC_VER > 1000
#	pragma once
#endif // MSC VER

#include <Windows.h>

typedef struct _dnode {
	LARGE_INTEGER fp;
	char *front;
	size_t size;
	struct _dnode *next;
}DNODE;

typedef struct {
	char *data;
	size_t size;
	LARGE_INTEGER fp;
}DBUFINFO; //다운로드할 데이터가 어디서부터 시작하는지, 청크는 얼마인지, 파일포인터는 어디인지를 저장하는 구조체

typedef struct {
	char *front; //DBUFFER의 시작지점
	char *bp; //DBUFFER의 마지막 지점
	size_t memsize; //전체 chunk buffer size
	DBUFINFO *info;
	unsigned int infosize;
	unsigned int index;
}DBUFFER;

typedef struct {
	HANDLE hHeap; //핸들을 커널 객체로 관리
	CRITICAL_SECTION bufcs; //뮤텍스
	CRITICAL_SECTION listcs;
	DBUFFER *front;
	DBUFFER *back;
	DNODE *head;
	DNODE *tail;
}DCACHE; //캐시의 구조 -> 캐시 내에 두개의 DBUFFER가 존재하여 두개를 switch해가며 다운로드와 플러시를 진행한다
		 //DNODE는 링크드 리스트 구조를 이루며 DBUFFER 2개가 모두 사용될 때를 위한 비상용이다.

extern BOOL create_dcache(DCACHE **ppCache);
extern void destroy_dcache(DCACHE *pCache);
extern BOOL dcache_insert(DCACHE *pCache, const char *IpBuffer, size_t nBytes, LARGE_INTEGER fp);//데이터를 어느 캐시에 넣을 것인지, CURL이 준 데이터의 시작 주소, 데이터 버퍼의 크기, 파일 포인터
extern BOOL dcache_flush(DCACHE *pCache, HANDLE hFile); // call flush thread
extern BOOL dcache_empty(DCACHE *pCache); // 캐시 contents 삭제

#endif
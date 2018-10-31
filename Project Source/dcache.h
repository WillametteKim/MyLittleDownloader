//dcache�� ��������Դϴ�.
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
}DBUFINFO; //�ٿ�ε��� �����Ͱ� ��𼭺��� �����ϴ���, ûũ�� ������, ���������ʹ� ��������� �����ϴ� ����ü

typedef struct {
	char *front; //DBUFFER�� ��������
	char *bp; //DBUFFER�� ������ ����
	size_t memsize; //��ü chunk buffer size
	DBUFINFO *info;
	unsigned int infosize;
	unsigned int index;
}DBUFFER;

typedef struct {
	HANDLE hHeap; //�ڵ��� Ŀ�� ��ü�� ����
	CRITICAL_SECTION bufcs; //���ؽ�
	CRITICAL_SECTION listcs;
	DBUFFER *front;
	DBUFFER *back;
	DNODE *head;
	DNODE *tail;
}DCACHE; //ĳ���� ���� -> ĳ�� ���� �ΰ��� DBUFFER�� �����Ͽ� �ΰ��� switch�ذ��� �ٿ�ε�� �÷��ø� �����Ѵ�
		 //DNODE�� ��ũ�� ����Ʈ ������ �̷�� DBUFFER 2���� ��� ���� ���� ���� �����̴�.

extern BOOL create_dcache(DCACHE **ppCache);
extern void destroy_dcache(DCACHE *pCache);
extern BOOL dcache_insert(DCACHE *pCache, const char *IpBuffer, size_t nBytes, LARGE_INTEGER fp);//�����͸� ��� ĳ�ÿ� ���� ������, CURL�� �� �������� ���� �ּ�, ������ ������ ũ��, ���� ������
extern BOOL dcache_flush(DCACHE *pCache, HANDLE hFile); // call flush thread
extern BOOL dcache_empty(DCACHE *pCache); // ĳ�� contents ����

#endif
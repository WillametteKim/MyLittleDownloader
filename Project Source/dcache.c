//������ ������ ������ curl�κ��� ������ �ٿ�޴� ������ �ϴ� �����Դϴ�.
#ifdef _DEBUG
#include <stdio.h>
#endif _DEBUG
#include <stdlib.h>
#include <Windows.h>
#include "dcache.h"

#define CS_LOOP_COUNT 4000

static inline BOOL init_heap(DCACHE *pCache) { //thread�ȿ� �ִ� cache�� �޸𸮿� �ø��� ���� �ʿ��� heap ������ Ȯ��
	DWORD dwEnableFH = 2; // ������ �޸𸮿� �Ҵ��ϴ� ���� Heapalloc()���� ����
	//default 1MB
	pCache->hHeap = HeapCreate(0, 0x100000, 0);
	if (pCache->hHeap != NULL) {
		HeapSetInformation(
			pCache->hHeap,
			HeapCompatibilityInformation,
			&dwEnableFH,
			sizeof(DWORD));
		return TRUE;
	}
	return FALSE;
}

static inline void destroy_heap(DCACHE *pCache) {
	if (pCache->hHeap) {
		HeapDestroy(pCache->hHeap);
		pCache->hHeap = NULL;
	}
}

static inline BOOL create_dbuffer(DCACHE *pCache) { //cache���� ��ġ�� �������� curl���κ��� �޾ƿ� �����͸� ���������� �����ϴ� ����
	const size_t memsize = 0x400000;  // 4MB ���۴� �� 256MB�� ����ϹǷ�
	const unsigned int infosize = 2048;//2048�̸� info�� ���ڶ� ������ ��ĥ���� ����

	pCache->front = (DBUFFER *)HeapAlloc(pCache->hHeap, 0, sizeof(DBUFFER));
	if (pCache->front == NULL)
		return FALSE;
	pCache->back = (DBUFFER *)HeapAlloc(pCache->hHeap, 0, sizeof(DBUFFER));
	if (pCache->back == NULL)
		goto free_front;

	pCache->front->front = (char *)HeapAlloc(pCache->hHeap, 0, memsize);
	if (pCache->front->front == NULL)
		goto free_back;
	pCache->back->front = (char *)HeapAlloc(pCache->hHeap, 0, memsize);
	if (pCache->back->front == NULL)
		goto free_front_front;

	pCache->front->info = (DBUFINFO *)HeapAlloc(pCache->hHeap, 0, infosize * sizeof(DBUFINFO));
	if (pCache->front->info == NULL)
		goto free_back_front;
	pCache->back->info = (DBUFINFO *)HeapAlloc(pCache->hHeap, 0, infosize * sizeof(DBUFINFO));
	if (pCache->back->info == NULL)
		goto free_front_info;
	// init parameters
	pCache->front->bp = pCache->front->front;
	pCache->front->index = 0; //info[index]�� ���ο� ������ ������ �ϴ� ��. ���� ���� INFO�� �̰��� ����.
	pCache->front->infosize = infosize;
	pCache->front->memsize = memsize;
	pCache->back->bp = pCache->back->front;
	pCache->back->index = 0;
	pCache->back->infosize = infosize;
	pCache->back->memsize = memsize;
	return TRUE;
free_front_info:
	HeapFree(pCache->hHeap, 0, pCache->front->info);
free_back_front:
	HeapFree(pCache->hHeap, 0, pCache->back->front);
free_front_front:
	HeapFree(pCache->hHeap, 0, pCache->front->front);
free_back:
	HeapFree(pCache->hHeap, 0, pCache->back);
free_front:
	HeapFree(pCache->hHeap, 0, pCache->front);
	pCache->front = NULL;
	pCache->back = NULL;
	return FALSE;
}


static inline void destroy_dbuffer(DCACHE *pCache) { //cache���� �ִ� �Ҵ�� �κ��� ���� free
	HeapFree(pCache->hHeap, 0, pCache->front->info);
	HeapFree(pCache->hHeap, 0, pCache->front->front);
	HeapFree(pCache->hHeap, 0, pCache->front);
	HeapFree(pCache->hHeap, 0, pCache->back->info);
	HeapFree(pCache->hHeap, 0, pCache->back->front);
	HeapFree(pCache->hHeap, 0, pCache->back);
}

static inline void dbuffer_swap(DCACHE *pCache) { //2���� dbuffer�� �����ư��鼭 �ٿ�ε带 ����. �� �� ������ ���빰�� �ϵ��� �����ϱ� ����.
	DBUFFER *temp = pCache->front;

	EnterCriticalSection(&pCache->bufcs);
	pCache->front = pCache->back;
	pCache->back = temp;
	LeaveCriticalSection(&pCache->bufcs);
}

//curl�� ���� ������ dbuffer�� �ִ� �۾��� �����ϴ� method. 
static inline BOOL dbuffer_insert(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) { 
	DBUFFER *pBuf;
	size_t nRemainingBytes;
	unsigned int index;

	EnterCriticalSection(&pCache->bufcs); //�ߺ� �۾��� ����
	pBuf = pCache->front;
	nRemainingBytes = pBuf->memsize - (pBuf->bp - pBuf->front);
	index = pBuf->index;
	if (nRemainingBytes < nBytes || index == pBuf->infosize) {
		LeaveCriticalSection(&pCache->bufcs);
		//printf("Insufficient : bytes : %zu, index : %u\n", nRemainingBytes, pBuf->index);
		return FALSE;
	}
	(pBuf->index)++;
	pBuf->info[index].data = pBuf->bp;
	pBuf->info[index].fp = fp;
	pBuf->info[index].size = nBytes;
	CopyMemory(pBuf->bp, lpBuffer, nBytes);
	pBuf->bp += nBytes;
	LeaveCriticalSection(&pCache->bufcs);
	return TRUE;
}

static inline BOOL init_cs(DCACHE *pCache) { //initialize critical section
	if (!InitializeCriticalSectionAndSpinCount(&pCache->listcs, CS_LOOP_COUNT))
		return FALSE;
	if (!InitializeCriticalSectionAndSpinCount(&pCache->bufcs, CS_LOOP_COUNT)) {
		DeleteCriticalSection(&pCache->listcs);
		return FALSE;
	}
	return TRUE;
}

static inline void destroy_cs(DCACHE *pCache) { //destroy critical section
	DeleteCriticalSection(&pCache->listcs);
	DeleteCriticalSection(&pCache->bufcs);
}



//dbuffer �ΰ��� ��� full�� �Ǿ��� ���� ����ϴ� ���� �������
static inline DNODE *dnode_alloc(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) {
	DNODE *pNewNode;

	pNewNode = (DNODE *)HeapAlloc(pCache->hHeap, 0, sizeof(DNODE) + nBytes);
	if (pNewNode == NULL)
		return NULL;
	pNewNode->front = ((char *)pNewNode) + sizeof(DNODE);
	pNewNode->size = nBytes;
	pNewNode->fp = fp;
	CopyMemory(pNewNode->front, lpBuffer, nBytes);
	pNewNode->next = NULL;
	return pNewNode;
}

static inline void dnode_free(DCACHE *pCache, DNODE *pNode) {  //free dnode
	//HeapFree(pCache->hHeap, 0, pNode->front);
	HeapFree(pCache->hHeap, 0, pNode);
}

static void dnode_nolock_destroy(DCACHE *pCache) { //free all dnode
	DNODE *pNode;
	DNODE *pNext;

	for (pNode = pCache->head; pNode; pNode = pNext) {
		pNext = pNode->next;
		dnode_free(pCache, pNode);
	}
}

#pragma warning(disable:4090)
static inline void dnode_lock_append(DCACHE *pCache, const DNODE *pNode) {
	EnterCriticalSection(&pCache->listcs);
	if (pCache->tail == NULL) {
		pCache->head = pNode;
		pCache->tail = pNode;
	}
	else {
		pCache->tail->next = pNode;
		pCache->tail = pNode;
	}
	LeaveCriticalSection(&pCache->listcs);
}
#pragma warning(default:4090)

static inline DNODE *dnode_lock_cut(DCACHE *pCache) {
	DNODE *pHead;

	EnterCriticalSection(&pCache->listcs);
	pHead = pCache->head;
	pCache->head = NULL;
	pCache->tail = NULL;
	LeaveCriticalSection(&pCache->listcs);
	return pHead;
}

//curl ���� buffer�� ������ dlist�� �ִ� method
static BOOL dlist_insert(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) {
	DNODE *pNewNode;

	pNewNode = dnode_alloc(pCache, lpBuffer, nBytes, fp);
	if (pNewNode == NULL)
		return FALSE;
	dnode_lock_append(pCache, pNewNode);
	return TRUE;
}

extern BOOL create_dcache(DCACHE **ppCache) { //dcache ���� method
	DCACHE *pCache;

	pCache = (DCACHE *)calloc(1, sizeof(DCACHE));
	if (pCache == NULL)
		return FALSE;

	if (!init_heap(pCache))
		goto free_cache;
	if (!init_cs(pCache))
		goto destroy_heap;
	if (!create_dbuffer(pCache))
		goto destroy_cs;

	*ppCache = pCache;
	return TRUE;
destroy_cs:
	destroy_cs(pCache);
destroy_heap:
	destroy_heap(pCache);
free_cache:
	free(pCache);
	return FALSE;
}

extern void destroy_dcache(DCACHE *pCache) { //destroy dcache method
	destroy_dbuffer(pCache);
	dnode_nolock_destroy(pCache);
	destroy_heap(pCache);
	destroy_cs(pCache);
	ZeroMemory(pCache, sizeof(DCACHE));
}


static BOOL dbuffer_flush(DCACHE *pCache, HANDLE hFile) {
	unsigned int i;
	DBUFFER *pBuf = pCache->back;
	DBUFINFO *pInfo;
	DWORD dwWritten;

	/*
	for (i = 0; i < pBuf->index; i++) {
	pInfo = &pBuf->info[i];
	if (!SetFilePointerEx(hFile, pInfo->fp, NULL, FILE_BEGIN))
	return FALSE;
	if (!WriteFile(hFile, pInfo->data, (DWORD)pInfo->size, &dwWritten, NULL))
	return FALSE;
	}
	*/
	LARGE_INTEGER prevfp = { .QuadPart = 0LL }; //64��Ʈ ������ 0���� �ʱ�ȭ�ϴ� ��.

	for (i = 0; i < pBuf->index; i++) {
		pInfo = &pBuf->info[i];
		if (i == 0 || prevfp.QuadPart != pInfo->fp.QuadPart) {//QuadPart�� 64��Ʈ
			if (!SetFilePointerEx(hFile, pInfo->fp, NULL, FILE_BEGIN))
				return FALSE;
		}
		if (!WriteFile(hFile, pInfo->data, (DWORD)pInfo->size, &dwWritten, NULL))
			return FALSE;
		prevfp.QuadPart = pInfo->fp.QuadPart + (long long)pInfo->size; //DBUFFER ûũ �����ŭ �Ű��ذ�
	}

	pBuf->index = 0;
	pBuf->bp = pBuf->front;
	return TRUE;
}//���ͳ� ���

extern BOOL dcache_insert(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) {
	// Buffer first, list later.
	if (!dbuffer_insert(pCache, lpBuffer, nBytes, fp))
		return dlist_insert(pCache, lpBuffer, nBytes, fp);
	return TRUE;
}//�����͸� ��� ĳ�ÿ� ���� ������, CURL�� �� �������� ���� �ּ�, ������ ������ ũ��, ���� ������


extern BOOL dcache_flush(DCACHE *pCache, HANDLE hFile) { //���� dbuffer_flush�� �ܺο����� ����� �� �ֵ��� �۾��� ��.
	DNODE *pNode;
	DNODE *pNext;
	DWORD dwWritten;

	if (pCache->front->index) {
		dbuffer_swap(pCache);
		if (!dbuffer_flush(pCache, hFile))
			return FALSE;
	}
	if (pCache->head) {
		for (pNode = dnode_lock_cut(pCache); pNode; pNode = pNext) {
			if (!SetFilePointerEx(hFile, pNode->fp, NULL, FILE_BEGIN))
				goto list_destroy;
			if (!WriteFile(hFile, pNode->front, (DWORD)pNode->size, &dwWritten, NULL))
				goto list_destroy;
			pNext = pNode->next;
			dnode_free(pCache, pNode);
		}
	}
	return TRUE;
list_destroy:
	for (; pNode; pNode = pNext) {
		pNext = pNode->next;
		dnode_free(pCache, pNode);
	}
	return FALSE;
}// call flush thread

extern BOOL dcache_empty(DCACHE *pCache) {
	return (pCache->head == NULL && pCache->front->index == 0);
} //cache�� ����ִ� ���� Ȯ��


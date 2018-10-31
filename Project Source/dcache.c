//각각의 스레드 내에서 curl로부터 파일을 다운받는 역할을 하는 파일입니다.
#ifdef _DEBUG
#include <stdio.h>
#endif _DEBUG
#include <stdlib.h>
#include <Windows.h>
#include "dcache.h"

#define CS_LOOP_COUNT 4000

static inline BOOL init_heap(DCACHE *pCache) { //thread안에 있는 cache를 메모리에 올리기 위해 필요한 heap 공간을 확보
	DWORD dwEnableFH = 2; // 실제로 메모리에 할당하는 것은 Heapalloc()으로 진행
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

static inline BOOL create_dbuffer(DCACHE *pCache) { //cache내에 위치한 공간으로 curl으로부터 받아온 데이터를 일차적으로 저장하는 공간
	const size_t memsize = 0x400000;  // 4MB 버퍼는 총 256MB면 충분하므로
	const unsigned int infosize = 2048;//2048이면 info가 모자라서 정보가 넘칠일은 없다

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
	pCache->front->index = 0; //info[index]에 새로운 정보가 들어가도록 하는 것. 컬이 가진 INFO가 이곳에 저장.
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


static inline void destroy_dbuffer(DCACHE *pCache) { //cache내에 있는 할당된 부분을 전부 free
	HeapFree(pCache->hHeap, 0, pCache->front->info);
	HeapFree(pCache->hHeap, 0, pCache->front->front);
	HeapFree(pCache->hHeap, 0, pCache->front);
	HeapFree(pCache->hHeap, 0, pCache->back->info);
	HeapFree(pCache->hHeap, 0, pCache->back->front);
	HeapFree(pCache->hHeap, 0, pCache->back);
}

static inline void dbuffer_swap(DCACHE *pCache) { //2개의 dbuffer를 번갈아가면서 다운로드를 진행. 꽉 찬 버퍼의 내용물을 하드웨어에 저장하기 위함.
	DBUFFER *temp = pCache->front;

	EnterCriticalSection(&pCache->bufcs);
	pCache->front = pCache->back;
	pCache->back = temp;
	LeaveCriticalSection(&pCache->bufcs);
}

//curl이 받은 파일을 dbuffer로 넣는 작업을 수행하는 method. 
static inline BOOL dbuffer_insert(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) { 
	DBUFFER *pBuf;
	size_t nRemainingBytes;
	unsigned int index;

	EnterCriticalSection(&pCache->bufcs); //중복 작업을 방지
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



//dbuffer 두개가 모두 full이 되었을 때를 대비하는 비상용 저장공간
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

//curl 내부 buffer의 파일을 dlist에 넣는 method
static BOOL dlist_insert(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) {
	DNODE *pNewNode;

	pNewNode = dnode_alloc(pCache, lpBuffer, nBytes, fp);
	if (pNewNode == NULL)
		return FALSE;
	dnode_lock_append(pCache, pNewNode);
	return TRUE;
}

extern BOOL create_dcache(DCACHE **ppCache) { //dcache 생성 method
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
	LARGE_INTEGER prevfp = { .QuadPart = 0LL }; //64비트 공간을 0으로 초기화하는 것.

	for (i = 0; i < pBuf->index; i++) {
		pInfo = &pBuf->info[i];
		if (i == 0 || prevfp.QuadPart != pInfo->fp.QuadPart) {//QuadPart는 64비트
			if (!SetFilePointerEx(hFile, pInfo->fp, NULL, FILE_BEGIN))
				return FALSE;
		}
		if (!WriteFile(hFile, pInfo->data, (DWORD)pInfo->size, &dwWritten, NULL))
			return FALSE;
		prevfp.QuadPart = pInfo->fp.QuadPart + (long long)pInfo->size; //DBUFFER 청크 사이즈만큼 옮겨준것
	}

	pBuf->index = 0;
	pBuf->bp = pBuf->front;
	return TRUE;
}//인터널 펑션

extern BOOL dcache_insert(DCACHE *pCache, const char *lpBuffer, size_t nBytes, LARGE_INTEGER fp) {
	// Buffer first, list later.
	if (!dbuffer_insert(pCache, lpBuffer, nBytes, fp))
		return dlist_insert(pCache, lpBuffer, nBytes, fp);
	return TRUE;
}//데이터를 어느 캐시에 넣을 것인지, CURL이 준 데이터의 시작 주소, 데이터 버퍼의 크기, 파일 포인터


extern BOOL dcache_flush(DCACHE *pCache, HANDLE hFile) { //위의 dbuffer_flush를 외부에서도 사용할 수 있도록 작업한 것.
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
} //cache가 비어있는 지를 확인


//각각의 스레드 내에서 curl로부터 파일을 다운받는 역할을 하는 파일입니다.
#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#include "dcache.h"

#define CS_LOOP_COUNT 4000

extern BOOL create_dcache(DCACHE **ppCache) {

}

extern void destroy_dcache(DCACHE *pCache) {

}

extern BOOL dcache_insert(DCACHE *pCache, const char *IpBuffer, size_t nBytes, LARGE_INTEGER fp) {

}//데이터를 어느 캐시에 넣을 것인지, CURL이 준 데이터의 시작 주소, 데이터 버퍼의 크기, 파일 포인터
extern BOOL dcache_flush(DCACHE *pCache, HANDLE hFile) {

}// call flush thread

extern BOOL dcache_empty(DCACHE *pCache) {

} // 캐시 contents 삭제


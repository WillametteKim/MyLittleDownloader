//������ ������ ������ curl�κ��� ������ �ٿ�޴� ������ �ϴ� �����Դϴ�.
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

}//�����͸� ��� ĳ�ÿ� ���� ������, CURL�� �� �������� ���� �ּ�, ������ ������ ũ��, ���� ������
extern BOOL dcache_flush(DCACHE *pCache, HANDLE hFile) {

}// call flush thread

extern BOOL dcache_empty(DCACHE *pCache) {

} // ĳ�� contents ����


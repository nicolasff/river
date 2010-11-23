#include "mem.h"

#include <stdlib.h>
#include <string.h>

size_t max_memory = 0;
static size_t cur_memory = 0;

#define HEADER_SIZE (sizeof(size_t))

void *
rmalloc(size_t size) {

	void *ptr;

	if(max_memory && cur_memory > max_memory) {
		return NULL;
	}

	cur_memory += size;
	ptr = malloc(size + HEADER_SIZE);
	memcpy(ptr, &size, HEADER_SIZE);
	return ptr + HEADER_SIZE;
}

void *
rcalloc(size_t nmemb, size_t size) {
	
	void *ptr;
	size_t total = nmemb * size;

	if(max_memory && cur_memory > max_memory) {
		return NULL;
	}

	cur_memory += total;
	ptr = calloc(1, total + HEADER_SIZE);
	memcpy(ptr, &total, HEADER_SIZE);
	return ptr + HEADER_SIZE;
}

char *
rstrdup(const char *s) {

	char *ret;
	size_t sz = strlen(s);

	ret = rcalloc(sz + 1, 1);
	memcpy(ret, s, sz);

	return ret;
}

void
rfree(void *ptr) {

	size_t sz;
	if(!ptr) {
		return;
	}

	memcpy(&sz, ptr - HEADER_SIZE, HEADER_SIZE);
	cur_memory -= sz;

	free(ptr-HEADER_SIZE);
}


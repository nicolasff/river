#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

extern size_t max_memory;

void *
rmalloc(size_t size);

void *
rcalloc(size_t nmemb, size_t size);

char *
rstrdup(const char *s);

void
rfree(void *ptr);

#endif

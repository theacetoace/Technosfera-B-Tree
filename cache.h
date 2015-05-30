#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include "mydb.h"
#include "page.h"

int read_cache(const DB *, void *, uint32_t);

int write_cache(const DB *, const Page *, uint32_t);

int flush_cache(const DB *);

#endif // CACHE_H

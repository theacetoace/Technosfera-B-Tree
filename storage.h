#ifndef STORAGE_H
#define STORAGE_H

#include <stdio.h>
#include "mydb.h"
#include "page.h"

int write_page(const DB *, const Page *, uint32_t);

int read_page(const DB *, void *, uint32_t);

size_t find_free_index(DB *);

void free_index(DB *, size_t);

#endif // STORAGE_H

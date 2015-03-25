#pragma once

#include <stdio.h>
#include "mydb.h"
#include "page.h"

int write_page(const DB *, const Page *, uint32_t);

int read_page(const DB *, void *, uint32_t);

size_t find_free_index(DB *);


#pragma once

#include "types.h"


typedef struct
{
    const uint32_t   magic;
    const PageSize   size;
    uint32_t         crc32;
    PageNumber number;
    PageKind   kind;
    uint8_t          data[1];
} Page;


/* do not use directly, use storage routines (storage.h) */
Page* page_create(PageSize size, PageKind kind);


/* parse page */
Page* page_parse(void* raw, PageSize expectedPageSize);


/* calculate page checksum and save in the page */
void page_checksum(Page*);

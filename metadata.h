#ifndef METADATA_H
#define METADATA_H


#include "mydb.h"
#include "page.h"
#include "types.h"


typedef struct
{
    Page       common;
    uint32_t   db_size;
    PageSize   padding;
    PageSize   page_size;
    PageNumber root;
    PageSize   index_count;
    PageNumber index_number[1];
} Metadata;


Metadata* metadata_create(const DBC config);


/* load page: page size read from metadata */
Metadata* metadata_parse(void* raw);

#endif // METADATA_H

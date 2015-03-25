#include <stdlib.h>
#include <strings.h>
#include <zlib.h>
#include <stdio.h>
#include "error.h"
#include "page.h"


Page* page_create(PageSize size, PageKind kind)
{
    uint32_t* result;

    result = malloc(size);

    bzero(result, size);

    result[0] = cPageMagic;
    result[1] = size;
    result[2] = 0;
    result[3] = 0;
    result[4] = kind;

    return (Page*)result;
}


Page* page_parse(void* raw, PageSize expectedPageSize)
{
    Page*    page;
    uint32_t crc;

    page = (Page*) raw;

    /* check magic */
    if (cPageMagic != page->magic)
    {
        printf("bad magic\n");
        error_code = ERROR_PAGE_BAD_MAGIC;
        return NULL;
    }

    /* check size */
    if (expectedPageSize != page->size)
    {   
        if (cPageMetaData == page->kind)
            expectedPageSize = page->size;
        else
        {
            printf("bad size\n");
            error_code = ERROR_PAGE_BAD_SIZE;
            return NULL;
        }
    }

    /* calculate CRC32 */
    //crc = crc32(0L, Z_NULL, 0);
    /* skip MAGIC, SIZE, CRC32 */
    //crc = crc32(crc, (void*)page, page->size - cPageRestOffset);
    crc = 0;
    /* check CRC32 */
    if (crc != page->crc32)
    {
        printf("bad crc\n");
        error_code = ERROR_PAGE_BAD_CRC32;
        return NULL;
    }

    if (page->kind != cPageMetaData &&
        page->kind != cPageIndex &&
        page->kind != cPageLeaf &&
        page->kind != cPageIntermediate &&
        page->kind != cPageRoot &&
        page->kind != (cPageRoot | cPageLeaf) &&
        page->kind != (cPageRoot | cPageIntermediate))
    {
        printf("bad kind\n");
        error_code = ERROR_PAGE_BAD_KIND;
        return NULL;
    }

    return page;
}


void page_checksum(Page* page)
{
    /* calculate CRC32 */
    page->crc32 = 0;//crc32(0L, Z_NULL, 0);
    /* skip MAGIC, SIZE, CRC32 */
    //page->crc32 = crc32(page->crc32, (void*)page, page->size - cPageRestOffset);
}

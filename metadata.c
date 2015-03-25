#include "metadata.h"

Metadata* metadata_create(const DBC config)
{
    Metadata* result;

    result = (Metadata*) page_create(config.page_size, cPageMetaData);

    if (NULL == result)
        return NULL;

    result->db_size     = config.db_size;
    result->page_size   = config.page_size;
    result->root        = cPageInvalid;
    result->index_count = 0;

    return result;
}


Metadata* metadata_parse(void* raw)
{
    return (Metadata*) page_parse(raw, 0);
}

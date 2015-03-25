#include "mydb.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>
#include "storage.h"
#include "b-tree.h"

int internal_db_select(DB *db, DBT *key, DBT *data) {
	return b_select(db, key, data);
}

int internal_db_insert(DB *db, DBT *key, DBT *data) {
	return b_insert(db, key, data);
}

DB *dbopen(const char *file, DBC *conf) {
    DB *result = (DB*)malloc(sizeof(DB));

    result->insert = internal_db_insert;
    result->select = internal_db_select;
    
    struct stat sb;
    
    if (stat(file, &sb) == 0 && S_ISREG(sb.st_mode)) {
        result->base = open(file, O_RDWR);
        
        result->parameters.db_size = 0;
        result->parameters.page_size = 0;
        result->parameters.cache_size = 0;

        void *meta_raw = (void *)malloc(cPagePadding);
        read_page(result, meta_raw, 0);
        Page *p = page_parse(meta_raw, cPagePadding);

        if (!p) { return NULL; }

        result->parameters.page_size = p->size;

        free(meta_raw);

        meta_raw = (void *)malloc(result->parameters.page_size);
        read_page(result, meta_raw, 0);
        p = page_parse(meta_raw, result->parameters.page_size);

        result->parameters.db_size    = ((uint32_t *)((void *)p->data))[0];
        result->parameters.cache_size = ((uint32_t *)((void *)p->data))[1];

        result->meta = p;

        void *index_raw = (void *)malloc(result->parameters.page_size);
        read_page(result, index_raw, 1);
        p = page_parse(index_raw, result->parameters.page_size);

        result->index = p;

        if (((uint32_t *)((void *)result->meta->data))[2] != 0) {
            void *root_raw = NULL;

            root_raw = (void *)malloc(result->parameters.page_size);
            read_page(result, root_raw, ((uint32_t *)((void *)result->meta->data))[2]);
            p = page_parse(root_raw, result->parameters.page_size);

            result->root = p;
        }

        return result;
    }

    result->base = open(file, O_CREAT | O_RDWR);
    
    result->parameters.db_size = conf->db_size;
    result->parameters.page_size = conf->page_size;
    result->parameters.cache_size = conf->cache_size;

    Page *meta = page_create(result->parameters.page_size, cPageMetaData);
    ((uint32_t *)((void *)meta->data))[0] = result->parameters.db_size;
    ((uint32_t *)((void *)meta->data))[1] = result->parameters.cache_size;
    write_page(result, meta, 0);

    result->meta = meta;

    Page *index = page_create(result->parameters.page_size, cPageIndex);
    memset((void *)(index->data), 0, (size_t)(result->parameters.page_size - cPagePadding)); // if segment
    if (write_page(result, index, 1) == -1) { return NULL; }

    result->index = index;
    
    return result;
}


int db_close(DB *db) {
    close(db->base);
    return 0;
}

int db_delete(DB *db, void *key, size_t key_len) {
    DBT keyt = {
		.data = key,
		.size = key_len
	};
	return db->delete(db, &keyt);
}

int db_select(DB *db, void *key, size_t key_len,
	   void **val, size_t *val_len) {
    DBT keyt = {
		.data = key,
		.size = key_len
	};
    DBT valt = {0, 0};
	int rc = db->select(db, &keyt, &valt);
	*val = valt.data;
	*val_len = valt.size;
	return rc;
}

int db_insert(DB *db, void *key, size_t key_len,
	   void *val, size_t val_len) {
    DBT keyt = {
		.data = key,
		.size = key_len
	};
    DBT valt = {
		.data = val,
		.size = val_len
	};
	return db->insert(db, &keyt, &valt);
}

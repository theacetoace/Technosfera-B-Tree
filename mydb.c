#include "mydb.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory.h>
#include "storage.h"
#include "b-tree.h"
#include "cache.h"

int internal_db_select(DB *db, DBT *key, DBT *data) {
    return b_select(db, key, data);
}

int internal_db_insert(DB *db, DBT *key, DBT *data) {
    return b_insert(db, key, data);
}

int internal_db_delete(DB *db, DBT *key) {
    return b_delete(db, key);
}

int internal_db_flush(DB *db) {
    return flush_cache(db);
}

DB *dbopen(const char *file, DBC *conf) {
    DB *result = (DB*)malloc(sizeof(DB));

    result->insert = internal_db_insert;
    result->select = internal_db_select;
    result->delete = internal_db_delete;
    result->sync   = internal_db_flush;
    
    struct stat sb;
    
    if (stat(file, &sb) == 0 && S_ISREG(sb.st_mode)) {
        result->base = open(file, O_RDWR);
        
        result->parameters.db_size = 0;
        result->parameters.page_size = 0;
        result->parameters.cache_size = 0;

        void *meta_raw = (void *)malloc(cPagePadding);
        read_meta(result, meta_raw, 0);
        Page *p = page_parse(meta_raw, cPagePadding);

        if (!p) { return NULL; }

        result->parameters.page_size = p->size;

        free(meta_raw);

        meta_raw = (void *)malloc(result->parameters.page_size);
        read_meta(result, meta_raw, 0);
        p = page_parse(meta_raw, result->parameters.page_size);

        result->parameters.db_size    = cast32(p->data, 0, 0);
        result->parameters.cache_size = cast32(p->data, 0, 1);

        result->meta = p;

        if (cast32(result->meta->data, 0, 2) != 0) {
            void *root_raw = NULL;

            root_raw = (void *)malloc(result->parameters.page_size);
            read_meta(result, root_raw, cast32(result->meta->data, 0, 2));
            p = page_parse(root_raw, result->parameters.page_size);

            result->root = p;
        }

        result->cache_size = result->parameters.cache_size / result->parameters.page_size;
        result->cache = (void *)malloc(result->cache_size * result->parameters.page_size);
        result->cache_id = (cache_struct *)malloc(result->cache_size * sizeof(cache_struct));
        for (size_t i = 0; i < result->cache_size; ++i) {
            result->cache_id[i].is_alive = false;
        }

        return result;
    }

    result->base = open(file, O_CREAT | O_RDWR, 0x777);
    
    result->parameters.db_size = conf->db_size;
    result->parameters.page_size = conf->page_size;
    result->parameters.cache_size = conf->cache_size;

    Page *meta = page_create(result->parameters.page_size, cPageMetaData);
    write32(meta->data, 0, 0, result->parameters.db_size);
    write32(meta->data, 0, 1, result->parameters.cache_size);
    size_t index_count = result->parameters.db_size / (result->parameters.page_size * 8 * (result->parameters.page_size - cPagePadding));
    write32(meta->data, 0, 3, index_count);

    for (size_t i = 1; i <= index_count; ++i) {
        Page *index = page_create(result->parameters.page_size, cPageIndex);
        memset((void *)(index->data), 0, (size_t)(result->parameters.page_size - cPagePadding));
        write_meta(result, index, i);
        free(index);
    }

    write_meta(result, meta, 0);
    result->meta = meta;

    result->cache_size = result->parameters.cache_size / result->parameters.page_size;
    result->cache = (void *)malloc(result->cache_size * result->parameters.page_size);
    result->cache_id = (cache_struct *)malloc(result->cache_size * sizeof(cache_struct));
    for (size_t i = 0; i < result->cache_size; ++i) {
        result->cache_id[i].is_alive = false;
    }

    return result;
}

DB *dbcreate(const char *file, DBC *conf) {
    return dbopen(file, conf);
}


int db_close(DB *db) {
    db->sync(db);
    free(db->cache);
    free(db->cache_id);
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

int db_flush(DB *db) {
    db->sync(db);
    return 0;
}

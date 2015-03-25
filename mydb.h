#pragma once

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include "types.h"
#include "page.h"

typedef struct {
    /* Maximum on-disk file size
     * 512MB by default
     * */
    size_t db_size;
    /* Page (node/data) size
     * 4KB by default
     * */
    size_t page_size;
    /* Maximum cached memory size
     * 16MB by default
     * */
    size_t cache_size;
} DBC;

typedef struct DB {
	/* Public API */
	/* Returns 0 on OK, -1 on Error */
	int (*close)(struct DB *db);
	int (*delete)(struct DB *db, DBT *key);
	int (*insert)(struct DB *db, DBT *key, DBT *data);
	/* * * * * * * * * * * * * *
	 * Returns malloc'ed data into 'struct DBT *data'.
	 * Caller must free data->data. 'struct DBT *data' must be alloced in
	 * caller.
	 * * * * * * * * * * * * * */
	int (*select)(struct DB *db, DBT *key, DBT *data);
	/* Sync cached pages with disk
	 * */
	int (*sync)(struct DB *db);
	/* For future uses - sync cached pages with disk
	 * int (*sync)(const struct DB *db)
	 * */
	/* Private API */
	/*     ...     */
    int base;
    DBC parameters;
    Page *meta, *index, *root;
} DB; /* Need for supporting multiple backends (HASH/BTREE) */



/* Open DB if it exists, otherwise create DB */
DB *dbopen(const char *file, DBC *conf);

int db_close(DB *db);
int db_delete(DB *, void *, size_t);
int db_select(DB *, void *, size_t, void **, size_t *);
int db_insert(DB *, void *, size_t, void * , size_t);

/* Sync cached pages with disk */
int db_sync(const DB *db);

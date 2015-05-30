#include "cache.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int get_page(const DB *db, void **p, uint32_t offset, bool is_read) {
	int pos = -1;
	int power_pos = -1;
	size_t min_power = 0;
	for (size_t i = 0; i < db->cache_size; ++i) {
		if (db->cache_id[i].is_alive && db->cache_id[i].offset == offset) {
			*p = (char *)db->cache + i * db->parameters.page_size;
			db->cache_id[i].power++;
			return 0;
		}
		if (pos == -1 && db->cache_id[i].is_alive == false) {
			pos = i;
		}
		if (db->cache_id[i].is_alive == true && 
			(min_power == 0 || min_power > db->cache_id[i].power)) {
			min_power = db->cache_id[i].power;
			power_pos = i;
		}
	}

	if (pos != -1) {
		db->cache_id[pos].is_alive = true;
		db->cache_id[pos].power = 1;
		db->cache_id[pos].offset = offset;
		db->cache_id[pos].is_dirty = !is_read;
		*p = (char *)db->cache + pos * db->parameters.page_size;
		if (!is_read) { return 0; }
		if (lseek(db->base, offset * db->parameters.page_size, SEEK_SET) == -1) {
			return -1;
		}
		return read(db->base, *p, db->parameters.page_size);
	}

	if (lseek(db->base, 
		db->cache_id[power_pos].offset * db->parameters.page_size, 
		SEEK_SET) == -1) {
		return -1;
	}

	*p = (char *)db->cache + power_pos * db->parameters.page_size;

	if (write(db->base, *p, db->parameters.page_size) == -1) {
		return -1; 
	}

	db->cache_id[power_pos].is_alive = true;
	db->cache_id[power_pos].power = 1;
	db->cache_id[power_pos].offset = offset;
	db->cache_id[pos].is_dirty = !is_read;
	if (!is_read) { return 0; }
	if (lseek(db->base, offset * db->parameters.page_size, SEEK_SET) == -1) {
		return -1;
	}
	return read(db->base, *p, db->parameters.page_size);
}

int read_cache(const DB *db, void *raw, uint32_t offset) {
	void *to_read;
	if (get_page(db, &to_read, offset, true) == -1) {
		return -1;
	}
	memcpy(raw, to_read, db->parameters.page_size);
	return 0;
}

int write_cache(const DB *db, const Page *p, uint32_t offset) {
	void *to_write;
	if (get_page(db, &to_write, offset, false) == -1) {
		return -1;
	}
	memcpy(to_write, (void *)p, db->parameters.page_size);
	return 0;
}

int flush_cache(const DB *db) {
	for (size_t i = 0; i < db->cache_size; ++i) {
		if (db->cache_id[i].is_alive && db->cache_id[i].is_dirty) {
			db->cache_id[i].is_alive = false;

			if (lseek(db->base, 
				db->cache_id[i].offset * db->parameters.page_size, 
				SEEK_SET) == -1) {
				return -1;
			}

			if (write(db->base, 
				(char *)db->cache + i * db->parameters.page_size, 
				db->parameters.page_size) == -1) {
				return 1;
			}
		}
	}
	return 0;
}

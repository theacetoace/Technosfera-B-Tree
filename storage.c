#include "storage.h"
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int read_page(const DB *db, void *raw, uint32_t offset) {
	if (lseek(db->base, offset * db->parameters.page_size, SEEK_SET) == -1) {
		return -1;
	}

	if (!offset && !db->parameters.page_size) {
		return read(db->base, raw, cPagePadding);
	}

	return read(db->base, raw, db->parameters.page_size);
}

int write_page(const DB *db, const Page *p, uint32_t offset) {
	if (lseek(db->base, offset * db->parameters.page_size, SEEK_SET) == -1) {
		return -1;
	}

	return write(db->base, (void *)p, db->parameters.page_size);
}

size_t find_free_index(DB *db) {
	if (!db->meta) { return 0; }

	size_t index_count = cast32(db->meta->data, 0, 3);

	void *raw = (void *)malloc(db->parameters.page_size);
	
	size_t ans = index_count + 1;

	for (size_t i = 1; i <= index_count; ++i) {
		read_page(db, raw, i);
    	Page *index = page_parse(raw, db->parameters.page_size);

    	void *data = index->data;

    	uint8_t pos = 1;
		size_t ind = 0;

    	while (ind < db->parameters.page_size - cPagePadding) {
			if (!(((uint8_t *)data + ind)[0] & pos)) {
				((uint8_t *)data + ind)[0] |= pos;
				write_page(db, index, i);
				free(raw);
				return ans;
			}

			ans++;
			pos <<= 1;
			if (!pos) { pos = 1; ind++; }
		}
	}
	free(raw);
	return 0;
}

void free_index(DB *db, size_t index) {
	if (!db->meta) { return; }

	size_t index_count = cast32(db->meta->data, 0, 3);

	index -= 1 + index_count;

	size_t ind_num = index / (8 * (db->parameters.page_size - cPagePadding));

	index -= ind_num * 8 * (db->parameters.page_size - cPagePadding);

	void *raw = (void *)malloc(db->parameters.page_size);
	read_page(db, raw, 1 + ind_num);
    Page *pindex = page_parse(raw, db->parameters.page_size);

	void *data = pindex->data;

	size_t ind = index >> 3;
	uint8_t pos = 1 << (index - (ind << 3));

	((uint8_t *)data + ind)[0] &= ~pos;
	write_page(db, pindex, 1 + ind_num);

	free(raw);
}

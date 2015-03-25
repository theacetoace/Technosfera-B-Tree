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
	if (!db->index) { return 0; }

	void *data = db->index->data;

	uint8_t pos = 1;
	size_t ind = 0;
	size_t ans = 2;

	while (true) {
		if (!pos) { pos = 1; ind++; }

		if (!(((uint8_t *)data + ind)[0] & pos)) {
			((uint8_t *)data + ind)[0] |= pos;
			return ans;
		}

		ans++;
		pos <<= 1;
	}
}

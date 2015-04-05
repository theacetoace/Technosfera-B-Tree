#include "b-tree.h"
#include "storage.h"

#include <memory.h>
#include <stdlib.h>

// TODO make faster
Page *bt_search(const DB *db, Page *node, DBT *key) {
	if (!node) { return NULL; }
	if (node->kind == cPageRoot || node->kind == cPageIntermediate) {
		size_t vert_num = cast32(node->data, 0, 2);
		size_t offset = 3 * sizeof(uint32_t);
		for (size_t i = 0; i < vert_num; ++i) {
			size_t key_offset = cast32(node->data, offset, 0);
			size_t size = cast32(node->data, key_offset, 0);
			int n = memcmp((uint8_t *)node->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
			if (n > 0 || (!n && size > key->size)) {
				size_t link = cast32(node->data, key_offset - sizeof(uint32_t), 0);
				void *raw = (void *)malloc(db->parameters.page_size);
				read_page(db, raw, link);
				Page *next = page_parse(raw, db->parameters.page_size);
				Page *res = bt_search(db, next, key);
				if (res != next) { free(raw); raw = NULL; }
				return res;
			}
			offset += sizeof(uint32_t);
		}
		size_t begining = cast16(node->data, 0, 0);
		size_t link = cast32(node->data, begining - sizeof(uint32_t), 0);
		void *raw = (void *)malloc(db->parameters.page_size);
		read_page(db, raw, link);
		Page *next = page_parse(raw, db->parameters.page_size);
		Page *res = bt_search(db, next, key);
		if (res != next) { free(raw); raw = NULL; }
		return res;
	}
	if (node->kind == cPageRootLeaf || node->kind == cPageLeaf) {
		return node;
	}
    return NULL;
}

void print_tree(const DB *db, Page *p) {
	size_t vert_num = cast32(p->data, 0, 2);

	printf("vertex number = %u vertex kind = %x vertex parent = %u vertex size = %u\n", 
		p->number, p->kind, cast32(p->data, 0, 1), cast32(p->data, 0, 2));
	fflush(stdout);

	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		if (p->kind == cPageRoot || p->kind == cPageIntermediate) {
			printf("%u/", cast32(p->data, key_offset - sizeof(uint32_t), 0));
			fflush(stdout);
		}
		size_t size = cast32(p->data, key_offset, 0);
		if (write(1, (uint8_t *)p->data + key_offset + sizeof(uint32_t), size) == -1) { return; }
		printf("/");
		if (i == vert_num - 1 && (p->kind == cPageRoot || p->kind == cPageIntermediate)) {
			printf("%u", cast32(p->data, key_offset + size + sizeof(uint32_t), 0));
			fflush(stdout);
		}
		fflush(stdout);
		offset += sizeof(uint32_t);
	}

	printf("\n\n");
	fflush(stdout);

	if (p->kind == cPageLeaf || p->kind == cPageRootLeaf) {
		return;
	}

	void *raw = (void *)malloc(db->parameters.page_size);

	offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		size_t link = cast32(p->data, key_offset - sizeof(uint32_t), 0);

		read_page(db, raw, link);
		Page *next = page_parse(raw, db->parameters.page_size);

		print_tree(db, next);

		offset += sizeof(uint32_t);
	}

	size_t key_offset = cast16(p->data, 0, 0);
	size_t link = cast32(p->data, key_offset - sizeof(uint32_t), 0);
	
	read_page(db, raw, link);
	Page *next = page_parse(raw, db->parameters.page_size);

	print_tree(db, next);

	free(raw);
	raw = NULL;
}

int b_select(const DB *db, DBT *key, DBT *data) {
	Page *p = bt_search(db, db->root, key);

	if (!p) {
		return -1; 
	}
	
	size_t vert_num = cast32(p->data, 0, 2);
	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		size_t size = cast32(p->data, key_offset, 0);
		int n = memcmp((uint8_t *)p->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
		if (!n && size == key->size) {
			size_t data_offset = cast32(p->data, key_offset + size + sizeof(uint32_t), 0);
			data->size = cast32(p->data, data_offset, 0);
			data->data = (void *)malloc(data->size);
			memcpy(data->data, (uint8_t *)p->data + data_offset + sizeof(uint32_t), data->size);
			if (p != db->root) { free(p); p = NULL; }

			return 0;
		}
		offset += sizeof(uint32_t);
	}

	if (p != db->root) { free(p); p = NULL; }

	return -1;
}

void fill_leaf(DB *db, Page *p, Page *node, size_t l, size_t r, size_t pos, DBT *key, DBT *data) {
	size_t begining = 3 * sizeof(uint32_t) + (r - l) * sizeof(uint32_t);
	size_t ending = db->parameters.page_size - cPagePadding;

	size_t offset = 3 * sizeof(uint32_t) + l * sizeof(uint32_t);
	if (pos < l) { offset -= sizeof(uint32_t); }
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;
	for (size_t i = l; i < r; ++i) {
		if (pos == i) {
			ending -= sizeof(uint32_t) + data->size;

			write32(node_data, new_offset, 0, begining);
			write32(node_data, begining, 0, key->size);
			memcpy((uint8_t *)node_data + begining + sizeof(uint32_t), key->data, key->size);
			write32(node_data, begining + key->size + sizeof(uint32_t), 0, ending);
			
			write32(node_data, ending, 0, data->size);
			memcpy((uint8_t *)node_data + ending + sizeof(uint32_t), data->data, data->size);

			begining += 2 * sizeof(uint32_t) + key->size;
			
			new_offset += sizeof(uint32_t);
			continue;
		}

		size_t key_offset = cast32(p->data, offset, 0);
		write32(node_data, new_offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t data_offset = cast32(p->data, key_offset + k_size + sizeof(uint32_t), 0);
		size_t d_size = cast32(p->data, data_offset, 0);

		ending -= d_size + sizeof(uint32_t);
		memcpy((uint8_t *)node_data + ending, (uint8_t *)p->data + data_offset, d_size + sizeof(uint32_t));
		write32(node_data, begining + k_size + sizeof(uint32_t), 0, ending);
		begining += 2 * sizeof(uint32_t) + k_size;

		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, ending);
}

void fill_intermediate(Page *p, Page *node, size_t l, size_t r, size_t pos, DBT *key, size_t link) {
	size_t begining = 3 * sizeof(uint32_t) + (r - l) * sizeof(uint32_t);
	
	size_t offset = 3 * sizeof(uint32_t) + l * sizeof(uint32_t);
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;

	size_t old_key_offset = cast32(p->data, offset, 0);
	size_t old_begining = old_key_offset - sizeof(uint32_t);

	write32(node_data, begining, 0, cast32(p->data, old_begining, 0));
	begining += sizeof(uint32_t);
	for (size_t i = l; i < r; ++i) {
		if (pos == i) {
			write32(node_data, new_offset, 0, begining);
			write32(node_data, begining, 0, key->size);
			memcpy((uint8_t *)node_data + begining + sizeof(uint32_t), key->data, key->size);
			write32(node_data, begining + key->size + sizeof(uint32_t), 0, link);

			begining += 2 * sizeof(uint32_t) + key->size;
			new_offset += sizeof(uint32_t);
			continue;
		}

		size_t key_offset = cast32(p->data, offset, 0);
		write32(node_data, new_offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t next_link = cast32(p->data, key_offset + k_size + sizeof(uint32_t), 0);
		write32(node_data, begining + k_size + sizeof(uint32_t), 0, next_link);
		
		begining += 2 * sizeof(uint32_t) + k_size;
		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, 0);
}

void insert_into_leaf(DB *db, Page **pp, size_t pos,DBT *key, DBT *data) {
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(t->data, 0, 2, vert_num + 1);

	fill_leaf(db, p, t, 0, vert_num + 1, pos, key, data);

	*pp = t;
	free(p);
	p = NULL;
}

void divide_leaf(DB *db, Page **pp, Page **ps, size_t pos, DBT *key, DBT *data) {
	Page *s = page_create(db->parameters.page_size, cPageLeaf);
	Page *t = page_create(db->parameters.page_size, cPageLeaf);
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	size_t tvert_num = (vert_num + 1) >> 1;
	size_t svert_num = vert_num - tvert_num + 1;

	/* copy meta from p to s */
	t->number = p->number;
	s->number = find_free_index(db);

	write32(s->data, 0, 1, cast32(p->data, 0, 1));
	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(s->data, 0, 2, svert_num);
	write32(t->data, 0, 2, tvert_num);

	/* insert verticies */

	fill_leaf(db, p, t, 0, tvert_num, pos, key, data);
	fill_leaf(db, p, s, tvert_num, vert_num + 1, pos, key, data);

	*pp = t;
	*ps = s;
	free(p);
	p = NULL;
}

void write_parents(DB *db, Page *p) {
	void *raw = (void *)malloc(db->parameters.page_size);

	size_t vert_num = cast32(p->data, 0, 2);
	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		size_t link = cast32(p->data, key_offset - sizeof(uint32_t), 0);
		
		read_page(db, raw, link);
		Page *next = page_parse(raw, db->parameters.page_size);
		write32(next->data, 0, 1, p->number);
		write_page(db, next, next->number);
		
		offset += sizeof(uint32_t);
	}
	size_t key_offset = cast32(p->data, 0, 2 + vert_num);
	size_t size = cast32(p->data, key_offset, 0);
	size_t begining = key_offset + size + sizeof(uint32_t);
	size_t link = cast32(p->data, begining, 0);

	read_page(db, raw, link);
	Page *next = page_parse(raw, db->parameters.page_size);
	write32(next->data, 0, 1, p->number);
	write_page(db, next, next->number);

	free(raw); 
	raw = NULL;
}

void divide_intermediate(DB *db, Page **pp, Page **ps, size_t pos, DBT *key, size_t link, DBT *res) {
	Page *s = page_create(db->parameters.page_size, cPageIntermediate);
	Page *t = page_create(db->parameters.page_size, cPageIntermediate);
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	size_t tvert_num = (vert_num + 1) >> 1;
	size_t svert_num = vert_num - tvert_num;

	/* copy meta from p to s */
	t->number = p->number;
	s->number = find_free_index(db);

	write32(s->data, 0, 1, cast32(p->data, 0, 1));
	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(s->data, 0, 2, svert_num);
	write32(t->data, 0, 2, tvert_num);

	/* insert verticies */
	fill_intermediate(p, t, 0, tvert_num, pos, key, link);
	size_t l = pos <= tvert_num ? tvert_num : (tvert_num + 1);
	size_t p_pos = pos == tvert_num ? 0 : pos;
	fill_intermediate(p, s, l, l + svert_num, p_pos, key, link);

	if (pos == tvert_num) {
		size_t key_offset = cast32(s->data, 3 * sizeof(uint32_t), 0);
		write32(s->data, key_offset - sizeof(uint32_t), 0, link);
		res->size = key->size;
		res->data = (void *)malloc(res->size);
		memcpy(res->data, key->data, res->size);
	} else {
		if (pos < tvert_num) { tvert_num--; }
		size_t key_offset = cast32(p->data, 3 * sizeof(uint32_t), tvert_num);
		res->size = cast32(p->data, key_offset, 0);
		res->data = (void *)malloc(res->size);
		memcpy(res->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), res->size);
	}

	write_parents(db, s);

	*pp = t;
	*ps = s;
	free(p);
	p = NULL;
}

void insert_into_intermediate(DB *db, Page **pparent, size_t pos, DBT *key, size_t link) {
	Page *p = *pparent;
	Page *t = page_create(db->parameters.page_size, p->kind);

	size_t vert_num = cast32(p->data, 0, 2);

	t->number = p->number;
	write32(t->data, 0, 1, cast32(p->data, 0, 1));
	write32(t->data, 0, 2, vert_num + 1);

	fill_intermediate(p, t, 0, vert_num + 1, pos, key, link);

	*pparent = t;
	free(p);
	p = NULL;
}

void create_root_leaf(DB *db, DBT *key, DBT *data) {
	Page *root = page_create(db->parameters.page_size, cPageRootLeaf);
	db->root = root;

	/* set data */
	void *node_data = db->root->data;
	/* padding from begining and ending */
	write16(node_data, 0, 0, key->size + 3 * sizeof(uint32_t) + 3 * sizeof(uint32_t));
	write16(node_data, 0, 1, db->parameters.page_size - cPagePadding - data->size - sizeof(uint32_t));
	/* parent */
	write32(node_data, 0, 1, 0);

	/* number of vertices */
	size_t offset = 2 * sizeof(uint32_t);
	write32(node_data, offset, 0, 1);

	/* key offset array */
	offset += sizeof(uint32_t);
	write32(node_data, offset, 0, offset + sizeof(uint32_t));

	/* key { size, val, offset_data_link } */
	offset += sizeof(uint32_t);
	write32(node_data, offset, 0, key->size);
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)node_data + offset, key->data, key->size);
	offset += key->size;
	write32(node_data, offset, 0, db->parameters.page_size - cPagePadding - data->size - sizeof(uint32_t));

	/* data { size, val } */
	offset = db->parameters.page_size - cPagePadding - data->size - sizeof(uint32_t);
	write32(node_data, offset, 0, data->size);
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)node_data + offset, data->data, data->size);
	offset += data->size;

	/* finding page to store */
	size_t index = find_free_index(db);
	write32(db->meta->data, 0, 2, index);
	db->root->number = index;
	write_page(db, db->root, index);
	write_page(db, db->meta, 0);
}

void create_root(DB *db, Page **pp, DBT *key, size_t link_l, size_t link_r) {
	Page *p = page_create(db->parameters.page_size, cPageRoot);
	p->number = find_free_index(db);

	/* padding from begining and ending */
	write16(p->data, 0, 0, key->size + 3 * sizeof(uint32_t) + 4 * sizeof(uint32_t));
	write16(p->data, 0, 1, 0);
	/* parent */
	write32(p->data, 0, 1, 0);

	/* number of vertices */
	size_t offset = 2 * sizeof(uint32_t);
	write32(p->data, offset, 0, 1);

	/* key offset array */
	offset += sizeof(uint32_t);
	write32(p->data, offset, 0, offset + 2 * sizeof(uint32_t));

	/* key { size, val, next_link } */
	offset += sizeof(uint32_t);
	write32(p->data, offset, 0, link_l);
	offset += sizeof(uint32_t);
	write32(p->data, offset, 0, key->size);
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)p->data + offset, key->data, key->size);
	offset += key->size;
	write32(p->data, offset, 0, link_r);
	
	*pp = p;
}

void create_intermediate(DB *db, Page **pp, DBT *key, size_t link_l, size_t link_r) {
	Page *p = *pp;
	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	/* padding from begining and ending */
	write16(t->data, 0, 0, key->size + 3 * sizeof(uint32_t) + 4 * sizeof(uint32_t));
	write16(t->data, 0, 1, 0);
	/* parent */
	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	/* number of vertices */
	size_t offset = 2 * sizeof(uint32_t);
	write32(t->data, offset, 0, 1);

	/* key offset array */
	offset += sizeof(uint32_t);
	write32(t->data, offset, 0, offset + 2 * sizeof(uint32_t));

	/* key { size, val, next_link } */
	offset += sizeof(uint32_t);
	write32(t->data, offset, 0, link_l);
	offset += sizeof(uint32_t);
	write32(t->data, offset, 0, key->size);
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)t->data + offset, key->data, key->size);
	offset += key->size;
	write32(t->data, offset, 0, link_r);

	*pp = t;
	free(p);
}

// TODO make faster
size_t find_pos(Page *p, DBT *key) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t offset = 3 * sizeof(uint32_t);
	size_t pos = 0;
	for (; pos < vert_num; ++pos) {
		size_t key_offset = cast32(p->data, offset, 0);
		size_t size = cast32(p->data, key_offset, 0);
		int n = memcmp((uint8_t *)p->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
		if (n > 0 || (!n && size > key->size)) {
			break;
		}
		offset += sizeof(uint32_t);
	}
	return pos;
}

int divide_intermediate_process(DB *db, Page **pp, size_t pos, DBT *key, size_t link) {
	Page *s = NULL;

	DBT skey;
	divide_intermediate(db, pp, &s, pos, key, link, &skey);

	Page *p = *pp;

	Page *parent = NULL;

	size_t parent_index = cast32(p->data, 0, 1);

	if (parent_index) {
		write_page(db, p, p->number);
		write_page(db, s, s->number);

		void *raw = (void *)malloc(db->parameters.page_size);
		read_page(db, raw, parent_index);
		parent = page_parse(raw, db->parameters.page_size);

		size_t parent_vert_num = cast32(parent->data, 0, 2);

		size_t p_pos = find_pos(parent, &skey);

		if (parent_vert_num < cPageNodesUp) {
			insert_into_intermediate(db, &parent, p_pos, &skey, s->number);
			write_page(db, parent, parent_index);
			if (parent->kind == cPageRoot) {
				memcpy(db->root, parent, db->parameters.page_size);
			}
		} else {
			divide_intermediate_process(db, &parent, p_pos, &skey, s->number);
		}
		
		free(parent);
		parent = NULL;
	} else {
		create_root(db, &parent, &skey, p->number, s->number);

		write32(p->data, 0, 1, parent->number);
		write32(s->data, 0, 1, parent->number);

		write_page(db, p, p->number);
		write_page(db, s, s->number);

		free(db->root);
		db->root = parent;
		write_page(db, parent, parent->number);
	}

	free(s);
	s = NULL;
	free(skey.data);
	skey.data = NULL;
	return 0;
}

void fill_leaf_same(DB *db, Page *p, Page *node, DBT *key, DBT *data) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t begining = 3 * sizeof(uint32_t) + vert_num * sizeof(uint32_t);
	size_t ending = db->parameters.page_size - cPagePadding;

	size_t offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		write32(node_data, offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t data_offset = cast32(p->data, key_offset + k_size + sizeof(uint32_t), 0);
		size_t d_size = cast32(p->data, data_offset, 0);

		if (!memcmp(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), min(key->size, k_size)) && key->size == k_size) {
			ending -= data->size + sizeof(uint32_t);
			write32(node_data, ending, 0, data->size);
			memcpy((uint8_t *)node_data + ending + sizeof(uint32_t), data->data, data->size);
		} else {
			ending -= d_size + sizeof(uint32_t);
			memcpy((uint8_t *)node_data + ending, (uint8_t *)p->data + data_offset, d_size + sizeof(uint32_t));
		}

		write32(node_data, begining + k_size + sizeof(uint32_t), 0, ending);
		begining += 2 * sizeof(uint32_t) + k_size;

		offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, ending);
}

void insert_same(DB *db, Page **pp, DBT *key, DBT *data) {
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(t->data, 0, 2, vert_num);

	fill_leaf_same(db, p, t, key, data);

	*pp = t;
	free(p);
	p = NULL;
}

void fill_leaf_delete(DB *db, Page *p, Page *node, DBT *key) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t begining = 3 * sizeof(uint32_t) + (vert_num - 1) * sizeof(uint32_t);
	size_t ending = db->parameters.page_size - cPagePadding;

	size_t offset = 3 * sizeof(uint32_t);
	size_t new_offset = offset;
	void *node_data = node->data;
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);

		size_t k_size = cast32(p->data, key_offset, 0);

		if (!memcmp(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), min(key->size, k_size)) && key->size == k_size) {
			offset += sizeof(uint32_t);
			continue;
		}

		write32(node_data, new_offset, 0, begining);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t data_offset = cast32(p->data, key_offset + k_size + sizeof(uint32_t), 0);
		size_t d_size = cast32(p->data, data_offset, 0);

		ending -= d_size + sizeof(uint32_t);
		memcpy((uint8_t *)node_data + ending, (uint8_t *)p->data + data_offset, d_size + sizeof(uint32_t));

		write32(node_data, begining + k_size + sizeof(uint32_t), 0, ending);
		begining += 2 * sizeof(uint32_t) + k_size;

		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, ending);
}

void fill_intermediate_delete(Page *p, Page *node, size_t link) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t begining = 3 * sizeof(uint32_t) + (vert_num - 1) * sizeof(uint32_t);
	
	size_t offset = 3 * sizeof(uint32_t);
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;

	size_t old_key_offset = cast32(p->data, offset, 0);
	size_t old_begining = old_key_offset - sizeof(uint32_t);

	write32(node_data, begining, 0, cast32(p->data, old_begining, 0));
	begining += sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);

		if (cast32(p->data, key_offset - sizeof(uint32_t), 0) == link) {
			offset += sizeof(uint32_t);
			continue;
		}

		write32(node_data, new_offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + 2 * sizeof(uint32_t));
		
		begining += 2 * sizeof(uint32_t) + k_size;
		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, 0);
}

void delete_key_intermediate(DB *db, Page **pp, size_t link) {
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(t->data, 0, 2, vert_num - 1);

	fill_intermediate_delete(p, t, link);

	*pp = t;
	free(p);
	p = NULL;
}

void fill_fintermediate_delete(Page *p, Page *node) {
	size_t vert_num = cast32(p->data, 0, 2) - 1;
	size_t begining = 3 * sizeof(uint32_t) + vert_num * sizeof(uint32_t);
	
	size_t offset = 4 * sizeof(uint32_t);
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;

	size_t old_key_offset = cast32(p->data, offset, 0);
	size_t old_begining = old_key_offset - sizeof(uint32_t);

	write32(node_data, begining, 0, cast32(p->data, old_begining, 0));
	begining += sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		write32(node_data, new_offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + 2 * sizeof(uint32_t));
		
		begining += 2 * sizeof(uint32_t) + k_size;
		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, 0);
}

void delete_first_intermediate(DB *db, Page **pp) {
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(t->data, 0, 2, vert_num - 1);

	fill_fintermediate_delete(p, t);

	*pp = t;
	free(p);
	p = NULL;
}

void fill_lintermediate_delete(Page *p, Page *node) {
	size_t vert_num = cast32(p->data, 0, 2) - 1;
	size_t begining = 3 * sizeof(uint32_t) + vert_num * sizeof(uint32_t);
	
	size_t offset = 3 * sizeof(uint32_t);
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;

	size_t old_key_offset = cast32(p->data, offset, 0);
	size_t old_begining = old_key_offset - sizeof(uint32_t);

	write32(node_data, begining, 0, cast32(p->data, old_begining, 0));
	begining += sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		write32(node_data, new_offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + 2 * sizeof(uint32_t));
		
		begining += 2 * sizeof(uint32_t) + k_size;
		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, 0);
}

void delete_last_intermediate(DB *db, Page **pp) {
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(t->data, 0, 2, vert_num - 1);

	fill_lintermediate_delete(p, t);

	*pp = t;
	free(p);
	p = NULL;
}

void delete_key_leaf(DB *db, Page **pp, DBT *key) {
	Page *p = *pp;

	size_t vert_num = cast32(p->data, 0, 2);

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	write32(t->data, 0, 1, cast32(p->data, 0, 1));

	write32(t->data, 0, 2, vert_num - 1);

	fill_leaf_delete(db, p, t, key);

	*pp = t;
	free(p);
	p = NULL;
}

void find_links(Page *p, uint32_t **plinks) {
	size_t vert_num = cast32(p->data, 0, 2);

	uint32_t *links = (uint32_t *)malloc((vert_num + 1) * sizeof(uint32_t));

	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		links[i] = cast32(p->data, key_offset - sizeof(uint32_t), 0);
		offset += sizeof(uint32_t);
	}

	size_t begining = cast16(p->data, 0, 0);
	links[vert_num] = cast32(p->data, begining - sizeof(uint32_t), 0);

	*plinks = links;
}

// TODO could be faster
size_t find_sibling(DB *db, const uint32_t *links, uint32_t val, size_t size) {
	size_t max_size = 0;
	size_t res = 0;

	void *raw = (void *)malloc(db->parameters.page_size);

	for (size_t i = 0; i < size; ++i) {
		if (i && links[i - 1] == val) {
			read_page(db, raw, links[i]);
			Page *p = page_parse(raw, db->parameters.page_size);
			if (cast32(p->data, 0, 2) > max_size) {
				max_size = cast32(p->data, 0, 2);
				res = i;
			}
			break;
		}
		if (i < size - 1 && links[i + 1] == val) {
			read_page(db, raw, links[i]);
			Page *p = page_parse(raw, db->parameters.page_size);
			if (cast32(p->data, 0, 2) > max_size) {
				max_size = cast32(p->data, 0, 2);
				res = i;
			}
			++i;
		}
	}

	free(raw);
	return links[res];
}

void find_first_leaf(Page *p, DBT *key, DBT *data) {
	size_t key_offset = cast32(p->data, 0, 3);
	key->size = cast32(p->data, key_offset, 0);
	key->data = (void *)malloc(key->size);
	memcpy(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), key->size);

	size_t data_offset = cast32(p->data, key_offset + key->size + sizeof(uint32_t), 0);
	data->size = cast32(p->data, data_offset, 0);
	data->data = (void *)malloc(data->size);
	memcpy(data->data, (uint8_t *)p->data + data_offset + sizeof(uint32_t), data->size);
}

void find_last_leaf(Page *p, DBT *key, DBT *data) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t key_offset = cast32(p->data, 0, 2 + vert_num);
	key->size = cast32(p->data, key_offset, 0);
	key->data = (void *)malloc(key->size);
	memcpy(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), key->size);

	size_t data_offset = cast32(p->data, key_offset + key->size + sizeof(uint32_t), 0);
	data->size = cast32(p->data, data_offset, 0);
	data->data = (void *)malloc(data->size);
	memcpy(data->data, (uint8_t *)p->data + data_offset + sizeof(uint32_t), data->size);
}

void redistribute_keys_leaf(DB *db, Page **pp, Page **ps, Page **pparent) {
	Page *p = *pp, *s = *ps, *parent = *pparent;

	size_t p_vert_num = cast32(p->data, 0, 2);
	size_t s_vert_num = cast32(s->data, 0, 2);

	size_t p_key_offset = cast32(p->data, 0, 2 + p_vert_num);
	size_t s_key_offset = cast32(s->data, 0, 3);

	size_t p_size = cast32(p->data, p_key_offset, 0);
	size_t s_size = cast32(s->data, s_key_offset, 0);

	int n = memcmp((uint8_t *)p->data + p_key_offset + sizeof(uint32_t),
				   (uint8_t *)s->data + s_key_offset + sizeof(uint32_t), 
				   min(p_size, s_size));

	if (!n) { n = p_size - s_size; }

	size_t iter = ((s_vert_num + p_vert_num) >> 1) - p_vert_num;

	DBT key, data;

	/* redistribute */
	for (size_t i = 0; i < iter; ++i) {
		fflush(stdout);
		if (n < 0) {
			find_first_leaf(s, &key, &data);
		} else {
			find_last_leaf(s, &key, &data);
		}
		delete_key_leaf(db, &s, &key);
		size_t pos = find_pos(p, &key);
		insert_into_leaf(db, &p, pos, &key, &data);
		free(key.data);
		free(data.data);
	}

	size_t l_link = s->number;
	if (n < 0) { l_link = p->number; }

	delete_key_intermediate(db, &parent, l_link);

	size_t r_link = p->number;
	if (n < 0) { r_link = s->number; }

	if (n < 0) {
		find_first_leaf(s, &key, &data);
	} else {
		find_first_leaf(p, &key, &data);
	}

	if (cast32(parent->data, 0, 2)) {
		size_t pos = find_pos(parent, &key);
		insert_into_intermediate(db, &parent, pos, &key, r_link);
	} else {
		create_intermediate(db, &parent, &key, l_link, r_link);
	}

	free(key.data);
	free(data.data);
	*pp = p;
	*ps = s;
	*pparent = parent;
}

size_t find_first_intermediate(Page *p, DBT *key) {
	size_t key_offset = cast32(p->data, 0, 3);
	key->size = cast32(p->data, key_offset, 0);
	key->data = (void *)malloc(key->size);
	memcpy(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), key->size);
	return cast32(p->data, key_offset - sizeof(uint32_t), 0);
}

size_t find_last_intermediate(Page *p, DBT *key) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t key_offset = cast32(p->data, 0, 2 + vert_num);
	key->size = cast32(p->data, key_offset, 0);
	key->data = (void *)malloc(key->size);
	memcpy(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), key->size);
	return cast32(p->data, key_offset + key->size + sizeof(uint32_t), 0);
}

void find_key_intermediate(Page *p, size_t link, DBT *key) {
	size_t vert_num = cast32(p->data, 0, 2);
	
	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);

		if (cast32(p->data, key_offset - sizeof(uint32_t), 0) == link) {
			key->size = cast32(p->data, key_offset, 0);
			key->data = (void *)malloc(key->size);
			memcpy(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), key->size);
			return;
		}

		offset += sizeof(uint32_t);
	}
}

void fill_fintermediate(Page *p, Page *node, DBT *key, size_t link) {
	size_t vert_num = cast32(p->data, 0, 2);
	size_t begining = 3 * sizeof(uint32_t) + (vert_num + 1) * sizeof(uint32_t);
	
	size_t offset = 3 * sizeof(uint32_t);
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;

	size_t old_key_offset = cast32(p->data, offset, 0);
	size_t old_begining = old_key_offset - sizeof(uint32_t);
	
	write32(node_data, begining, 0, link);
	begining += sizeof(uint32_t);
	write32(node_data, new_offset, 0, begining);
	write32(node_data, begining, 0, key->size);
	begining += sizeof(uint32_t);
	memcpy((uint8_t *)node_data + begining, key->data, key->size);
	begining += key->size;

	write32(node_data, begining, 0, cast32(p->data, old_begining, 0));
	begining += sizeof(uint32_t);
	new_offset += sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = cast32(p->data, offset, 0);
		write32(node_data, new_offset, 0, begining);

		size_t k_size = cast32(p->data, key_offset, 0);
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + 2 * sizeof(uint32_t));
		
		begining += 2 * sizeof(uint32_t) + k_size;
		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	write16(node_data, 0, 0, begining);
	write16(node_data, 0, 1, 0);
}

void insert_first_into_intermediate(DB *db, Page **pparent, DBT *key, size_t link) {
	Page *p = *pparent;
	Page *t = page_create(db->parameters.page_size, p->kind);

	size_t vert_num = cast32(p->data, 0, 2);

	t->number = p->number;
	write32(t->data, 0, 1, cast32(p->data, 0, 1));
	write32(t->data, 0, 2, vert_num + 1);

	fill_fintermediate(p, t, key, link);

	*pparent = t;
	free(p);
	p = NULL;
}

void redistribute_keys_intermediate(DB *db, Page **pp, Page **ps, Page **pparent) {
	Page *p = *pp, *s = *ps, *parent = *pparent;

	size_t p_vert_num = cast32(p->data, 0, 2);
	size_t s_vert_num = cast32(s->data, 0, 2);

	size_t p_key_offset = cast32(p->data, 0, 2 + p_vert_num);
	size_t s_key_offset = cast32(s->data, 0, 3);

	size_t p_size = cast32(p->data, p_key_offset, 0);
	size_t s_size = cast32(s->data, s_key_offset, 0);

	int n = memcmp((uint8_t *)p->data + p_key_offset + sizeof(uint32_t),
				   (uint8_t *)s->data + s_key_offset + sizeof(uint32_t), 
				   min(p_size, s_size));

	if (!n) { n = p_size - s_size; }

	size_t iter = ((s_vert_num + p_vert_num) >> 1) - p_vert_num;

	DBT key, pkey;

	size_t l_link = s->number;
	if (n < 0) { l_link = p->number; }

	size_t r_link = p->number;
	if (n < 0) { r_link = s->number; }

	find_key_intermediate(parent, l_link, &pkey);

	/* redistribute */
	for (size_t i = 0; i < iter; ++i) {
		size_t link;
		if (n < 0) {
			link = find_first_intermediate(s, &key);
			delete_first_intermediate(db, &s);
			size_t pos = p_vert_num + i;
			insert_into_intermediate(db, &p, pos, &pkey, link);
		} else {
			link = find_last_intermediate(s, &key);
			delete_last_intermediate(db, &s);
			insert_first_into_intermediate(db, &p, &pkey, link);
		}
		pkey.size = key.size;
		pkey.data = (void *)realloc(pkey.data, key.size);
		memcpy(pkey.data, key.data, pkey.size);
		free(key.data);
	}

	write_parents(db, p);

	delete_key_intermediate(db, &parent, l_link);

	if (cast32(parent->data, 0, 2)) {
		size_t pos = find_pos(parent, &key);
		insert_into_intermediate(db, &parent, pos, &key, r_link);
	} else {
		create_intermediate(db, &parent, &key, l_link, r_link);
	}

	free(pkey.data);
	*pp = p;
	*ps = s;
	*pparent = parent;
}

void handle_intermediate(DB *db, Page **pp);

void merge_intermediate(DB *db, Page **pp, Page **ps, Page **pparent) {
	Page *p = *pp, *s = *ps, *parent = *pparent;

	size_t p_vert_num = cast32(p->data, 0, 2);
	size_t s_vert_num = cast32(s->data, 0, 2);

	size_t p_key_offset = cast32(p->data, 0, 2 + p_vert_num);
	size_t s_key_offset = cast32(s->data, 0, 3);

	size_t p_size = cast32(p->data, p_key_offset, 0);
	size_t s_size = cast32(s->data, s_key_offset, 0);

	int n = memcmp((uint8_t *)p->data + p_key_offset + sizeof(uint32_t),
				   (uint8_t *)s->data + s_key_offset + sizeof(uint32_t), 
				   min(p_size, s_size));

	if (!n) { n = p_size - s_size; }

	DBT key, pkey;

	size_t l_link = s->number, ms_vert_num = p_vert_num, mp_vert_num = s_vert_num;
	Page *mp = s, *ms = p;
	if (n < 0) { l_link = p->number; mp = p; ms = s; ms_vert_num = s_vert_num; mp_vert_num = p_vert_num; }

	find_key_intermediate(parent, l_link, &pkey);

	/* merging */
	{
		size_t key_offset = cast32(ms->data, 0, 3);
		size_t link = cast32(ms->data, key_offset - sizeof(uint32_t), 0);
		insert_into_intermediate(db, &mp, mp_vert_num, &pkey, link);
		free(pkey.data);
	}
	for (size_t i = 1; i <= ms_vert_num; ++i) {
		size_t key_offset = cast32(ms->data, 0, 2 + i);
		key.size = cast32(ms->data, key_offset, 0);
		key.data = (void *)malloc(key.size);
		memcpy(key.data, (uint8_t *)ms->data + key_offset + sizeof(uint32_t), key.size);
		size_t link = cast32(ms->data, key_offset + key.size + sizeof(uint32_t), 0);

		insert_into_intermediate(db, &mp, mp_vert_num + i, &key, link);
		free(key.data);
	}
	free_index(db, ms->number);
	write_parents(db, mp);
	write_page(db, mp, mp->number);

	size_t parent_vert_num = cast32(parent->data, 0, 2);

	if (parent_vert_num == 1) {
		mp->kind = cPageRoot;
		memcpy(db->root, mp, db->parameters.page_size);
		write_page(db, mp, mp->number);
		free_index(db, parent->number);
	}
	else if (parent_vert_num > cPageNodesDown || (parent_vert_num <= cPageNodesDown && parent->kind == cPageRoot)) {
		delete_key_intermediate(db, &parent, l_link);
		write_page(db, parent, parent->number);
		if (parent->kind == cPageRoot) {
			memcpy(db->root, parent, db->parameters.page_size);
		}
	}
	else {
		delete_key_intermediate(db, &parent, l_link);
		handle_intermediate(db, &parent);
		free(mp); free(ms);
		return;
	}

	free(mp); 
	free(ms);
	free(parent);
}

void handle_intermediate(DB *db, Page **pp) {
	Page *p = *pp;
	size_t parent_index = cast32(p->data, 0, 1);

	void *raw_parent = (void *)malloc(db->parameters.page_size);
	read_page(db, raw_parent, parent_index);
	Page *parent = page_parse(raw_parent, db->parameters.page_size);

	size_t parent_vert_num = cast32(parent->data, 0, 2);

	uint32_t *links;

	find_links(parent, &links);

	size_t sibling_index = find_sibling(db, links, p->number, parent_vert_num + 1);

	void *raw_sibling = (void *)malloc(db->parameters.page_size);
	read_page(db, raw_sibling, sibling_index);
	Page *s = page_parse(raw_sibling, db->parameters.page_size);

	size_t sibling_vert_num = cast32(s->data, 0, 2);

	/* case if can redistribute */
	if (sibling_vert_num > cPageNodesDown) {
		redistribute_keys_intermediate(db, &p, &s, &parent);
		write_page(db, p, p->number);
		write_page(db, s, s->number);
		write_page(db, parent, parent->number);
		if (parent->kind == cPageRoot) {
			memcpy(db->root, parent, db->parameters.page_size);
		}
		free(p);
		free(s);
		free(parent);
		return;
	}

	/* merge case =( */
	if (sibling_vert_num == cPageNodesDown) {
		merge_intermediate(db, &p, &s, &parent);
		return;
	}
}

void merge_leaf(DB *db, Page **pp, Page **ps, Page **pparent) {
	Page *p = *pp, *s = *ps, *parent = *pparent;

	size_t p_vert_num = cast32(p->data, 0, 2);
	size_t s_vert_num = cast32(s->data, 0, 2);

	size_t p_key_offset = cast32(p->data, 0, 2 + p_vert_num);
	size_t s_key_offset = cast32(s->data, 0, 3);

	size_t p_size = cast32(p->data, p_key_offset, 0);
	size_t s_size = cast32(s->data, s_key_offset, 0);

	int n = memcmp((uint8_t *)p->data + p_key_offset + sizeof(uint32_t),
				   (uint8_t *)s->data + s_key_offset + sizeof(uint32_t), 
				   min(p_size, s_size));

	if (!n) { n = p_size - s_size; }

	DBT key, data;

	size_t l_link = s->number, m_vert_num = p_vert_num;
	Page *mp = s, *ms = p;
	if (n < 0) { l_link = p->number; mp = p; ms = s; m_vert_num = s_vert_num; }

	/* merging */
	for (size_t i = 0; i < m_vert_num; ++i) {
		size_t key_offset = cast32(ms->data, 0, 3 + i);
		key.size = cast32(ms->data, key_offset, 0);
		key.data = (void *)malloc(key.size);
		memcpy(key.data, (uint8_t *)ms->data + key_offset + sizeof(uint32_t), key.size);
		size_t data_offset = cast32(ms->data, key_offset + key.size + sizeof(uint32_t), 0);
		data.size = cast32(ms->data, data_offset, 0);
		data.data = (void *)malloc(data.size);
		memcpy(data.data, (uint8_t *)ms->data + data_offset + sizeof(uint32_t), data.size);
		size_t pos = find_pos(mp, &key);
		insert_into_leaf(db, &mp, pos, &key, &data);
		free(key.data);
		free(data.data);
	}
	free_index(db, ms->number);
	write_page(db, mp, mp->number);

	size_t parent_vert_num = cast32(parent->data, 0, 2);

	if (parent_vert_num == 1) {
		mp->kind = cPageRootLeaf;
		memcpy(db->root, mp, db->parameters.page_size);
		write_page(db, mp, mp->number);
		free_index(db, parent->number);
	}
	else if (parent_vert_num > cPageNodesDown || (parent_vert_num <= cPageNodesDown && parent->kind == cPageRoot)) {
		delete_key_intermediate(db, &parent, l_link);
		write_page(db, parent, parent->number);
		if (parent->kind == cPageRoot) {
			memcpy(db->root, parent, db->parameters.page_size);
		}
	}
	else {
		delete_key_intermediate(db, &parent, l_link);
		handle_intermediate(db, &parent);
		free(mp); free(ms);
		return;
	}

	free(mp);
	free(ms);
	free(parent);
}

int b_insert(DB *db, DBT *key, DBT *data) {
	if (!db->root) {
		create_root_leaf(db, key, data);
		return 0;
	}

	Page *p = bt_search(db, db->root, key);

	if (!p) { return -1; }

	size_t vert_num = cast32(p->data, 0, 2);

	/* case if key exists */
	{
		size_t offset = 3 * sizeof(uint32_t);
		for (size_t i = 0; i < vert_num; ++i) {
			size_t key_offset = cast32(p->data, offset, 0);
			size_t size = cast32(p->data, key_offset, 0);
			int n = memcmp((uint8_t *)p->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
			if (!n && size == key->size) {
				insert_same(db, &p, key, data);
				write_page(db, p, p->number);
				if (p->kind == cPageRootLeaf) { 
					memcpy(db->root, p, db->parameters.page_size);
				}
				free(p); p = NULL;
				return 0;
			}
			offset += sizeof(uint32_t);
		}
	}

	/* case when leaf is not full */
	if (vert_num < cPageNodesUp) {
		bool is_root = p == db->root;

		size_t pos = find_pos(p, key);

		insert_into_leaf(db, &p, pos, key, data);

		write_page(db, p, p->number);

		if (!is_root) { free(p); p = NULL; }
		else { db->root = p; }
		return 0;
	}

	size_t parent_index = cast32(p->data, 0, 1);

	if (!parent_index) {
		// root leaf full case
		Page *s = NULL;

		/* dividing lists part */
		size_t pos = find_pos(p, key);

		p->kind = cPageLeaf;

		divide_leaf(db, &p, &s, pos, key, data);

		/* creating and inserting into new root */
		Page *root = NULL;

		size_t skey_offset = cast32(s->data, 3 * sizeof(uint32_t), 0);

		DBT skey = {
			.size = cast32(s->data, skey_offset, 0),
			.data = (uint8_t *)s->data + skey_offset + sizeof(uint32_t)
		};

		create_root(db, &root, &skey, p->number, s->number);

		write32(p->data, 0, 1, root->number);
		write32(s->data, 0, 1, root->number);

		db->root = root;
		write32(db->meta->data, 0, 2, root->number);
		write_page(db, p, p->number);
		write_page(db, s, s->number);
		write_page(db, db->root, root->number);
		write_page(db, db->meta, 0);

		free(s);
		s = NULL;
		free(p);
		p = NULL;
		return 0;
	}

	void *raw = (void *)malloc(db->parameters.page_size);
	read_page(db, raw, parent_index);
	Page *parent = page_parse(raw, db->parameters.page_size);

	size_t parent_vert_num = cast32(parent->data, 0, 2);
	
	/* case when parent is not full */
	if (vert_num == cPageNodesUp && parent_vert_num < cPageNodesUp) {
		Page *s = NULL;

		/* dividing leafs part */
		size_t pos = find_pos(p, key);

		divide_leaf(db, &p, &s, pos, key, data);

		/* inserting into parent part */
		size_t skey_offset = cast32(s->data, 3 * sizeof(uint32_t), 0);

		DBT skey = {
			.size = cast32(s->data, skey_offset, 0),
			.data = (uint8_t *)s->data + skey_offset + sizeof(uint32_t)
		};

		pos = find_pos(parent, &skey);

		insert_into_intermediate(db, &parent, pos, &skey, s->number);

		write_page(db, p, p->number);
		write_page(db, s, s->number);
		write_page(db, parent, parent->number);

		if (parent->kind == cPageRoot) {
			memcpy(db->root, parent, db->parameters.page_size);
		}
		free(parent);
		parent = NULL;
		free(p);
		p = NULL;
		free(s);
		s = NULL;
		return 0;
	}

	/*case when parent is full too */
	if (vert_num == cPageNodesUp && parent_vert_num == cPageNodesUp) {
		Page *s = NULL;

		/* dividing leafs part */
		size_t pos = find_pos(p, key);

		divide_leaf(db, &p, &s, pos, key, data);

		/* inserting into parent part */
		size_t skey_offset = cast32(s->data, 3 * sizeof(uint32_t), 0);

		DBT skey = {
			.size = cast32(s->data, skey_offset, 0),
			.data = (uint8_t *)s->data + skey_offset + sizeof(uint32_t)
		};

		pos = find_pos(parent, &skey);

		write_page(db, p, p->number);
		write_page(db, s, s->number);

		divide_intermediate_process(db, &parent, pos, &skey, s->number);

		write_page(db, db->meta, 0);

		free(parent);
		parent = NULL;
		free(p);
		p = NULL;
		free(s);
		s = NULL;

		return 0;
	}

	return -1;
}

int b_delete(DB *db, DBT *key) {
	Page *p = bt_search(db, db->root, key);

	if (!p) { return -1; }

	size_t vert_num = cast32(p->data, 0, 2);

	/* case if key doesn't exists */
	{
		bool is_here = false;

		size_t offset = 3 * sizeof(uint32_t);
		for (size_t i = 0; i < vert_num; ++i) {
			size_t key_offset = cast32(p->data, offset, 0);
			size_t size = cast32(p->data, key_offset, 0);
			int n = memcmp((uint8_t *)p->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
			if (!n && size == key->size) {
				is_here = true;
				break;
			}
			offset += sizeof(uint32_t);
		}
		
		if (!is_here) {
			if (p != db->root) { free(p); p = NULL; } 
			return 0; 
		}
	}

	/* case if leaf not half full */
	if (vert_num > cPageNodesDown || p->kind == cPageRootLeaf) {
		delete_key_leaf(db, &p, key);
		write_page(db, p, p->number);
		if (p->kind == cPageRootLeaf) {
			memcpy(db->root, p, db->parameters.page_size);
		}
		free(p);
		return 0;
	}

	size_t parent_index = cast32(p->data, 0, 1);

	void *raw_parent = (void *)malloc(db->parameters.page_size);
	read_page(db, raw_parent, parent_index);
	Page *parent = page_parse(raw_parent, db->parameters.page_size);

	size_t parent_vert_num = cast32(parent->data, 0, 2);

	uint32_t *links;

	find_links(parent, &links);

	size_t sibling_index = find_sibling(db, links, p->number, parent_vert_num + 1);

	void *raw_sibling = (void *)malloc(db->parameters.page_size);
	read_page(db, raw_sibling, sibling_index);
	Page *sibling = page_parse(raw_sibling, db->parameters.page_size);

	size_t sibling_vert_num = cast32(sibling->data, 0, 2);

	/* case if can redistribute */
	if (vert_num == cPageNodesDown && sibling_vert_num > cPageNodesDown) {
		delete_key_leaf(db, &p, key);
		redistribute_keys_leaf(db, &p, &sibling, &parent);
		write_page(db, p, p->number);
		write_page(db, sibling, sibling->number);
		write_page(db, parent, parent->number);
		if (parent->kind == cPageRoot) {
			memcpy(db->root, parent, db->parameters.page_size);
		}
		free(p);
		free(sibling);
		free(parent);
		return 0;
	}

	/* merge case =( */
	if (vert_num == cPageNodesDown && sibling_vert_num == cPageNodesDown) {
		delete_key_leaf(db, &p, key);
		merge_leaf(db, &p, &sibling, &parent);
		return 0;
	}

	return -1;
}

#include "b-tree.h"
#include "types.h"
#include "storage.h"

#include <memory.h>
#include <stdlib.h>

// TODO make faster
Page *bt_search(const DB *db, Page *node, DBT *key) {
	if (!node) { return NULL; }
	if (node->kind == cPageRoot || node->kind == cPageIntermediate) {
		size_t vert_num = ((uint32_t *)((void *)node->data))[2];
		size_t offset = 3 * sizeof(uint32_t);
		for (size_t i = 0; i < vert_num; ++i) {
			size_t key_offset = ((uint32_t *)((uint8_t *)node->data + offset))[0];
			size_t size = ((uint32_t *)((uint8_t *)node->data + key_offset))[0];
			int n = memcmp((uint8_t *)node->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
			if (n > 0 || (!n && size > key->size)) {
				size_t link = ((uint32_t *)((uint8_t *)node->data + key_offset - sizeof(uint32_t)))[0];
				void *raw = (void *)malloc(db->parameters.page_size);
				read_page(db, raw, link);
				Page *next = page_parse(raw, db->parameters.page_size);
				Page *res = bt_search(db, next, key);
				if (res != next) { free(raw); raw = NULL; }
				return res;
			}
			offset += sizeof(uint32_t);
		}
		size_t begining = ((uint16_t *)((void *)node->data))[0];
		size_t link = ((uint32_t *)((uint8_t *)node->data + begining - sizeof(uint32_t)))[0];
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
	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	printf("vertex number = %u vertex kind = %x vertex parent = %u vertex size = %u\n", 
		p->number, p->kind, ((uint32_t *)((uint8_t *)p->data))[1], ((uint32_t *)((uint8_t *)p->data))[2]);
	fflush(stdout);

	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		if (p->kind == cPageRoot || p->kind == cPageIntermediate) {
			printf("%u/", ((uint32_t *)((uint8_t *)p->data + key_offset - sizeof(uint32_t)))[0]);
			fflush(stdout);
		}
		size_t size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
		write(1, (uint8_t *)p->data + key_offset + sizeof(uint32_t), size);
		printf("/");
		if (i == vert_num - 1 && (p->kind == cPageRoot || p->kind == cPageIntermediate)) {
			printf("%u", ((uint32_t *)((uint8_t *)p->data + key_offset + size + sizeof(uint32_t)))[0]);
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
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		size_t link = ((uint32_t *)((uint8_t *)p->data + key_offset - sizeof(uint32_t)))[0];

		read_page(db, raw, link);
		Page *next = page_parse(raw, db->parameters.page_size);

		print_tree(db, next);

		offset += sizeof(uint32_t);
	}

	size_t key_offset = ((uint16_t *)((uint8_t *)p->data))[0];
	size_t link = ((uint32_t *)((uint8_t *)p->data + key_offset - sizeof(uint32_t)))[0];
	
	read_page(db, raw, link);
	Page *next = page_parse(raw, db->parameters.page_size);

	print_tree(db, next);

	free(raw);
	raw = NULL;
}

int b_select(const DB *db, DBT *key, DBT *data) {
	Page *p = bt_search(db, db->root, key);

	//printf("==========================================================\n");
	//fflush(stdout);
	//print_tree(db, db->root);

	if (!p) {
		return -1; 
	}
	
	size_t vert_num = ((uint32_t *)((void *)p->data))[2];
	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		size_t size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
		int n = memcmp((uint8_t *)p->data + key_offset + sizeof(uint32_t), key->data, min(size, key->size));
		if (!n && size == key->size) {
			size_t data_offset = ((uint32_t *)((uint8_t *)p->data + key_offset + size + sizeof(uint32_t)))[0];
			data->size = ((uint32_t *)((uint8_t *)p->data + data_offset))[0];
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

			((uint32_t *)((uint8_t *)node_data + new_offset))[0] = begining;
			((uint32_t *)((uint8_t *)node_data + begining))[0] = key->size;
			memcpy((uint8_t *)node_data + begining + sizeof(uint32_t), key->data, key->size);
			((uint32_t *)((uint8_t *)node_data + begining + key->size + sizeof(uint32_t)))[0] = ending;
			

			((uint32_t *)((uint8_t *)node_data + ending))[0] = data->size;
			memcpy((uint8_t *)node_data + ending + sizeof(uint32_t), data->data, data->size);

			begining += 2 * sizeof(uint32_t) + key->size;
			
			new_offset += sizeof(uint32_t);
			continue;
		}

		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		((uint32_t *)((uint8_t *)node_data + new_offset))[0] = begining;

		size_t k_size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t data_offset = ((uint32_t *)((uint8_t *)p->data + key_offset + k_size + sizeof(uint32_t)))[0];
		size_t d_size = ((uint32_t *)((uint8_t *)p->data + data_offset))[0];

		ending -= d_size + sizeof(uint32_t);
		memcpy((uint8_t *)node_data + ending, (uint8_t *)p->data + data_offset, d_size + sizeof(uint32_t));
		((uint32_t *)((uint8_t *)node_data + begining + k_size + sizeof(uint32_t)))[0] = ending;
		begining += 2 * sizeof(uint32_t) + k_size;

		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	((uint16_t *)((void *)node_data))[0] = begining;
	((uint16_t *)((void *)node_data))[1] = ending;
}

void fill_intermediate(Page *p, Page *node, size_t l, size_t r, size_t pos, DBT *key, size_t link) {
	size_t begining = 3 * sizeof(uint32_t) + (r - l) * sizeof(uint32_t);
	
	size_t offset = 3 * sizeof(uint32_t) + l * sizeof(uint32_t);
	size_t new_offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;

	size_t old_key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
	size_t old_begining = old_key_offset - sizeof(uint32_t);

	((uint32_t *)((uint8_t *)node_data + begining))[0] = ((uint32_t *)((uint8_t *)p->data + old_begining))[0];
	begining += sizeof(uint32_t);
	for (size_t i = l; i < r; ++i) {
		if (pos == i) {
			((uint32_t *)((uint8_t *)node_data + new_offset))[0] = begining;
			((uint32_t *)((uint8_t *)node_data + begining))[0] = key->size;
			memcpy((uint8_t *)node_data + begining + sizeof(uint32_t), key->data, key->size);
			((uint32_t *)((uint8_t *)node_data + begining + key->size + sizeof(uint32_t)))[0] = link;

			begining += 2 * sizeof(uint32_t) + key->size;
			new_offset += sizeof(uint32_t);
			continue;
		}

		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		((uint32_t *)((uint8_t *)node_data + new_offset))[0] = begining;

		size_t k_size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t next_link = ((uint32_t *)((uint8_t *)p->data + key_offset + k_size + sizeof(uint32_t)))[0];
		((uint32_t *)((uint8_t *)node_data + begining + k_size + sizeof(uint32_t)))[0] = next_link;
		
		begining += 2 * sizeof(uint32_t) + k_size;
		offset += sizeof(uint32_t);
		new_offset += sizeof(uint32_t);
	}

	((uint16_t *)((void *)node_data))[0] = begining;
	((uint16_t *)((void *)node_data))[1] = 0;
}

void page_swap(Page **p, Page **q) {
	Page *swap = *p;
	*p = *q;
	*q = swap;
}

void insert_into_leaf(DB *db, Page **pp, size_t pos,DBT *key, DBT *data) {
	Page *p = *pp;

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	((uint32_t *)((void *)t->data))[1] = ((uint32_t *)((void *)p->data))[1];

	((uint32_t *)((uint8_t *)t->data))[2] = vert_num + 1;

	fill_leaf(db, p, t, 0, vert_num + 1, pos, key, data);

	*pp = t;
	free(p);
	p = NULL;
}

void divide_leaf(DB *db, Page **pp, Page **ps, size_t pos, DBT *key, DBT *data) {
	Page *s = page_create(db->parameters.page_size, cPageLeaf);
	Page *t = page_create(db->parameters.page_size, cPageLeaf);
	Page *p = *pp;

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	size_t tvert_num = (vert_num + 1) >> 1;
	size_t svert_num = vert_num - tvert_num + 1;

	/* copy meta from p to s */
	t->number = p->number;
	s->number = find_free_index(db);

	((uint32_t *)((void *)s->data))[1] = ((uint32_t *)((void *)p->data))[1];
	((uint32_t *)((void *)t->data))[1] = ((uint32_t *)((void *)p->data))[1];

	((uint32_t *)((void *)s->data))[2] = svert_num;
	((uint32_t *)((void *)t->data))[2] = tvert_num;

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

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];
	size_t offset = 3 * sizeof(uint32_t);
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		size_t link = ((uint32_t *)((uint8_t *)p->data + key_offset - sizeof(uint32_t)))[0];
		
		read_page(db, raw, link);
		Page *next = page_parse(raw, db->parameters.page_size);
		((uint32_t *)((uint8_t *)next->data))[1] = p->number;
		write_page(db, next, next->number);
		
		offset += sizeof(uint32_t);
	}
	size_t begining = ((uint16_t *)((void *)p->data))[0];
	size_t link = ((uint32_t *)((uint8_t *)p->data + begining - sizeof(uint32_t)))[0];

	read_page(db, raw, link);
	Page *next = page_parse(raw, db->parameters.page_size);
	((uint32_t *)((uint8_t *)next->data))[1] = p->number;
	write_page(db, next, next->number);

	free(raw); 
	raw = NULL;
}

void divide_intermediate(DB *db, Page **pp, Page **ps, size_t pos, DBT *key, size_t link, DBT *res) {
	Page *s = page_create(db->parameters.page_size, cPageIntermediate);
	Page *t = page_create(db->parameters.page_size, cPageIntermediate);
	Page *p = *pp;

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	size_t tvert_num = (vert_num + 1) >> 1;
	size_t svert_num = vert_num - tvert_num;

	/* copy meta from p to s */
	t->number = p->number;
	s->number = find_free_index(db);

	((uint32_t *)((void *)s->data))[1] = ((uint32_t *)((void *)p->data))[1];
	((uint32_t *)((void *)t->data))[1] = ((uint32_t *)((void *)p->data))[1];

	((uint32_t *)((void *)s->data))[2] = svert_num;
	((uint32_t *)((void *)t->data))[2] = tvert_num;

	/* insert verticies */
	fill_intermediate(p, t, 0, tvert_num, pos, key, link);
	size_t l = pos <= tvert_num ? tvert_num : (tvert_num + 1);
	size_t p_pos = pos == tvert_num ? 0 : pos;
	fill_intermediate(p, s, l, l + svert_num, p_pos, key, link);

	if (pos == tvert_num) {
		fflush(stdout);
		size_t key_offset = ((uint32_t *)((uint8_t *)s->data + 3 * sizeof(uint32_t)))[0];
		((uint32_t *)((uint8_t *)s->data + key_offset - sizeof(uint32_t)))[0] = link;
		res->size = key->size;
		res->data = (void *)malloc(res->size);
		memcpy(res->data, key->data, res->size);
	} else {
		if (pos < tvert_num) { tvert_num--; }
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + 3 * sizeof(uint32_t)))[tvert_num];
		res->size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
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

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	t->number = p->number;
	((uint32_t *)((void *)t->data))[1] = ((uint32_t *)((void *)p->data))[1];
	((uint32_t *)((uint8_t *)t->data))[2] = vert_num + 1;

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
	((uint16_t *)((void *)node_data))[0] = key->size + 3 * sizeof(uint32_t) + 3 * sizeof(uint32_t);
	((uint16_t *)((void *)node_data))[1] = db->parameters.page_size - cPagePadding - data->size - sizeof(uint32_t);
	/* parent */
	((uint32_t *)((void *)node_data))[1] = 0;

	/* number of vertices */
	size_t offset = 2 * sizeof(uint32_t);
	((uint32_t *)((uint8_t *)node_data + offset))[0] = 1;

	/* key offset array */
	offset += sizeof(uint32_t);
	((uint32_t *)((uint8_t *)node_data + offset))[0] = offset + sizeof(uint32_t);

	/* key { size, val, offset_data_link } */
	offset += sizeof(uint32_t);
	((uint32_t *)((uint8_t *)node_data + offset))[0] = key->size;
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)node_data + offset, key->data, key->size);
	offset += key->size;
	((uint32_t *)((uint8_t *)node_data + offset))[0] = db->parameters.page_size - cPagePadding - data->size - sizeof(uint32_t);

	/* data { size, val } */
	offset = db->parameters.page_size - cPagePadding - data->size - sizeof(uint32_t);
	((uint32_t *)((uint8_t *)node_data + offset))[0] = data->size;
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)node_data + offset, data->data, data->size);
	offset += data->size;

	/* finding page to store */
	size_t index = find_free_index(db);
	((uint32_t *)((void *)db->meta->data))[2] = index;
	db->root->number = index;
	write_page(db, db->root, index);
	write_page(db, db->index, 1);
	write_page(db, db->meta, 0);
}

void create_root(DB *db, Page **pp, DBT *key, size_t link_l, size_t link_r) {
	Page *p = page_create(db->parameters.page_size, cPageRoot);
	p->number = find_free_index(db);

	/* padding from begining and ending */
	((uint16_t *)((void *)p->data))[0] = key->size + 3 * sizeof(uint32_t) + 4 * sizeof(uint32_t);
	((uint16_t *)((void *)p->data))[1] = 0;
	/* parent */
	((uint32_t *)((void *)p->data))[1] = 0;

	/* number of vertices */
	size_t offset = 2 * sizeof(uint32_t);
	((uint32_t *)((uint8_t *)p->data + offset))[0] = 1;

	/* key offset array */
	offset += sizeof(uint32_t);
	((uint32_t *)((uint8_t *)p->data + offset))[0] = offset + 2 * sizeof(uint32_t);

	/* key { size, val, next_link } */
	offset += sizeof(uint32_t);
	((uint32_t *)((uint8_t *)p->data + offset))[0] = link_l;
	offset += sizeof(uint32_t);
	((uint32_t *)((uint8_t *)p->data + offset))[0] = key->size;
	offset += sizeof(uint32_t);
	memcpy((uint8_t *)p->data + offset, key->data, key->size);
	offset += key->size;
	((uint32_t *)((uint8_t *)p->data + offset))[0] = link_r;
	*pp = p;
}

// TODO make faster
size_t find_pos(Page *p, DBT *key) {
	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];
	size_t offset = 3 * sizeof(uint32_t);
	size_t pos = 0;
	for (; pos < vert_num; ++pos) {
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		size_t size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
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

	size_t parent_index = ((uint32_t *)((uint8_t *)p->data))[1];

	if (parent_index) {
		write_page(db, p, p->number);
		write_page(db, s, s->number);

		void *raw = (void *)malloc(db->parameters.page_size);
		read_page(db, raw, parent_index);
		parent = page_parse(raw, db->parameters.page_size);

		size_t parent_vert_num = ((uint32_t *)((uint8_t *)parent->data))[2];

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

		((uint32_t *)((uint8_t *)p->data))[1] = parent->number;
		((uint32_t *)((uint8_t *)s->data))[1] = parent->number;

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
	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];
	size_t begining = 3 * sizeof(uint32_t) + vert_num * sizeof(uint32_t);
	size_t ending = db->parameters.page_size - cPagePadding;

	size_t offset = 3 * sizeof(uint32_t);
	void *node_data = node->data;
	for (size_t i = 0; i < vert_num; ++i) {
		size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
		((uint32_t *)((uint8_t *)node_data + offset))[0] = begining;

		size_t k_size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
		memcpy((uint8_t *)node_data + begining, (uint8_t *)p->data + key_offset, k_size + sizeof(uint32_t));

		size_t data_offset = ((uint32_t *)((uint8_t *)p->data + key_offset + k_size + sizeof(uint32_t)))[0];
		size_t d_size = ((uint32_t *)((uint8_t *)p->data + data_offset))[0];

		if (!memcmp(key->data, (uint8_t *)p->data + key_offset + sizeof(uint32_t), min(key->size, k_size)) && key->size == k_size) {
			ending -= data->size + sizeof(uint32_t);
			((uint32_t *)((uint8_t *)node_data + ending))[0] = data->size;
			memcpy((uint8_t *)node_data + ending + sizeof(uint32_t), data->data, data->size);
		} else {
			ending -= d_size + sizeof(uint32_t);
			memcpy((uint8_t *)node_data + ending, (uint8_t *)p->data + data_offset, d_size + sizeof(uint32_t));
		}

		((uint32_t *)((uint8_t *)node_data + begining + k_size + sizeof(uint32_t)))[0] = ending;
		begining += 2 * sizeof(uint32_t) + k_size;

		offset += sizeof(uint32_t);
	}

	((uint16_t *)((void *)node_data))[0] = begining;
	((uint16_t *)((void *)node_data))[1] = ending;
}

void insert_same(DB *db, Page **pp, DBT *key, DBT *data) {
	Page *p = *pp;

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	Page *t = page_create(db->parameters.page_size, p->kind);
	t->number = p->number;

	((uint32_t *)((void *)t->data))[1] = ((uint32_t *)((void *)p->data))[1];

	((uint32_t *)((uint8_t *)t->data))[2] = vert_num;

	fill_leaf_same(db, p, t, key, data);

	*pp = t;
	free(p);
	p = NULL;
}

int b_insert(DB *db, DBT *key, DBT *data) {
	if (!db->root) {
		create_root_leaf(db, key, data);
		return 0;
	}

	Page *p = bt_search(db, db->root, key);

	if (!p) { return -1; }

	size_t vert_num = ((uint32_t *)((uint8_t *)p->data))[2];

	/* case if key exists */
	{
		size_t offset = 3 * sizeof(uint32_t);
		for (size_t i = 0; i < vert_num; ++i) {
			size_t key_offset = ((uint32_t *)((uint8_t *)p->data + offset))[0];
			size_t size = ((uint32_t *)((uint8_t *)p->data + key_offset))[0];
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

	size_t parent_index = ((uint32_t *)((uint8_t *)p->data))[1];

	if (!parent_index) {
		// root leaf full case
		Page *s = NULL;

		/* dividing lists part */
		size_t pos = find_pos(p, key);

		p->kind = cPageLeaf;

		divide_leaf(db, &p, &s, pos, key, data);

		/* creating and inserting into new root */
		Page *root = NULL;

		size_t skey_offset = ((uint32_t *)((uint8_t *)s->data + 3 * sizeof(uint32_t)))[0];

		DBT skey = {
			.size = ((uint32_t *)((uint8_t *)s->data + skey_offset))[0],
			.data = (uint8_t *)s->data + skey_offset + sizeof(uint32_t)
		};

		create_root(db, &root, &skey, p->number, s->number);

		((uint32_t *)((void *)p->data))[1] = root->number;
		((uint32_t *)((void *)s->data))[1] = root->number;

		db->root = root;
		((uint32_t *)((void *)db->meta->data))[2] = root->number;
		write_page(db, p, p->number);
		write_page(db, s, s->number);
		write_page(db, db->root, root->number);
		write_page(db, db->index, 1);
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

	size_t parent_vert_num = ((uint32_t *)((uint8_t *)parent->data))[2];
	
	/* case when parent is not full */
	if (vert_num == cPageNodesUp && parent_vert_num < cPageNodesUp) {
		Page *s = NULL;

		/* dividing leafs part */
		size_t pos = find_pos(p, key);

		divide_leaf(db, &p, &s, pos, key, data);

		/* inserting into parent part */
		size_t skey_offset = ((uint32_t *)((uint8_t *)s->data + 3 * sizeof(uint32_t)))[0];

		DBT skey = {
			.size = ((uint32_t *)((uint8_t *)s->data + skey_offset))[0],
			.data = (uint8_t *)s->data + skey_offset + sizeof(uint32_t)
		};

		pos = find_pos(parent, &skey);

		insert_into_intermediate(db, &parent, pos, &skey, s->number);

		write_page(db, p, p->number);
		write_page(db, s, s->number);
		write_page(db, parent, parent->number);
		write_page(db, db->index, 1);

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
		size_t skey_offset = ((uint32_t *)((uint8_t *)s->data + 3 * sizeof(uint32_t)))[0];

		DBT skey = {
			.size = ((uint32_t *)((uint8_t *)s->data + skey_offset))[0],
			.data = (uint8_t *)s->data + skey_offset + sizeof(uint32_t)
		};

		pos = find_pos(parent, &skey);

		write_page(db, p, p->number);
		write_page(db, s, s->number);

		divide_intermediate_process(db, &parent, pos, &skey, s->number);

		//write_page(db, parent, parent->number);
		write_page(db, db->index, 1);
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

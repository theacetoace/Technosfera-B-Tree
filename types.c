#include "types.h"

const uint32_t cPageMagic      = 0xfacabada;
const uint32_t cPageRestOffset = 3 * sizeof(uint32_t);

const PageNumber cPageInvalid = 0xfefefefe;


const uint32_t cPageMetaData     = 0xffffffffUL;
const uint32_t cPageIndex        = 0xeeeeeeeeUL;
const uint32_t cPageLeaf         = 0xddddddd4UL;
const uint32_t cPageIntermediate = 0xddddddd1UL;
const uint32_t cPageRoot         = 0xddddddd2UL;
const uint32_t cPageRootLeaf     = 0xddddddd6UL;

const uint32_t cPageNodesDown = 4;
const uint32_t cPageNodesUp = 8;
const uint32_t cPagePadding = 20;

uint32_t cast32(void *a, uint32_t offset, uint32_t ind) {
	uint8_t *v = (uint8_t *)a + offset + ind * sizeof(uint32_t);
	return v[0] | (v[1] << 8) | (v[2] << 16) | (v[3] << 24);
}

uint32_t cast16(void *a, uint32_t offset, uint32_t ind) {
	uint8_t *v = (uint8_t *)a + offset + ind * sizeof(uint16_t);
	return v[0] | (v[1] << 8);
}

void write32(void *a, uint32_t offset, uint32_t ind, uint32_t what) {
	uint8_t *v = (uint8_t *)a + offset + ind * sizeof(uint32_t);
	v[0] = what & 0xff;
	v[1] = (what >> 8) & 0xff;
	v[2] = (what >> 16) & 0xff;
	v[3] = (what >> 24) & 0xff;
}

void write16(void *a, uint32_t offset, uint32_t ind, uint16_t what) {
	uint8_t *v = (uint8_t *)a + offset + ind * sizeof(uint16_t);
	v[0] = what & 0xff;
	v[1] = (what >> 8) & 0xff;
}

#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	void  *data;
	uint32_t size;
} DBT;

typedef uint32_t PageSize;
typedef uint32_t PageNumber;


extern const uint32_t cPageMagic;
extern const uint32_t cPageRestOffset;

extern const PageNumber cPageInvalid;


typedef uint32_t PageKind;
extern const uint32_t cPageMetaData;
extern const uint32_t cPageIndex;
extern const uint32_t cPageLeaf;
extern const uint32_t cPageIntermediate;
extern const uint32_t cPageRoot;
extern const uint32_t cPageRootLeaf;

extern const uint32_t cPageNodesDown;
extern const uint32_t cPageNodesUp;
extern const uint32_t cPagePadding;

#ifndef min
#define min(a, b) (((a)<(b))?(a):(b))
#endif

extern uint32_t cast32(void *a, uint32_t offset, uint32_t ind);

extern uint32_t cast16(void *a, uint32_t offset, uint32_t ind);

extern void write32(void *a, uint32_t offset, uint32_t ind, uint32_t what);

extern void write16(void *a, uint32_t offset, uint32_t ind, uint16_t what);

#endif // TYPES_H

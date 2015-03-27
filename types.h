#pragma once


#include <stdbool.h>
#include <stdint.h>

typedef struct {
	void  *data;
	size_t size;
} DBT;

typedef uint32_t PageSize;
typedef uint32_t PageNumber;


static const uint32_t cPageMagic      = 0xfacabada;
static const uint32_t cPageRestOffset = 3 * sizeof(uint32_t);

static const PageNumber cPageInvalid = 0xfefefefe;


typedef uint32_t PageKind;
static const uint32_t cPageMetaData     = 0xffffffffUL;
static const uint32_t cPageIndex        = 0xeeeeeeeeUL;
static const uint32_t cPageLeaf         = 0xddddddd4UL;
static const uint32_t cPageIntermediate = 0xddddddd1UL;
static const uint32_t cPageRoot         = 0xddddddd2UL;
static const uint32_t cPageRootLeaf     = 0xddddddd6UL;

static const uint32_t cPageNodesDown = 4;
static const uint32_t cPageNodesUp = cPageNodesDown << 1;
static const uint32_t cPagePadding = 20;

#ifndef min
#define min(a, b) (((a)<(b))?(a):(b))
#endif

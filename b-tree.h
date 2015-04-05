#ifndef B_TREE_H
#define B_TREE_H

#include <stdio.h>
#include "page.h"
#include "types.h"
#include "mydb.h"

int b_select(const DB *, DBT *, DBT *);

int b_insert(DB *, DBT *, DBT *);

int b_delete(DB *, DBT *);

#endif // B_TREE_H

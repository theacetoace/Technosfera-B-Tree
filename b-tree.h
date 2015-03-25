#pragma once

#include <stdio.h>
#include "page.h"
#include "types.h"
#include "mydb.h"

int b_select(const DB *, DBT *, DBT *);

int b_insert(DB *, DBT *, DBT *);

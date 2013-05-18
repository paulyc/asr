//
//  mmalloc.cpp
//  mac
//
//  Created by Paul Ciarlo on 5/18/13.
//  Copyright (c) 2013 Paul Ciarlo. All rights reserved.
//

#include "mmalloc.h"

void do_stuff(void *m, size_t by, const char *f, int l)
{
    _malloc_map[m].bytes = by;
	_malloc_map[m].file = f;
	_malloc_map[m].line = l;
}

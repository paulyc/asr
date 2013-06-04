// ASR - Digital Signal Processor
// Copyright (C) 2002-2013  Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __mac__mmalloc__
#define __mac__mmalloc__

#include <cstdlib>
#include <unordered_map>

extern bool destructing_hash;

struct alloc_info
{
	alloc_info(){}
	alloc_info(size_t b, const char *f, int l) :
    bytes(b), file(f), line(l){}
	size_t bytes;
	const char *file;
	int line;
};

class my_hash_map
{
public:
    my_hash_map()
    {
        for (int i=0; i<1024; ++i)
            tbl[i] = 0;
    }
	~my_hash_map()
	{
		destructing_hash = true;
        for (int i=0; i<1024; ++i)
        {
            node *n = tbl[i];
            while (n)
            {
                void *free_node = n;
                n = n->next;
                free(free_node);
            }
        }
	}
    alloc_info &operator[](void *m)
    {
        const int indx = (size_t(m) >> 4) % 1024;
        node **n = &(tbl[indx]);
        while (*n && (*n)->it != m)
        {
            n = &(*n)->next;
        }
        if (!*n)
        {
            *n = (node*)malloc(sizeof(node));
            (*n)->it = m;
            (*n)->next = 0;
        }
        return (*n)->info;
    }
    void erase(void *m)
    {
        const int indx = (size_t(m) >> 4) % 1024;
        node **n = &(tbl[indx]);
        while (*n)
        {
            if ((*n)->it == m)
            {
                void *free_node = *n;
                *n = (*n)->next;
                free(free_node);
                break;
            }
            n = &(*n)->next;
        }
    }
    void dump()
    {
        for (int i=0; i<1024; ++i)
        {
            node *n = tbl[i];
            while (n)
            {
                printf("%p allocated %s:%d bytes %d\n", n->it,
                   n->info.file,
                   n->info.line,
                   n->info.bytes);
                n = n->next;
            }
        }
    }
private:
    struct node
    {
        void *it;
        alloc_info info;
        node *next;
    };
    node *tbl[1024];
};

extern my_hash_map _malloc_map;

#endif /* defined(__mac__mmalloc__) */

#ifndef WFX_ARRAY_TREE_H
#define WFX_ARRAY_TREE_H

#include <nginx.h>
#include <ngx_http.h>

typedef struct {
  unsigned           min;
  unsigned           max;
} arraytree_range_t;

typedef struct {
  size_t             chunklen;
  size_t             itemsize;
  int                cur;
  rbtree_seed_t      tree;
  arraytree_chunk_t *first;
} arraytree_t;

typedef struct {
  arraytree_range_t  range;
  
  int                refcount;
  void              *chunkdata;
  char               data[1];
} arraytree_chunk_t;


ngx_int_t arraytree_init(arraytree_t *at, size_t chunklen, size_t datasize);

ngx_int_t arraytree_empty(arraytree_t *at);

ngx_int_t arraytree_set(arraytree_t *at, unsigned index, void *data);
ngx_int_t arraytree_unset(arraytree_t *at, unsigned index);

void *arraytree_each(arraytree_t *at, unsigned *index);

#endif //WFX_ARRAY_TREE_H

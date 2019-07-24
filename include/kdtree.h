
#ifndef __KDTREE_H__
#define __KDTREE_H__

#include "ocr.h"

struct knode_st {
    unsigned int coord[2];
    entry_t *entry;
    struct knode_st *left, *right;
};

typedef struct knode_st knode_t;

knode_t *kd_create(entry_t **entries, unsigned int nb_entries, unsigned int depth);

#endif


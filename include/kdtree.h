
#ifndef __KDTREE_H__
#define __KDTREE_H__

#define KD_INODE 1
#define KD_LEAF 2

struct knode_st;
typedef struct knode_st knode_t;

#include "ocr.h"

struct kinode_st {
    unsigned int coord[2];
    entry_t *entry;
    knode_t *left, *right;
};

struct kleaf_st {
	entry_t **entries;
	unsigned int nb_entries;
};

struct knode_st {
	/* Either INODE or LEAF */
	unsigned char type;
	union {
		struct kinode_st inode;
		struct kleaf_st leaf; 
	} node;
};

knode_t *kd_create(entry_t **entries, unsigned int nb_entries, unsigned int depth);
void kd_search(knode_t *node, img_t *img, entry_t **best, float *best_dist);

#endif


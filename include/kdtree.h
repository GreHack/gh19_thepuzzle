#ifndef __KDTREE_H__
#define __KDTREE_H__

#define KD_INODE 1
#define KD_LEAF 2

struct knode_st;
typedef struct knode_st knode_t;

#include "packed/ocr.h"

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
	/* either INODE or LEAF */
	unsigned char type;
	/* pointer of the global list of entries to be able to free it
	   at exit - this pointer, if not NULL, is the same for all kd nodes */
	entry_t **entries;
	union {
		struct kinode_st inode;
		struct kleaf_st leaf; 
	} node;
};

#ifndef KD_LOAD
/* create a kd tree from a list of entries and a max depth */
knode_t *kd_create(entry_t **entries, unsigned int nb_entries, unsigned int depth);
#endif

/* find the closest element to img in the kd tree
 * outputs a pointer to the closest element and the distance (img, best)
 */
void kd_search(knode_t *node, img_t *img, entry_t **best, float *best_dist);

void kd_free(knode_t *tree, bool free_entries);

#ifdef KD_DUMP
/* save kd structure in a file */
void kd_dump(knode_t *tree, FILE *file);
#endif

#ifdef KD_LOAD
/* get a file descriptor to read kd from executable */
FILE *kd_get_fd(char *path);
/* load kd structure from a file */
knode_t *kd_load(FILE *file);
/* decrypt kd data */
void kd_decrypt(FILE *fd);
#endif

#if TEST_KD
void kd_test(knode_t *k, img_t *img);
#endif

#endif

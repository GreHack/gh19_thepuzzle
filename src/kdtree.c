
#define _GNU_SOURCE

#include "kdtree.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "img.h"
#include "packed/ocr.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define KD_DEPTH 10

int kd_compare(entry_t **e1, entry_t **e2, unsigned int *depth)
{
	unsigned int h = (*e1)->img->h;
	unsigned int x, y;
	x = *depth/h;
	y = *depth % h;
	pix_t p1 = (*e1)->img->pix[x][y];
	pix_t p2 = (*e2)->img->pix[x][y];
	return (p1 < p2)?-1:((p1 == p2)?0:1);
}

knode_t *kd_create(entry_t **entries, unsigned int nb_entries, unsigned int depth)
{
	if (!entries || nb_entries == 0) {
		return NULL;
	}
	unsigned int h = entries[0]->img->h;
	knode_t *node = (knode_t *) malloc(sizeof(knode_t));
	if (depth == KD_DEPTH) {
		/* Max depth reached - storing the rest in leaf node */		
		node->type = KD_LEAF;
		node->node.leaf.entries = entries;
		node->node.leaf.nb_entries = nb_entries;
	} else {
		/* sort entries relatively to pixel corresponding to depth */	 
		node->type = KD_INODE;
		qsort_r((void *) entries, nb_entries, sizeof(entry_t *), (int (*)(const void *, const void *, void *)) kd_compare, &depth);
		int median = (signed int) nb_entries/2;
		node->node.inode.entry = entries[median];
		node->node.inode.coord[0] = depth / h;
		node->node.inode.coord[1] = depth % h;
		node->node.inode.left = kd_create(entries, MAX(median - 1, 0), depth + 1);
		node->node.inode.right = kd_create(entries + median, MAX(((signed int) nb_entries) - median - 1, 0), depth + 1);
	}
	return node;
}

void kd_dump_inode(struct kinode_st *node, FILE *file)
{
	/* write node type */
	fwrite("I", sizeof(char), 1, file);
	/* write coordinates */
	char c;
	c = (char) node->coord[0];
	fwrite(&c, sizeof(char), 1, file);
	c = (char) node->coord[1];
	fwrite(&c, sizeof(char), 1, file);
	/* dump entry */
	ocr_dump_entry(node->entry, file);
}

void kd_dump_leaf(struct kleaf_st *node, FILE *file)
{
	/* write node type */
	fwrite("L", sizeof(char), 1, file);
	/* write number of entries */ 
	fwrite(&(node->nb_entries), sizeof(unsigned int), 1, file);
	/* dump entries */
	for (unsigned int i = 0; i < node->nb_entries; i++)
		ocr_dump_entry(node->entries[i], file);
}

void kd_dump(knode_t *node, FILE *file)
{
	if (node->type == KD_LEAF) {
		kd_dump_leaf(&(node->node.leaf), file);
	} else {
		kd_dump_inode(&(node->node.inode), file);
		kd_dump(node->node.inode.left, file);
		kd_dump(node->node.inode.right, file);
	}
}

knode_t *kd_load_inode(FILE *file)
{
	knode_t *node = (knode_t *) malloc(sizeof(knode_t));
	node->type = KD_INODE;
	node->node.inode.coord[0] = 0;
	node->node.inode.coord[1] = 0;
	/* read coordinates */
	fread(&(node->node.inode.coord[0]), sizeof(char), 1, file);
	fread(&(node->node.inode.coord[1]), sizeof(char), 1, file);
	/* read entry */
	node->node.inode.entry = ocr_load_entry(file);
#ifdef DEBUG_KD
	if (node->node.inode.entry->img->h <= node->node.inode.coord[0])
		exit(1);
	if (node->node.inode.entry->img->w <= node->node.inode.coord[1])
		exit(1);
#endif
	node->node.inode.left = NULL;
	node->node.inode.right = NULL;
	return node;
}

knode_t *kd_load_leaf(FILE *file)
{
	knode_t *node = (knode_t *) malloc(sizeof(knode_t));
	node->type = KD_LEAF;
	/* read number of entries */ 
	fread(&(node->node.leaf.nb_entries), sizeof(unsigned int), 1, file);
	/* allocate entries */
	node->node.leaf.entries = (entry_t **) malloc(sizeof(entry_t *) * node->node.leaf.nb_entries);
	/* read entries */
	for (unsigned int i = 0; i < node->node.leaf.nb_entries; i++)
		node->node.leaf.entries[i] = ocr_load_entry(file);
	return node;
}

knode_t *kd_load(FILE *file)
{
	char type;
	knode_t *kn;
	/* Read type of node */
	fread(&type, sizeof(char), 1, file);
	switch (type) {
	case 'I':
		kn = kd_load_inode(file);
		kn->node.inode.left = kd_load(file);
		kn->node.inode.right = kd_load(file);
		break;
	case 'L':
		kn = NULL;
		kn = kd_load_leaf(file);
		break;
	default:
		exit(1);
	}
	return kn;
}

void kd_search(knode_t *node, img_t *img, entry_t **best, float *best_dist)
{
	float d;
	if (node->type == KD_LEAF) {
		/* This is a final node of the tree */
		for (unsigned int i = 0; i < node->node.leaf.nb_entries; i++) {
			d = img_dist(node->node.leaf.entries[i]->img, img);
			if (d <= *best_dist) {
				*best = node->node.leaf.entries[i];
				*best_dist = d;
			}
		}
	} else {
		if (!node->node.inode.left || !node->node.inode.right) {
			exit(1);
		}
		unsigned int h, w;
		h = node->node.inode.coord[0];
		w = node->node.inode.coord[1];
		pix_t node_pix = node->node.inode.entry->img->pix[h][w];
		pix_t entry_pix = img->pix[h][w];
		/* This is an internal node */
		/* Compute distance between curr node and entry */
		d = img_dist(node->node.inode.entry->img, img);
		if (d < *best_dist) {
			*best_dist = d;
			*best = node->node.inode.entry;
		}
		if (pow(entry_pix - node_pix, 2) <= *best_dist) {
			/* Continue exploration left */
			kd_search(node->node.inode.left, img, best, best_dist);
		}
		if (pow(entry_pix - node_pix, 2) <= *best_dist) {
			/* Continue exploration right */
			kd_search(node->node.inode.right, img, best, best_dist);
		}
	}
}


#define _GNU_SOURCE

#include "kdtree.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "img.h"
#include "packed/ocr.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define KD_DEPTH 100

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
	unsigned int w = entries[0]->img->w;
	knode_t *node = (knode_t *) malloc(sizeof(knode_t));
	if (depth <= h * w) {
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

#if 1
void kd_search(knode_t *node, img_t *img, entry_t **best, float *best_dist)
{
	float d;
	if (node->type == KD_LEAF) {
		/* This is a final node of the tree */
		// TODO parcourir les candidats
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
#endif

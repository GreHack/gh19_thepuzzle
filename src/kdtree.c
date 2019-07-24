
#define _GNU_SOURCE

#include "kdtree.h"

#include <stdio.h>
#include <stdlib.h>

#include "ocr.h"

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
    return (p1.r < p2.r)?-1:((p1.r == p2.r)?0:1);
}

knode_t *kd_create(entry_t **entries, unsigned int nb_entries, unsigned int depth)
{
    if (!entries || nb_entries == 0) {
        return NULL;
    }
    /* sort entries relatively to pixel corresponding to depth */     
    qsort_r((void *) entries, nb_entries, sizeof(entry_t *), (int (*)(const void *, const void *, void *)) kd_compare, &depth);
    int median = (signed int) nb_entries/2;
    knode_t *node = (knode_t *) malloc(sizeof(knode_t));
    node->entry = entries[median];
    node->coord[0] = depth / (entries[0]->img->h);
    node->coord[1] = depth % (entries[0]->img->h);
    node->left = kd_create(entries, MAX(median - 1, 0), depth + 1);
    node->right = kd_create(entries + median, MAX(((signed int) nb_entries) - median - 1, 0), depth + 1);
    return node;
}

#ifndef __IMG_H__
#define __IMG_H__

#include "global.h"
#include <stdio.h>

typedef /* struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} */ unsigned char pix_t;

typedef struct pix_list pix_list_t;

struct pix_list {
	unsigned int h;
	unsigned int w;
	pix_list_t *nxt;
	pix_list_t *prev;
};

typedef struct {
	/* array of h entries of the type pix_t * ;
	   each of these entries is an array of pix_t of size w */
	pix_t **pix;
	/* linked-list of pointers to white pixels */
	pix_list_t *wpix;	
	unsigned int nb_wpix;
	unsigned int h;
	unsigned int w;
} img_t;

pix_t img_rgb_to_pix(pix_t *pix, unsigned char r, unsigned char g, unsigned char b);
void img_set_pix(img_t *img, unsigned int h, unsigned int w, pix_t pix);
void img_set_pix_rgb(img_t *img, unsigned int h, unsigned int w, unsigned char r, unsigned char g, unsigned char b);
void img_free(img_t *img);
img_t *img_crop(img_t *img, unsigned int h, unsigned int w, unsigned int dh, unsigned int dw, unsigned int *nb_pix);
img_t *img_reduce(img_t *img, unsigned int ratio);
pix_t **img_allocate_pixels(unsigned int h, unsigned int w);
#ifdef DEBUG
void img_to_file(img_t *img, char *filepath);
void img_show_cli(img_t *img); 
#endif
float img_dist(img_t *i1, img_t *i2);
img_t *img_center(img_t *img);

#ifdef KD_DUMP
void img_dump(img_t *img, FILE *file);
#endif

#ifdef KD_LOAD
img_t *img_load(FILE *file);
#endif

#endif

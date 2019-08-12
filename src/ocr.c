
#include "ocr.h"

/* This will confuse people */
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "img.h"

#if KD_TREE
#include "kdtree.h"
#endif

#define MGC_LABEL	0x00000801
#define MGC_DATA	0x00000803

#define DEBUG 1


/* Read unsigned int from file (big endian) */
unsigned int ocr_read_uint(FILE *fd)
{
	unsigned int read_val;
	if (fread(&read_val, sizeof(unsigned int), 1, fd) != 1) {
		exit(1);
	}
	return htonl(read_val);
}


/* Check the magic number the beginning of a file */
bool ocr_check_magic(FILE *fd, unsigned int magic)
{
	return ocr_read_uint(fd) == magic;
}


pix_t **ocr_allocate_pixels(unsigned int h, unsigned int w)
{
	pix_t **pix = (pix_t **) malloc(sizeof(pix_t *) * h);
	for (unsigned int i = 0; i < h; i++) {
		pix[i] = (pix_t *) malloc(sizeof(pix_t) * w);
	}
	return pix;
}


/* Read one entry for the data file to train the OCR */
entry_t *ocr_read_one_entry(FILE *flabel, FILE *fdata, unsigned int h, unsigned int w)
{
	unsigned int nb_read = 0;
	img_t *img = (img_t *) malloc(sizeof(img_t));
	img->h = h;
	img->w = w;
	/* allocate pixels */
	img->pix = ocr_allocate_pixels(h, w);
	for (unsigned int dh = 0; dh < h; dh++) {
		for (unsigned int dw = 0; dw < w; dw++) {
			/* read pixel */
			unsigned char pixval;
			pixval = (char) fgetc(fdata);
			/* set pixel in structure */
			img->pix[dh][dw] = pixval;
			nb_read += 1;
		}
	}
	entry_t *entry = (entry_t *) malloc(sizeof(entry_t));
	entry->img = img;
	entry->label = '0' + (char) fgetc(flabel);
	return entry;
}


/* Debug mode - display image in CLI */
#if DEBUG
void ocr_show_img_cli(img_t *img)
{
	for(unsigned int i = 0; i < img->h; i++) {
		for (unsigned int j = 0; j < img->w; j++) {
			if (img->pix[i][j] > 127) {
				printf("x");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
	return;
}
#endif


/* Compute distance between two images (euclidian) */
/* CAUTION: returns square of dist */
float ocr_dist(img_t *i1, img_t *i2)
{
	float dist = 0;
	if (i1->h != i2->h || i1->w != i2->w) {
		exit(1);
	}
	for (unsigned int i = 0; i < i1->h; i++) {
		for (unsigned int j = 0; j < i1->w; j++) {
			dist += pow(i1->pix[i][j] - i2->pix[i][j], 2.0);
		}
	}
	return dist;
}


#if !KD_TREE
char ocr_nearest_array(ocr_t *ocr, img_t *img)
{
	float min_dist = INFINITY;
	float dist;
	entry_t *closest = NULL;
	for (unsigned int i = 0; i < ocr->nb_entries; i++) {
		dist = ocr_dist(img, ocr->entries[i]->img);
		if (dist < min_dist) {
			min_dist = dist;
			closest = ocr->entries[i];
		}
	}
#if DEBUG
	ocr_show_img_cli(img);
	printf("recognized: %c\n", closest->label);
	ocr_show_img_cli(closest->img);
#endif
	if (dist < 4000000)
		return '!';
	return closest->label;
}

#else

char ocr_nearest_kd(ocr_t *ocr, img_t *img) // entry_t **entries, unsigned int nb_entries, img_t *img)
{
	entry_t *best;
	float dist;
	kd_search(ocr, img, &best, &dist);
#if DEBUG
	ocr_show_img_cli(img);
	printf("recognized: %c\n", best->label);
	printf("dist: %f\n", ocr_dist(best->img, img));
	ocr_show_img_cli(best->img);
#endif
	return best->label;
}

#endif

/* Find the nearest neighbor of the image given as a parameter in the 
   data set of the OCR */
char ocr_recognize(ocr_t *ocr, img_t *img)
{
#if KD_TREE
	return ocr_nearest_kd(ocr, img);
#else 
	return ocr_nearest_array(ocr, img);
#endif
}

ocr_t *ocr_train(char *label_path, char *data_path)
{
	unsigned int nb_label, nb_data;
	FILE *flabel = fopen(label_path, "rb");
	FILE *fdata = fopen(data_path, "rb");
	entry_t **entries;
	/* check file opening */
	if (!fdata || !flabel) {
		exit(1);
	}
	/* check magic number */
	if (!ocr_check_magic(flabel, MGC_LABEL)) {
		exit(1);
	}
	if (!ocr_check_magic(fdata, MGC_DATA)) {
		exit(1);
	}
	/* read number of label and data */
	nb_label = ocr_read_uint(flabel);
	nb_data = ocr_read_uint(fdata);
	/* allocate entries table */
	entries = (entry_t **) malloc(sizeof(entry_t *) * nb_label);
	/* assert consistency */
	if (nb_label != nb_data) {
		exit(1);
	}
	/* read size of images */
	unsigned int h, w;
	w = ocr_read_uint(fdata);
	h = ocr_read_uint(fdata);
	/* construct data pool */
#if DEBUG
	nb_label -= 10;
#endif
	for (unsigned int i = 0; i < nb_label; i++) {
		entries[i] = ocr_read_one_entry(flabel, fdata, h, w);
		// ocr_show_img_cli(entries[i]->img);
		// printf("label: %c\n", entries[i]->label);
	}
#if KD_TREE
	ocr_t *ocr = kd_create(entries, nb_label, 0);
#else
	ocr_t *ocr = malloc(sizeof(ocr_t));
	ocr->nb_entries = nb_label;
	ocr->entries = entries;
#endif
#if DEBUG
	for (unsigned int i = 0; i < 10; i++) {
		entry_t *entry = ocr_read_one_entry(flabel, fdata, h, w);
		ocr_recognize(ocr, entry->img);
	}
#endif
	return ocr;
#if 0
	knode_t *ktree = kd_create(entries, nb_entries, 0);
	return NULL;
#endif
}

#define R_W 140
#define R_H 140

ocr_from_img(ocr_t *ocr, img_t *img)
{
	fprintf(stderr, "H = %i ; W = %i\n", img->h, img->w);
	for (int h = 0; h < img->h - R_H; h++) {
		for (int w = 0; w < img->w - R_W; w++) {
			img_t *cropped = img_crop(img, h, w, R_H, R_W);
#if 0
			reduced = img_reduce(cropped, 28, 28);
			ocr_show_img_cli(reduced);
			char c = ocr_recognize(ocr, reduced);
			if (c != '!') {
				fprintf("Recognized: %c\n", c);
			}
#else
			ocr_show_img_cli(cropped);
			img_free(cropped);
#endif
		}
	}
}

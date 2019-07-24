
#include "ocr.h"

/* This will confuse people */
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "kdtree.h"

#define MGC_LABEL	0x00000801
#define MGC_DATA	0x00000803

unsigned int ocr_read_uint(FILE *fd)
{
	unsigned int read_val;
	if (fread(&read_val, sizeof(unsigned int), 1, fd) != 1) {
		exit(1);
	}
	return htonl(read_val);
}

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
			img->pix[dh][dw].r = pixval;
			img->pix[dh][dw].g = pixval;
			img->pix[dh][dw].b = pixval;
			nb_read += 1;
		}
	}
	entry_t *entry = (entry_t *) malloc(sizeof(entry_t));
	entry->img = img;
	entry->label = '0' + (char) fgetc(flabel);
	return entry;
}

void ocr_show_img_cli(img_t *img)
{
	for(unsigned int i = 0; i < img->h; i++) {
		for (unsigned int j = 0; j < img->w; j++) {
			if ((img->pix[i][j].r + img->pix[i][j].g + img->pix[i][j].b)/3.0 > 127) {
				printf("x");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
	return;
}

float ocr_dist(img_t *i1, img_t *i2)
{
	float dist = 0;
	if (i1->h != i2->h || i1->w != i2->w) {
		exit(1);
	}
	for (unsigned int i = 0; i < i1->h; i++) {
		for (unsigned int j = 0; j < i1->w; j++) {
			dist += pow((float) (i1->pix[i][j].r - i2->pix[i][j].r), 2.0);
		}
	}
	return sqrt(dist);
}

char ocr_recognize(entry_t **entries, unsigned int nb_entries, img_t *img)
{
	float min_dist = INFINITY;
	float dist;
	entry_t *closest = NULL;
	for (unsigned int i = 0; i < nb_entries; i++) {
		dist = ocr_dist(img, entries[i]->img);
		if (dist < min_dist) {
			min_dist = dist;
			closest = entries[i];
		}
	}
	ocr_show_img_cli(img);
	printf("recognized: %c\n", closest->label);
	ocr_show_img_cli(closest->img);
	return closest->label;
}

int ocr_train(char *label_path, char *data_path)
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
    nb_label = 1000;
	for (unsigned int i = 0; i < nb_label; i++) {
		entries[i] = ocr_read_one_entry(flabel, fdata, h, w);
		// ocr_show_img_cli(entries[i]->img);
		// printf("label: %c\n", entries[i]->label);
	}
	entry_t *entry = ocr_read_one_entry(flabel, fdata, h, w);
	ocr_recognize(entries, nb_label, entry->img);
    kd_create(entries, nb_label, 0);
	return 0;
}


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

#define READ_W 28*2
#define READ_H 28*2
#define FLAG_LEN 4
#define WRECT_MIN_W FLAG_LEN * READ_W

/* Read unsigned int from file (big endian) */
unsigned int ocr_read_uint(FILE *fd)
{
	unsigned int read_val;
	if (fread(&read_val, sizeof(unsigned int), 1, fd) != 1) {
		exit(1);
	}
	return htonl(read_val);
}

void ocr_next_white_rectangle(img_t *img, unsigned int *h, unsigned int *w)
{
	pix_list_t *senti = img->wpix;
	do {
		if (!senti) {
			break;
		}
		if (senti->h < *h || (senti->h == *h && senti->w < *w))
			goto next;
		if (senti->w + WRECT_MIN_W >= img->w || senti->h + READ_H > img->h) 
			goto next;
		bool white = true;
		for (unsigned int dw = senti->w; dw < senti->w + WRECT_MIN_W; dw++) {
			white &= (img->pix[senti->h][dw] == 0);
			white &= (img->pix[senti->h+READ_H - 1][dw] == 0);
		}
		if (white) {
			*h = senti->h; 
			*w = senti->w;
			return;
		}
next:
		senti = senti->nxt;
	} while (senti->nxt != img->wpix);
	/* No more white rectangle */
	*h = img->h;
	*w = img->w;
	return;
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
			if (img->pix[i][j]) {
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
	if (dist < 40000000)
		return '!';
	return closest->label;
}

#else

char ocr_nearest_kd(ocr_t *ocr, img_t *img) // entry_t **entries, unsigned int nb_entries, img_t *img)
{
	entry_t *best;
	float dist = 40000000000000;
	kd_search(ocr, img, &best, &dist);
#if 0 // DEBUG
	ocr_show_img_cli(img);
	printf("recognized: %c\n", best->label);
	printf("dist: %f\n", ocr_dist(best->img, img));
	ocr_show_img_cli(best->img);
#endif
	if (dist > 3000000)
		return '!';
	printf("dist: %f\n", ocr_dist(best->img, img));
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
	return ocr;
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

bool ocr_in_white_rectangle(img_t *img)
{
	for(unsigned int w = 0; w < img->w; w++) {
		if (img->pix[0][w]) { // + img->pix[h][1] + img->pix[h][2] + img->pix[h][3])
			return false;
		}
		if (img->pix[img->h - 1][w]) { //  + img->pix[h][img->w - 2] + img->pix[h][img->w - 3] + img->pix[h][img->w - 4])
			return false;
		}
	}
	return true;
}

bool ocr_fast_filter(img_t *img, unsigned int nb_pix, bool *in_white_rectangle)
{
	*in_white_rectangle = ocr_in_white_rectangle(img);
	if (nb_pix < 50)
		return false;
	if (!in_white_rectangle)
		return false; 
	for(unsigned int w = 0; w < img->w; w++) {
		if (img->pix[0][w] + img->pix[1][w] + img->pix[2][w] + img->pix[3][w])
			return false;
		if (img->pix[img->h - 1][w] + img->pix[img->h - 2][w] + img->pix[img->h - 3][w] + img->pix[img->h - 4][w])
			return false;
	}
	for(unsigned int h = 0; h < img->h; h++) {
		if (img->pix[h][0] + img->pix[h][1] + img->pix[h][2] + img->pix[h][3])
			return false;
		if (img->pix[h][img->w - 1] + img->pix[h][img->w - 2] + img->pix[h][img->w - 3] + img->pix[h][img->w - 4])
			return false;
	}
	return true;
}

unsigned int ocr_w_last_pix(img_t *img)
{
    for (unsigned dw = img->w - 1; dw > 0; dw--) {
        for (unsigned int dh = 0; dh < img->h; dh++) {
            if (img->pix[dh][dw])
                return dw;
        }
    }
    return 0;
}

void ocr_from_img(ocr_t *ocr, img_t *img)
{
	unsigned int h = 0, w = 0;
	bool in_white_rectangle;
	char input[FLAG_LEN + 1];
	input[FLAG_LEN] = '\0';
	unsigned int input_len = 0;
	while (h < img->h - READ_H && w < img->w - READ_W) {
        fprintf(stderr, "%u:%u\n", h, w);
		if (input_len == FLAG_LEN) {
			fprintf(stderr, "input: %s\n", input);
			break;
		}
		unsigned int nb_pix;
		img_t *cropped = img_crop(img, h, w, READ_H, READ_W, &nb_pix);
		img_t *reduced = img_reduce(cropped, READ_H / 28);
		img_free(cropped);
		if (!ocr_fast_filter(reduced, nb_pix, &in_white_rectangle)) {
			img_free(reduced);
			w++;
			if (w + READ_W == img->w) {
				w = 0;
				h++;
				input_len = 0;
			}
			if (!in_white_rectangle) {
				ocr_next_white_rectangle(img, &h, &w);
                input_len = 0;
			}
			continue;
		}
		char c = ocr_recognize(ocr, reduced);
		if (c != '!') {
			fprintf(stderr, "Recognized!! %c | nb_pix = %u\n", c, nb_pix);
			ocr_show_img_cli(reduced);
            w += ocr_w_last_pix(reduced) * (READ_H / 28);
			input[input_len] = c;
            input_len++;
		}
		img_free(reduced);
		w++;
		if (w + READ_W == img->w) {
			w = 0;
			h++;
			input_len = 0;
		}
	}
    return;
}


#include "packed/ocr.h"

/* This will confuse people */
#include <arpa/inet.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "gen/flag.h"
#include "global.h"
#include "img.h"

/* Global constants relative to OCR */
#if KD_TREE
#include "kdtree.h"
#endif

/* structure for the OCR dataset */
#define MGC_LABEL	0x00000801
#define MGC_DATA	0x00000803

/* read options: width and height of squares to perform OCR on */
#define RATIO 2
#define READ_W 28*RATIO
#define READ_H 28*RATIO
/* min. size of a white rectangle large enough to contain the flag */
#define WRECT_MIN_W FLAG_LEN * READ_W

/* distance threshold - the distance between the input 
   and the closest element of the dataset should be lower
   than this value for a digit to be recognized */
#define DIST_THRESHOLD 	2500000

/* read unsigned int from file (big endian) */
unsigned int ocr_read_uint(FILE *fd)
{
	unsigned int read_val;
	if (fread(&read_val, sizeof(unsigned int), 1, fd) != 1) {
		exit(1);
	}
	return htonl(read_val);
}

/* check the magic number the beginning of a file */
bool ocr_check_magic(FILE *fd, unsigned int magic)
{
	return ocr_read_uint(fd) == magic;
}

/* read one entry for the data file to train the OCR */
entry_t *ocr_read_one_entry(FILE *flabel, FILE *fdata, unsigned int h, unsigned int w)
{
	/* mapping for OCR */
	char map[10] = { '#', 'G', 'r', 'e', 'H', 'a', 'c', 'k', '1', '9'};
	unsigned int nb_read = 0;
	img_t *img = img_alloc(h, w);
	for (unsigned int dh = 0; dh < h; dh++) {
		for (unsigned int dw = 0; dw < w; dw++) {
			/* read pixel */
			unsigned char pixval;
			pixval = (char) fgetc(fdata);
			/* set pixel in structure */
			img->pix[dh][dw] = pixval?255:0;
			nb_read += 1;
		}
	}
	entry_t *entry = (entry_t *) malloc(sizeof(entry_t));
	unsigned int a, b, c, d;
	entry->img = img_center(img, &a, &b, &c, &d);
	img_free(img);
	entry->label = map[fgetc(flabel)];
	return entry;
}

/* find next white rectangle in image where a flag could be detected by ocr
   - h and w are the current positions in the image where to start the research from;
   - it looks for a rectangle of READ_H pixels in height and at least WRECT_MIN_W pixels
	 of width, with white borders
 */
void ocr_next_white_rectangle(img_t *img, unsigned int *h, unsigned int *w)
{
	/* iterate over white pixels of the image */
	pix_list_t *senti = img->wpix;
	do {
		if (!senti) {
			break;
		}
		/* exclude white pixels that are before the current image position */
		if (senti->h < *h || (senti->h == *h && senti->w < *w))
			goto next;
		if (senti->w + WRECT_MIN_W >= img->w || senti->h + READ_H > img->h) 
			goto next;
		bool white = true;
		/* check for white pixels on the top and bottom borders */
		for (unsigned int dw = senti->w; dw < senti->w + WRECT_MIN_W; dw++) {
			white &= (img->pix[senti->h][dw] == 0);
			white &= (img->pix[senti->h + 1][dw] == 0);
			white &= (img->pix[senti->h+READ_H - 1][dw] == 0);
			white &= (img->pix[senti->h+READ_H - 2][dw] == 0);
		}
		/* check for white pixels on left border */
		for (unsigned int dh = senti->h; dh < senti->h + READ_H; dh++) {
			white &= (img->pix[dh][senti->w] == 0);
			white &= (img->pix[dh][senti->w + 1] == 0);
		}
		/* if borders are white, then return the position of the top-left corner of 
		   this white rectangle */
		if (white) {
			*h = senti->h; 
			*w = senti->w;
			return;
		}
next:
		/* next candidate on the list of white pixels */
		senti = senti->nxt;
	} while (senti->nxt != img->wpix);
	/* no more white rectangle */
	*h = img->h;
	*w = img->w;
	return;
}


/******************************************************************************/
#if !KD_TREE 
/******************************************************************************/

/* find image in dataset nearest to the input image
   search in an array */
char ocr_nearest_array(ocr_t *ocr, img_t *img)
{
	float min_dist = INFINITY;
	float dist;
	entry_t *closest = NULL;
	for (unsigned int i = 0; i < ocr->nb_entries; i++) {
		dist = img_dist(img, ocr->entries[i]->img);
		if (dist < min_dist) {
			min_dist = dist;
			closest = ocr->entries[i];
		}
	}
#ifdef DEBUG_OCR
	img_show_cli(img);
	printf("recognized: %c\n", closest->label);
	img_show_cli(closest->img);
#endif
	if (dist < 40000000)
		return '!';
	return closest->label;
}

/******************************************************************************/
#else
/******************************************************************************/

/* find image in dataset nearest to the input image
   search in a kd tree */
char ocr_nearest_kd(ocr_t *ocr, img_t *img)
{
	entry_t *best;
	float dist = FLT_MAX;
	kd_search(ocr, img, &best, &dist);
#ifdef DEBUG_OCR
	img_show_cli(img);
	img_show_cli(best->img);
	fprintf(stderr, "dist: %f\n", img_dist(best->img, img));
	fprintf(stderr, "dist returned: %f\n", dist);
#endif
	if (dist > DIST_THRESHOLD)
		return '!';
	return best->label;
}

/******************************************************************************/
#endif
/******************************************************************************/


/* find the nearest neighbor of the image given as a parameter in the 
   data set of the OCR 
   this function is structure-agnostic */
char ocr_recognize(ocr_t *ocr, img_t *img)
{
#if KD_TREE
	char nearest = ocr_nearest_kd(ocr, img);
#else 
	char nearest = ocr_nearest_array(ocr, img);
#endif
	return nearest;
}

#ifndef KD_LOAD

/* read the training dataset and construct data structure for OCR in memory */
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
	/* check magic numbers */
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
	for (unsigned int i = 0; i < nb_label; i++) {
		entries[i] = ocr_read_one_entry(flabel, fdata, h, w);
	}
#if KD_TREE
	/* constuct a kd-tree */
	ocr_t *ocr = kd_create(entries, nb_label, 0);
	ocr->entries = entries;
#else
	/* construct an array */
	ocr_t *ocr = malloc(sizeof(ocr_t));
	ocr->nb_entries = nb_label;
	ocr->entries = entries;
#endif
#if TEST_KD
	for (unsigned int i = nb_label - 100; i < nb_label; i++)
		kd_test(ocr, entries[i]->img);
#endif
	fclose(flabel);
	fclose(fdata);
	return ocr;
}

#endif

/* fast check to avoid heavy ocr detection on squares that are not good candidates
   use several heuristics such as the number of pixels */
bool ocr_fast_filter(img_t *img, unsigned int nb_pix, unsigned int min_h, unsigned int max_h, unsigned int min_w, unsigned int max_w, bool *in_white_rectangle)
{
#if DEBUG_OCR
	fprintf(stderr, "minw: %u | maxw: %u | minh: %u | maxh: %u\n", min_w, max_w, min_h, max_h);
#endif
	*in_white_rectangle = (min_h > 0 && max_h < img->h - 1 && min_w > 0 && max_w < img->w - 1);
	if (nb_pix < 50 || nb_pix > 400) {
#ifdef DEBUG_OCR
		fprintf(stderr, "wrong nb of pixels (%u)\n", nb_pix);
#endif
		return false;
	}
	if (!*in_white_rectangle) {
#ifdef DEBUG_OCR
		fprintf(stderr, "not in white rectangle\n");
#endif
		return false; 
	}
	/* additional constraint on width */
	return true;
 //	(min_w > 1 && max_w < img->w - 2);
}

/* find the last column of pixels where there is at least one non-white pixel */
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

#ifdef DEBUG_OCR

char ocr_revert_map(char c)
{
	switch (c) {
	case '#':
		return '0';
	case 'G':
		return '1';
	case 'r':
		return '2';
	case 'e':
		return '3';
	case 'H':
		return '4';
	case 'a':
		return '5';
	case 'c':
		return '6';
	case 'k':
		return '7';
	case '1':
		return '8';
	case '9':
		return '9';
	default:
		return '?';
	}
}

#endif

img_t *ocr_focus(img_t *img, unsigned int h, unsigned int w, unsigned int *nb_pix, unsigned int *min_h, unsigned int *max_h, unsigned int *min_w, unsigned int *max_w)
{
	/* crop image to focus on current rectangle */
	img_t *cropped = img_crop(img, h, w, READ_H, READ_W, nb_pix);
#ifdef DEBUG_OCR	
	img_show_cli(cropped);
#endif
	/* center image to make detection easier */
	img_t *centered = img_center(cropped, min_h, max_h, min_w, max_w);
	img_free(cropped);
	/* reduce it to the size of kd tiles */
	img_t *reduced = img_reduce(centered, RATIO);
	img_free(centered);
	return reduced;
}

/* read the flag with ocr on the image given as a parameter 
   return the flag as a string if found, NULL otherwise */
char *ocr_read_flag(ocr_t *ocr, img_t *img)
{
#ifdef DEBUG_OCR
	fprintf(stderr, "reading flag on the screen...\n");
#endif
	/* reading position */
	unsigned int h = 0, w = 0;
	/* current read is in white rectangle? */
	bool in_white_rectangle;
	/* input flag chars read in the image */
	char *input = malloc(sizeof(char) * (FLAG_LEN + 1));
	input[FLAG_LEN] = '\0';
	/* number of chars read */
	unsigned int input_len = 0;
	/* number of non-white pixels in the current reading rectangle */
	unsigned int nb_pix;
	/* min and max w coordinates for, resp., the first and the last non-white pixel */
	unsigned int min_w, max_w;
	/* min and max h coordinates for, resp., the first and the last non-white pixel */
	unsigned int min_h, max_h;
	ocr_next_white_rectangle(img, &h, &w);
	/* main detection loop */
	while (h < img->h - READ_H && w < img->w - READ_W) {
		/* if flag has correct length, let's return it */
		if (input_len == FLAG_LEN) {
#ifdef DEBUG_OCR
			fprintf(stderr, "input: %s\n", input);
#endif
			break;
		}
		/* crop image to focus detection on current position */
		img_t *rect = ocr_focus(img, h, w, &nb_pix, &min_h, &max_h, &min_w, &max_w);
		if (ocr_fast_filter(rect, nb_pix/(RATIO * RATIO), min_h / RATIO, max_h / RATIO, min_w / RATIO, max_w / RATIO, &in_white_rectangle)) {
#ifdef DEBUG_OCR	
			fprintf(stderr, "Fast check: OK\n");
			img_show_cli(rect);
#endif
			char c = ocr_recognize(ocr, rect);
			if (c != '!') {
#ifdef DEBUG_OCR	
				fprintf(stderr, "Recognized!! %c | nb_pix = %u\n", ocr_revert_map(c), nb_pix);
				img_show_cli(rect);
#endif
				input[input_len] = c;
				input_len++;
			}
		}
		if (in_white_rectangle) {
			w += READ_W - 1;
		} else {
			if (min_w < 4) {
				w += max_w;
			} else {
				w += min_w - 2;
			}
		}
		if (w + READ_W >= img->w) {
			ocr_next_white_rectangle(img, &h, &w);
			input_len = 0;
			break;
		}
		img_free(rect);
	}
	if (input_len < FLAG_LEN) {
		FREE(input);
		input = NULL;
	}
	return input;
}

#ifdef KD_DUMP

void ocr_dump_entry(entry_t *entry, FILE *file)
{
	/* write label */
	fwrite(&(entry->label), sizeof(char), 1, file);  
	/* write image */
	img_dump(entry->img, file);
	return;	
}

#endif

#ifdef KD_LOAD

entry_t *ocr_load_entry(FILE *file)
{
	entry_t *entry = (entry_t *) malloc(sizeof(entry_t));
	/* read label */
	FREAD_KD(&(entry->label), sizeof(char), 1, file);  
	/* read image */
	entry->img = img_load(file);
	return entry;	
}

#endif

void ocr_free_entry(entry_t *entry)
{
	img_free(entry->img);
	FREE(entry);
	return;
}

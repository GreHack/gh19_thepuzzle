
#include "packed/ocr.h"

/* This will confuse people */
#include <arpa/inet.h>
#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
#define READ_W 28*2
#define READ_H 28*2
/* min. size of a white rectangle large enough to contain the flag */
#define WRECT_MIN_W FLAG_LEN * READ_W

/* distance threshold - the distance between the input 
   and the closest element of the dataset should be lower
   than this value for a digit to be recognized */
#define DIST_THRESHOLD 	3000000


/* read unsigned int from file (big endian) */
unsigned int ocr_read_uint(FILE *fd)
{
    TUPAC_BEG
	unsigned int read_val;
	if (fread(&read_val, sizeof(unsigned int), 1, fd) != 1) {
		exit(1);
	}
    TUPAC_END
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
	unsigned int nb_read = 0;
	img_t *img = (img_t *) malloc(sizeof(img_t));
	img->h = h;
	img->w = w;
	/* allocate pixels */
	img->pix = img_allocate_pixels(h, w);
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
			white &= (img->pix[senti->h+READ_H - 1][dw] == 0);
		}
		/* check for white pixels on left border */
		for (unsigned int dh = senti->h; dh < senti->h + READ_H; dh++) {
			white &= (img->pix[dh][senti->w] == 0);
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
	if (dist > DIST_THRESHOLD)
		return '!';
#ifdef DEBUG_OCR
	printf("dist: %f\n", img_dist(best->img, img));
#endif
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
	return ocr_nearest_kd(ocr, img);
#else 
	return ocr_nearest_array(ocr, img);
#endif
}

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
#else
	/* construct an array */
	ocr_t *ocr = malloc(sizeof(ocr_t));
	ocr->nb_entries = nb_label;
	ocr->entries = entries;
#endif
	return ocr;
}

/* return true iif the image given as an input has white pixels 
   in the top and bottom borders */
bool ocr_in_white_rectangle(img_t *img)
{
	for(unsigned int w = 0; w < img->w; w++) {
		/* check top pix */
		if (img->pix[0][w]) {
			return false;
		}
		/* check bottom pix */
		if (img->pix[img->h - 1][w]) {
			return false;
		}
	}
	return true;
}

/* fast check to avoid heavy ocr detection on squares that are not good candidates
   use several heuristics such as the number of pixels */
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

/* read the flag with ocr on the image given as a parameter 
   return the flag as a string if found, NULL otherwise */
char *ocr_read_flag(ocr_t *ocr, img_t *img)
{
	/* reading position */
	unsigned int h = 0, w = 0;
	/* current read is in white rectangle? */
	bool in_white_rectangle;
	/* input flag chars read in the image */
	char *input = malloc(sizeof(char) * (FLAG_LEN + 1));
	input[FLAG_LEN] = '\0';
	/* number of chars read */
	unsigned int input_len = 0;
	while (h < img->h - READ_H && w < img->w - READ_W) {
		/* if flag has correct length, let's return it */
		if (input_len == FLAG_LEN) {
#ifdef DEBUG_OCR
			fprintf(stderr, "input: %s\n", input);
#endif
			return input;
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
#ifdef DEBUG_OCR
			fprintf(stderr, "Recognized!! %c | nb_pix = %u\n", c, nb_pix);
			img_show_cli(reduced);
#endif
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
	free(input);
	return NULL;
}

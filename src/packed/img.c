#include "img.h"

#include <math.h>
#include <stdlib.h>

#define MIN3(a,b,c) (a<b?(a<c?a:c):(b<c?b:c))
#define MAX3(a,b,c) (a>b?(a>c?a:c):(b>c?b:c))

pix_t img_rgb_to_pix(pix_t *pix, unsigned char r, unsigned char g, unsigned char b)
{
	*pix = (255 - (MIN3(r, g, b) + MAX3(r, g, b)) / 2);
	return *pix;
}

/* IMPORTANT: insert in queue such that we keep an ordered linked list */
void img_add_white_pix(img_t *img, unsigned int h, unsigned int w)
{
	pix_list_t *wpix;
	if (img->wpix == NULL) {
		img->wpix = malloc(sizeof(pix_list_t));
		wpix = img->wpix;
		wpix->prev = wpix;
		wpix->nxt = wpix;
	} else {
		wpix = img->wpix->prev;
		wpix->nxt = malloc(sizeof(pix_list_t));
		wpix->nxt->nxt = img->wpix;
		wpix->nxt->prev = wpix;
		img->wpix->prev = wpix->nxt;
		wpix = wpix->nxt;
	}
	wpix->h = h;
	wpix->w = w;
	img->nb_wpix += 1;
	return;
}

void img_set_pix(img_t *img, unsigned int h, unsigned int w, pix_t pix)
{
	img->pix[h][w] = pix;
	if (pix == 0) {
		/* Add a white pixel to the image */
		img_add_white_pix(img, h, w);
	}
	return;
}

void img_set_pix_rgb(img_t *img, unsigned int h, unsigned int w, unsigned char r, unsigned char g, unsigned char b)
{
	pix_t pix = img_rgb_to_pix(&(img->pix[h][w]), r, g, b);
	if (pix == 0) {
		/* Add a white pixel to the image */
		img_add_white_pix(img, h, w);
	}
	return;
}

void img_free(img_t *img)
{
	for (unsigned int h = 0; h < img->h; h++) {
		free(img->pix[h]);
	}
	free(img->pix);
	// TODO free linked list of white pixels
	free(img);
}

pix_t **img_allocate_pixels(unsigned int h, unsigned int w)
{
	pix_t **pix = (pix_t **) malloc(sizeof(pix_t *) * h);
	for (unsigned int i = 0; i < h; i++) {
		pix[i] = (pix_t *) calloc(sizeof(pix_t), w);
	}
	return pix;
}


img_t *img_alloc(unsigned int h, unsigned int w)
{
	img_t *img = (img_t *) malloc(sizeof(img_t));
	img->h = h;
	img->w = w;
	img->pix = (pix_t **) malloc(h * sizeof(pix_t *));
	for (unsigned int dh = 0; dh < h; dh++) {
		img->pix[dh] = (pix_t *) malloc(w * sizeof(pix_t));
	}
	img->wpix = NULL;
	return img;
}

img_t *img_crop(img_t *img, unsigned int h, unsigned int w, unsigned int dh, unsigned int dw, unsigned int *nb_pix)
{
	TUPAC_BEG
	img_t *cropped = img_alloc(dh, dw);
	*nb_pix = 0;
	for (unsigned int y = h; y < h + dh; y++) {
		for (unsigned int x = w; x < w + dw; x++) {
			unsigned char p = img->pix[y][x];
			cropped->pix[y - h][x - w] = p;
			*nb_pix += p?1:0;
		}
	}
	TUPAC_END
	return cropped;
}

#ifdef DEBUG
void img_to_file(img_t *img, char *filepath)
{
	// Write the buffer to ppm file
	FILE* out = fopen(filepath, "wb");
	fprintf(out, "P5\n%d %d\n255\n", img->w, img->h);
	for (unsigned int dh = 0; dh < img->h; dh++)
		fwrite(img->pix[dh], 1, img->w * sizeof(pix_t), out);
	fclose(out);
}
#endif

img_t *img_reduce(img_t *img, unsigned int ratio)
{
	TUPAC_BEG
	img_t *new_img = img_alloc(img->h / ratio, img->w / ratio);
	for (unsigned int dh = 0; dh < new_img->h; dh++) {
		for (unsigned int dw = 0; dw < new_img->w; dw++) {
			unsigned int val = 0;
			for (unsigned int i = 0; i < ratio; i++) {
				for (unsigned int j = 0; j < ratio; j++) {
					val += img->pix[ratio * dh + i][ratio * dw + j]?1:0;
				}
			}
			img_set_pix(new_img, dh, dw, val>=2?255:0);
		}
	}
	TUPAC_END
	return new_img;
}

img_t *img_center(img_t *img, unsigned int *min_h, unsigned int *max_h, unsigned int *min_w, unsigned int *max_w)
{
	TUPAC_BEG
	img_t *new_img = (img_t *) malloc(sizeof(img_t));
	new_img->h = img->h;
	new_img->w = img->w;
	new_img->pix = img_allocate_pixels(img->h, img->w);
	/* init min & max values */
	*min_h = img->h - 1;
	*max_h = 0;
	*min_w = img->w - 1;
	*max_w = 0;
	/* compute center */
	for (unsigned int dh = 0; dh < img->h; dh++) {
		for (unsigned int dw = 0; dw < img->w; dw++) {
			if (img->pix[dh][dw]) {
				if (*min_h > dh)
					*min_h = dh;
				if (*min_w > dw)
					*min_w = dw;
				if (*max_h < dh)
					*max_h = dh;
				if (*max_w < dw)
					*max_w = dw;
			}
		}
	}
	if (*max_w == 0 || *max_h == 0)
		goto img_center_end;
	unsigned int hstart, wstart;
	hstart = (img->h - (*max_h - *min_h))/2;
	wstart = (img->w - (*max_w - *min_w))/2;
	for (unsigned int dh = 0; dh <= *max_h - *min_h; dh++)
		for (unsigned int dw = 0; dw <= *max_w - *min_w; dw++)
			new_img->pix[hstart + dh][wstart + dw] = img->pix[*min_h + dh][*min_w + dw];
img_center_end:
	TUPAC_END
	return new_img;
}

#ifdef DEBUG
/* display image in CLI */
void img_show_cli(img_t *img)
{
	for (unsigned int j = 0; j < img->w; j++)
		printf(".");
	printf("\n");
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
	for (unsigned int j = 0; j < img->w; j++)
		printf(".");
	printf("\n");
	return;
}
#endif

/* compute distance between two images (euclidian) */
/* CAUTION: returns square of dist */
float img_dist(img_t *i1, img_t *i2)
{
	float dist = 0.0;
	if (i1->h != i2->h || i1->w != i2->w) {
		exit(1);
	}
	for (unsigned int i = 0; i < i1->h; i++) {
		for (unsigned int j = 0; j < i1->w; j++) {
			if (i1->pix[i][j] != i2->pix[i][j]) {
				dist += pow(i1->pix[i][j] - i2->pix[i][j], 2.0);
#ifdef DEBUG_OCR
				// fprintf(stderr, "h: %u | w: %u | p1: %u | p2: %u\n", i, j, i1->pix[i][j], i2->pix[i][j]);
#endif
			}
		}
	}
	return dist;
}

#ifdef KD_DUMP

void img_dump(img_t *img, FILE *file)
{
	fwrite(&(img->h), sizeof(char), 1, file);
	fwrite(&(img->w), sizeof(char), 1, file);
	for (unsigned int h = 0; h < img->h; h++)
		for (unsigned int w = 0; w < img->w; w++)
			fwrite(&(img->pix[h][w]), sizeof(pix_t), 1, file);
}

#endif

#ifdef KD_LOAD

img_t *img_load(FILE *file)
{
	img_t *img = (img_t *) malloc(sizeof(img_t));
	img->h = 0;
	img->w = 0;
	/* read h and w */
	FREAD_KD(&(img->h), sizeof(char), 1, file);
	FREAD_KD(&(img->w), sizeof(char), 1, file);
	/* allocate pixels */
	img->pix = img_allocate_pixels(img->h, img->w);
	/* read pixels */
	for (unsigned int h = 0; h < img->h; h++) {
		for (unsigned int w = 0; w < img->w; w++) {
			FREAD_KD(&(img->pix[h][w]), sizeof(pix_t), 1, file);
		}
	}
#ifdef DEBUG_IMG
	img_show_cli(img);
#endif
	return img;
}

#endif

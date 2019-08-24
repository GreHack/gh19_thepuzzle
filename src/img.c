
#include "img.h"

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

void img_set_pix(img_t *img, unsigned int h, unsigned int w, unsigned char r, unsigned char g, unsigned char b)
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
	free(img);
}

img_t *img_crop(img_t *img, unsigned int h, unsigned int w, unsigned int dh, unsigned int dw, unsigned int *nb_pix)
{
	img_t *cropped = (img_t *) malloc(sizeof(img_t));
	*nb_pix = 0;
	cropped->h = dh;
	cropped->w = dw;
	cropped->pix = (pix_t **) malloc(dh * sizeof(pix_t *));
	for (unsigned int y = h; y < h + dh; y++) {
		cropped->pix[y - h] = (pix_t *) malloc(dw * sizeof(pix_t));
		for (unsigned int x = w; x < w + dw; x++) {
			unsigned char p = img->pix[y][x];
			cropped->pix[y - h][x - w] = p;
			*nb_pix += p?1:0;
		}
	}
	return cropped;
}

#if DEBUG
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

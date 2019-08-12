
#include "img.h"

#define MIN3(a,b,c) (a<b?(a<c?a:c):(b<c?b:c))
#define MAX3(a,b,c) (a>b?(a>c?a:c):(b>c?b:c))

void img_rgb_to_pix(pix_t *pix, unsigned char r, unsigned char g, unsigned char b)
{
	*pix = (MIN3(r, g, b) + MAX3(r, g, b)) / 2;
	return;
}

void img_free(img_t *img)
{
	for (int h = 0; h < img->h; h++) {
		free(img->pix[h]);
	}
	free(img->pix);
	free(img);
}

img_t *img_crop(img_t *img, unsigned int h, unsigned int w, unsigned int dh, unsigned int dw)
{
	img_t *cropped = (img_t *) malloc(sizeof(img_t));
	cropped->h = dh;
	cropped->w = dw;
	cropped->pix = (pix_t **) malloc(dh * sizeof(pix_t *));
	for (unsigned int y = h; y < h + dh; y++) {
		cropped->pix[y] = (pix_t *) malloc(dw * sizeof(pix_t));
		for (unsigned int x = w; x < w + dw; x++) {
			cropped->pix[y - h][x - w] = img->pix[y][x];
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
	for (int dh = 0; dh < img->h; dh++)
		fwrite(img->pix[dh], 1, img->w * sizeof(pix_t), out);
	fclose(out);
}
#endif


#ifndef __OCR_H__
#define __OCR_H__

#include <stdbool.h>

typedef struct {
	unsigned char r;
	unsigned char g;
	unsigned char b;
} pix_t;

typedef struct {
	pix_t **pix;
	unsigned int h;
	unsigned int w;
} img_t;

typedef struct {
	img_t *img;
	unsigned char label;
} entry_t;

typedef struct {
	entry_t *data[10000];
} set_t;

int ocr_train(char *label_path, char *data_path);

#endif

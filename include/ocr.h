
#ifndef __OCR_H__
#define __OCR_H__

#include <stdbool.h>

#include "img.h"

#define KD_TREE 1

typedef struct {
	img_t *img;
	unsigned char label;
} entry_t;

#if KD_TREE

#include "kdtree.h"
typedef knode_t ocr_t;

#else

typedef struct {
	entry_t **entries;
	unsigned int nb_entries;
} ocr_t;

#endif

float ocr_dist(img_t *i1, img_t *i2);
char ocr_recognize(ocr_t *ocr, img_t *img);
ocr_t *ocr_train(char *label_path, char *data_path);
void ocr_from_img(ocr_t *ocr, img_t *img);

#endif

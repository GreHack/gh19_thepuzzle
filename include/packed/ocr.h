
#ifndef __OCR_H__
#define __OCR_H__

#include <stdbool.h>

#include "global.h"
#include "img.h"

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

#ifndef KD_LOAD
ocr_t *ocr_train(char *label_path, char *data_path);
#endif

char *ocr_read_flag(ocr_t *ocr, img_t *img);

#ifdef KD_DUMP
void ocr_dump_entry(entry_t *entry, FILE *file);
#endif

#ifdef KD_LOAD
entry_t *ocr_load_entry(FILE *file);
#endif

void ocr_free_entry(entry_t *entry);
#endif

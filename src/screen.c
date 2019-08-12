#include "screen.h"

#include <stdlib.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <string.h>

#define DEBUG 1

img_t *screen_convert_to_img(char *data, int width, int height)
{
	img_t *img = (img_t *) malloc(sizeof(img_t));
	img->h = height;
	img->w = width;
	img->pix = (pix_t **) malloc(height * sizeof(pix_t *));
	for (int dh = 0; dh < height; dh++) {
		img->pix[dh] = (pix_t *) malloc(width * sizeof(pix_t));
		for (int dw = 0; dw < width; dw++) {
			char *imgpix = data + dh * width * 4 + dw * 4;
			img_rgb_to_pix(&(img->pix[dh][dw]), *(imgpix + 2), *(imgpix + 1), *(imgpix + 0));
		}
	}
	return img;
}

img_t *screen_capture()
{
	Display *display = XOpenDisplay(NULL);
	if (!display) {
		return NULL;
	}
	Window window = DefaultRootWindow(display);

	// Fetch screen size
	Screen *screen = DefaultScreenOfDisplay(display);
	int width = screen->width;
	int height = screen->height;

	// Take a screenshot
	XImage *image;
	image = XGetImage (display, window, 0, 0, width, height, AllPlanes, ZPixmap);
	img_t *img = screen_convert_to_img(image->data, width, height);
#if 0
	// Convert it to ppm
	// image->data is of format bgra, but to print that into a ppm we only
	// want the rgb format
	struct pix {
		uint8_t r;
		uint8_t g;
		uint8_t b;
	} __attribute__((packed));
	struct pix* img = malloc(width * height * sizeof(struct pix));
	memset(img, 0, width * height * sizeof(struct pix));
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			struct pix* t = img + y * width + x;
		}
	}
#endif
	// Free memory
	XFree (image);

	return img;
}

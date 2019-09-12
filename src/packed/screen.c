#include "screen.h"

#include <stdlib.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <string.h>

img_t *screen_convert_to_img(char *data, int width, int height)
{
	TUPAC_BEG
	img_t *img = (img_t *) malloc(sizeof(img_t));
	img->h = height;
	img->w = width;
	img->nb_wpix = 0;
	img->wpix = NULL;
	img->pix = (pix_t **) malloc(height * sizeof(pix_t *));
	for (int dh = 0; dh < height; dh++) {
		img->pix[dh] = (pix_t *) malloc(width * sizeof(pix_t));
		for (int dw = 0; dw < width; dw++) {
			char *imgpix = data + dh * width * 4 + dw * 4;
			img_set_pix_rgb(img, dh, dw, *(imgpix + 2), *(imgpix + 1), *(imgpix + 0));
		}
	}
	TUPAC_END
	return img;
}

img_t *screen_capture()
{
	TUPAC_BEG
	img_t *img = NULL;
	Display *display = XOpenDisplay(NULL);
	if (!display) {
		goto screen_capture_ret;
	}
	Window window = DefaultRootWindow(display);
	// Fetch screen size
	Screen *screen = DefaultScreenOfDisplay(display);
	int width = screen->width;
	int height = screen->height;
	// Take a screenshot
	XImage *image;
	image = XGetImage (display, window, 0, 0, width, height, AllPlanes, ZPixmap);
	img = screen_convert_to_img(image->data, width, height);
	// Free memory
	XFree (image);
screen_capture_ret:
	TUPAC_END
	return img;
}

#include "screen.h"

#include <stdlib.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <string.h>

#include "img.h"

img_t *screen_convert_to_img(char *data, int width, int height)
{
	img_t *img = img_alloc(height, width);
	for (int dh = 0; dh < height; dh++) {
		for (int dw = 0; dw < width; dw++) {
			char *imgpix = data + dh * width * 4 + dw * 4;
			img_set_pix_rgb(img, dh, dw, *(imgpix + 2), *(imgpix + 1), *(imgpix + 0));
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
	/* free memory */
	XDestroyImage(image);
	/* close display */
	XCloseDisplay(display);
	return img;
}

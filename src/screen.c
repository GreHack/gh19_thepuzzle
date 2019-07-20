#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdint.h>
#include <string.h>

int screen_init()
{
	Display *display = XOpenDisplay(NULL);
	if (!display) {
		return 1;
	}
	Window window = DefaultRootWindow(display);

	// Fetch screen size
	Screen *screen = DefaultScreenOfDisplay(display);
	int width = screen->width;
	int height = screen->height;

	// Take a screenshot
	XImage *image;
	image = XGetImage (display, window, 0, 0, width, height, AllPlanes, ZPixmap);

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
			char *imgpix = image->data + y * width * 4 + x * 4;
			t->r = *(imgpix + 2);
			t->g = *(imgpix + 1);
			t->b = *(imgpix + 0);
		}
	}

	// Write the buffer to ppm file
	FILE* out = fopen("out.ppm", "wb");
	fprintf(out, "P6\n%d %d\n255\n", width, height);
	fwrite(img, 1, width * height * sizeof(struct pix), out);
	fclose(out);
	printf("Say hi! :) -> out.ppm\n");

	// Free memory
	free(img);
	XFree (image);

	return 0;
}


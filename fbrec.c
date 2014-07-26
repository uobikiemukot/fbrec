/*
	Copyright (C) 2014 haru <uobikiemukot at gmail dot com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "fbrec.h"
#include "framebuffer.h"
//#include "quant.h"
#include "gifsave89.h"
//#include "fb2gif.h"
#include "ttyrec.h"

void set_colormap(struct framebuffer *fb, int colormap[COLORS * BYTES_PER_PIXEL + 1])
{
	int i, ci;
	uint8_t index;
	uint32_t r, g, b;

	if (fb->bytes_per_pixel == 1) {
		for (i = 0; i < COLORS; i++) {
			ci = i * BYTES_PER_PIXEL;
			colormap[ci + 0] = (*(fb->cmap_org->red   + i) >> BITS_PER_BYTE) & bit_mask[BITS_PER_BYTE];
			colormap[ci + 1] = (*(fb->cmap_org->green + i) >> BITS_PER_BYTE) & bit_mask[BITS_PER_BYTE];
			colormap[ci + 2] = (*(fb->cmap_org->blue  + i) >> BITS_PER_BYTE) & bit_mask[BITS_PER_BYTE];
		}
	} else {
		for (i = 0; i < COLORS; i++) {
			index = (uint8_t) i;
			ci = i * BYTES_PER_PIXEL;

			r = (index >> RED_SHIFT)   & bit_mask[RED_MASK];
			g = (index >> GREEN_SHIFT) & bit_mask[GREEN_MASK];
			b = (index >> BLUE_SHIFT)  & bit_mask[BLUE_MASK];

			colormap[ci + 0] = r * bit_mask[BITS_PER_BYTE] / bit_mask[RED_MASK];
			colormap[ci + 1] = g * bit_mask[BITS_PER_BYTE] / bit_mask[GREEN_MASK];
			colormap[ci + 2] = b * bit_mask[BITS_PER_BYTE] / bit_mask[BLUE_MASK];
		}
	}
	colormap[COLORS * BYTES_PER_PIXEL] = -1;
}

void take_capture(struct framebuffer *fb)
{
	memcpy(fb->buf, fb->fp, fb->width * fb->height * fb->bytes_per_pixel);
}

uint32_t pixel2index(struct fb_var_screeninfo *vinfo, uint32_t pixel)
{
	uint32_t r, g, b;

	if (vinfo->bits_per_pixel == 8)
		return pixel;

	/* split r, g, b bits */
	r = (pixel >> vinfo->red.offset)   & bit_mask[vinfo->red.length];
	g = (pixel >> vinfo->green.offset) & bit_mask[vinfo->green.length];
	b = (pixel >> vinfo->blue.offset)  & bit_mask[vinfo->blue.length];

	/* get MSB ..._MASK bits */
	r = (r >> (vinfo->red.length   - RED_MASK))   & bit_mask[RED_MASK];
	g = (g >> (vinfo->green.length - GREEN_MASK)) & bit_mask[GREEN_MASK];
	b = (b >> (vinfo->blue.length  - BLUE_MASK))  & bit_mask[BLUE_MASK];

	/*
	r = r * bit_mask[BITS_PER_BYTE] / bit_mask[vinfo->red.length];
	g = g * bit_mask[BITS_PER_BYTE] / bit_mask[vinfo->green.length];
	b = b * bit_mask[BITS_PER_BYTE] / bit_mask[vinfo->blue.length];

	r = (r >> RED_SHIFT)   & bit_mask[RED_MASK];
	g = (g >> GREEN_SHIFT) & bit_mask[GREEN_MASK];
	b = (b >> BLUE_SHIFT)  & bit_mask[BLUE_MASK];
	*/

	return (r << RED_SHIFT) | (g << GREEN_SHIFT) | (b << BLUE_SHIFT);
}

void apply_colormap(struct framebuffer *fb, unsigned char *img)
{
	int w, h;
	uint32_t pixel = 0;

	for (h = 0; h < fb->height; h++) {
		for (w = 0; w < fb->width; w++) {
			memcpy(&pixel, fb->buf + h * fb->line_length
				+ w * fb->bytes_per_pixel, fb->bytes_per_pixel);
			*(img + h * fb->width + w) = pixel2index(&fb->vinfo, pixel) & bit_mask[BITS_PER_BYTE];
		}
	}
}

size_t write_gif(unsigned char *gifimage, int size, const char *filename)
{
	FILE *fp;
	size_t wsize = 0;

 	fp = efopen(filename, "w");
	wsize = fwrite(gifimage, sizeof(unsigned char), size, fp);
	efclose(fp);

	return wsize;
}

void set_ttyraw(int fd, struct termios *save_tm)
{
	struct termios tm;

	etcgetattr(fd, save_tm);
	tm = *save_tm;
	tm.c_iflag = tm.c_oflag = 0;
	tm.c_cflag &= ~CSIZE;
	tm.c_cflag |= CS8;
	tm.c_lflag &= ~(ECHO | ISIG | ICANON);
	tm.c_cc[VMIN]  = 1; /* min data size (byte) */
	tm.c_cc[VTIME] = 0; /* time out */
	etcsetattr(fd, TCSAFLUSH, &tm);
}

void restore_ttymode(int fd, struct termios *save_tm)
{
	tcsetattr(fd, TCSAFLUSH, save_tm);
}

bool ttyrec_read(FILE *fp, struct ttyrec_header *header, unsigned char *buf)
{
	if (read_header(fp, header) == 0)
		return false;

	//fprintf(stderr, "len:%d\n", header->len);
	assert(header->len < BUFSIZE);
	if (fread(buf, 1, header->len, fp) == 0)
		perror("fread");

	return true;
}

float ttyrec_wait(struct timeval prev, struct timeval cur, double speed)
{
	(void) prev;
	(void) cur;
	(void) speed;
	return 0;
}

void ttyrec_write(int len, unsigned char *buf)
{
	fwrite(buf, 1, len, stdout);
}

void play_and_rec(struct framebuffer *fb, float speed, FILE *fp)
{
	bool first_time = true;
	struct timeval prev;
	unsigned char buf[BUFSIZE];
	struct ttyrec_header header;
	void *gsdata;
	unsigned char *gifimage = NULL;
	int gifsize, colormap[COLORS * BYTES_PER_PIXEL + 1];
	unsigned char *img;

	setbuf(stdout, NULL);
	setbuf(fp, NULL);

	/* allocate buffer */
	img = (unsigned char *) ecalloc(fb->width * fb->height, 1);

	/* init gif image */
	set_colormap(fb, colormap);
	if (!(gsdata = newgif((void **) &gifimage, fb->width, fb->height, colormap, 0)))
		return;

	animategif(gsdata, 0, 0, -1, 2);

	/* main loop */
	while (1) {
		if (!ttyrec_read(fp, &header, buf))
			break;

		if (!first_time)
			speed = ttyrec_wait(prev, header.tv, speed);
		first_time = false;

		ttyrec_write(header.len, buf);

		/* take screenshot */
		take_capture(fb);
		apply_colormap(fb, img);
		putgif(gsdata, img);

		prev = header.tv;
	}

	/* output gif */
	gifsize = endgif(gsdata);
	if (gifsize > 0) {
		write_gif(gifimage, gifsize, output_file);
		free(gifimage);
	}

	/* cleanup */
	free(img);
}

int main(int argc, char *argv[])
{
	float speed = 1.0;
	FILE *fp;
	struct termios save_tm;
	struct framebuffer fb;

	if (argc < 2) {
		fprintf(stderr, "usage: fbrec [record_file]\n");
		return EXIT_FAILURE;
	}

	fp = efopen(argv[1], "r");

	fb_init(&fb);
	set_ttyraw(STDIN_FILENO, &save_tm);

	play_and_rec(&fb, speed, fp);

	restore_ttymode(STDIN_FILENO, &save_tm);
	fb_die(&fb);

	efclose(fp);
		
	return EXIT_SUCCESS;
}

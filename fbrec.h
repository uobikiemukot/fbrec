/* See LICENSE for licence details. */
#define _XOPEN_SOURCE 600
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/fb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

const char *fb_path     = "/dev/fb0";
const char *output_file = "fbrec.gif";

enum {
	COLORS          = 256,
	BITS_PER_BYTE   = 8,
	BYTES_PER_PIXEL = 3,
	BUFSIZE         = 65535,
};

enum cmap_bitfield {
	RED_SHIFT   = 5,
	GREEN_SHIFT = 2,
	BLUE_SHIFT  = 0,
	RED_MASK	= 3,
	GREEN_MASK  = 3,
	BLUE_MASK   = 2
};

const uint32_t bit_mask[] = {
	0x00,
	0x01,       0x03,       0x07,       0x0F,
	0x1F,       0x3F,       0x7F,       0xFF,
	0x1FF,      0x3FF,      0x7FF,      0xFFF,
	0x1FFF,     0x3FFF,     0x7FFF,     0xFFFF,
	0x1FFFF,    0x3FFFF,    0x7FFFF,    0xFFFFF,
	0x1FFFFF,   0x3FFFFF,   0x7FFFFF,   0xFFFFFF,
	0x1FFFFFF,  0x3FFFFFF,  0x7FFFFFF,  0xFFFFFFF,
	0x1FFFFFFF, 0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF,
};

struct framebuffer {
	unsigned char *fp;               /* pointer of framebuffer (read only) */
	unsigned char *buf;
	int fd;                          /* file descriptor of framebuffer */
	int width, height;               /* display resolution */
	long screen_size;                /* screen data size (byte) */
	int line_length;                 /* line length (byte) */
	int bytes_per_pixel;             /* BYTES per pixel */
	struct fb_cmap *cmap_org;        /* cmap for legacy framebuffer (8bpp pseudocolor) */
	struct fb_var_screeninfo vinfo;
};

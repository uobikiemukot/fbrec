/* See LICENSE for licence details. */
/* error functions */
void error(char *str)
{
	perror(str);
	exit(EXIT_FAILURE);
}

void fatal(char *str)
{
	fprintf(stderr, "%s\n", str);
	exit(EXIT_FAILURE);
}

/* wrapper of C functions */
int eopen(const char *path, int flag)
{
	int fd;
	errno = 0;

	if ((fd = open(path, flag)) < 0) {
		fprintf(stderr, "cannot open \"%s\"\n", path);
		error("open");
	}

	return fd;
}

void eclose(int fd)
{
	errno = 0;

	if (close(fd) < 0)
		error("close");
}

FILE *efopen(const char *path, char *mode)
{
	FILE *fp;
	errno = 0;

	if ((fp = fopen(path, mode)) == NULL) {
		fprintf(stderr, "cannot open \"%s\"\n", path);
		error("fopen");
	}

	return fp;
}

void efclose(FILE *fp)
{
	errno = 0;

	if (fclose(fp) < 0)
		error("fclose");
}

void *ecalloc(size_t nmemb, size_t size)
{
	void *ptr;
	errno = 0;

	if ((ptr = calloc(nmemb, size)) == NULL)
		error("calloc");

	return ptr;
}

void *emmap(void *addr, size_t len, int prot, int flag, int fd, off_t offset)
{
	uint32_t *fp;
	errno = 0;

	if ((fp = (uint32_t *) mmap(addr, len, prot, flag, fd, offset)) == MAP_FAILED)
		error("mmap");

	return fp;
}

void emunmap(void *ptr, size_t len)
{
	errno = 0;

	if (munmap(ptr, len) < 0)
		error("munmap");
}

void etcgetattr(int fd, struct termios *tm)
{
	errno = 0;

	if (tcgetattr(fd, tm) < 0)
		error("tcgetattr");
}

void etcsetattr(int fd, int action, const struct termios *tm)
{
	errno = 0;

	if (tcsetattr(fd, action, tm) < 0)
		error("tcgetattr");
}

inline int my_ceil(int val, int div)
{
	return (val + div - 1) / div;
}

/* framebuffer function */
void cmap_create(struct fb_cmap **cmap)
{
	*cmap           = (struct fb_cmap *) ecalloc(1, sizeof(struct fb_cmap));
	(*cmap)->start  = 0;
	(*cmap)->len    = COLORS;
	(*cmap)->red    = (uint16_t *) ecalloc(COLORS, sizeof(uint16_t));
	(*cmap)->green  = (uint16_t *) ecalloc(COLORS, sizeof(uint16_t));
	(*cmap)->blue   = (uint16_t *) ecalloc(COLORS, sizeof(uint16_t));
	(*cmap)->transp = NULL;
}

void cmap_die(struct fb_cmap *cmap)
{
	if (cmap) {
		free(cmap->red);
		free(cmap->green);
		free(cmap->blue);
		free(cmap->transp);
		free(cmap);
	}
}

void fb_init(struct framebuffer *fb)
{
	char *path;
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

	if ((path = getenv("FRAMEBUFFER")) != NULL)
		fb->fd = eopen(path, O_RDWR);
	else
		fb->fd = eopen(fb_path, O_RDWR);

	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo))
		fatal("ioctl: FBIOGET_FSCREENINFO failed");

	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo))
		fatal("ioctl: FBIOGET_VSCREENINFO failed");

	fb->width  = vinfo.xres;
	fb->height = vinfo.yres;
	fb->screen_size = finfo.smem_len;
	fb->line_length = finfo.line_length;

	if ((finfo.visual == FB_VISUAL_TRUECOLOR || finfo.visual == FB_VISUAL_DIRECTCOLOR)
		&& (vinfo.bits_per_pixel == 15 || vinfo.bits_per_pixel == 16
		|| vinfo.bits_per_pixel == 24 || vinfo.bits_per_pixel == 32)) {
		fb->cmap_org = NULL;
		fb->bytes_per_pixel = my_ceil(vinfo.bits_per_pixel, BITS_PER_BYTE);
	}
	else if (finfo.visual == FB_VISUAL_PSEUDOCOLOR && vinfo.bits_per_pixel == 8) {
		cmap_create(&fb->cmap_org);
		if (ioctl(fb->fd, FBIOGETCMAP, fb->cmap_org))
			fatal("ioctl: FBIOGETCMAP failed");
		fb->bytes_per_pixel = 1;
	}
	else /* non packed pixel, mono color, grayscale: not implimented */
		fatal("unsupported framebuffer type");

	fb->fp	= (unsigned char *) emmap(0, fb->screen_size, PROT_WRITE | PROT_READ, MAP_SHARED, fb->fd, 0);
	fb->buf	= (unsigned char *) ecalloc(fb->width * fb->height, fb->bytes_per_pixel);
	fb->vinfo = vinfo;
}

void fb_die(struct framebuffer *fb)
{
	cmap_die(fb->cmap_org);
	emunmap(fb->fp, fb->screen_size);
	free(fb->buf);
	eclose(fb->fd);
}

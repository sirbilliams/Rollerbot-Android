/* Desktop tool: rb_import <disc folder | image.iso|.bin> <dest folder> [language]. Used for tests and by hand. */
#define _FILE_OFFSET_BITS 64
#include "RbImport.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int prog(void *u, int stage, long long d, long long t) { (void)u; if (d == t) fprintf(stderr, "stage %d done (%lld)\n", stage, t); return 0; }

int main(int argc, char **argv)
{
	if (argc < 3) { fprintf(stderr, "usage: rb_import <disc folder|image> <dest> [language]\n"); return 2; }
	char err[512] = "", tmp[1024];
	const char *src = argv[1], *lang = argc > 3 ? argv[3] : "english";
	struct stat st;
	if (stat(src, &st)) { perror(src); return 2; }
	if (S_ISDIR(st.st_mode)) snprintf(tmp, sizeof tmp, "%s", src);
	else {
		snprintf(tmp, sizeof tmp, "%s.cabs", argv[2]);
		int fd = open(src, O_RDONLY);
		if (fd < 0 || rb_image_copy_cabinets(fd, tmp, prog, NULL, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 1; }
		close(fd);
	}
	if (rb_import_cabinets(tmp, argv[2], lang, prog, NULL, err, sizeof err)) { fprintf(stderr, "%s\n", err); return 1; }
	puts("ok");
	return 0;
}

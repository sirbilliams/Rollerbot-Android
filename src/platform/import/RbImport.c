/* See RbImport.h. ISO 9660 reading is original; cabinet decoding is unshield's. */
#define _FILE_OFFSET_BITS 64
#include "RbImport.h"
#include "libunshield.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#define SECTOR 2048

/* ---- image access: logical 2048-byte sectors on top of a cooked or raw (2352) image ---- */
typedef struct { int fd; int stride, skip; } Img;

static unsigned int le32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned int)p[3] << 24; }

static int pread_all(int fd, void *buf, size_t n, long long off)
{
	size_t got = 0;
	while (got < n) {
		ssize_t r = pread(fd, (char *)buf + got, n - got, (off_t)(off + (long long)got));
		if (r < 0 && errno == EINTR) continue;
		if (r <= 0) return -1;
		got += (size_t)r;
	}
	return 0;
}

static int img_read(const Img *im, void *buf, size_t n, long long logical)
{
	unsigned char *o = buf;
	while (n) {
		long long sector = logical / SECTOR;
		size_t in = (size_t)(logical % SECTOR), take = SECTOR - in;
		if (take > n) take = n;
		if (pread_all(im->fd, o, take, sector * im->stride + im->skip + (long long)in)) return -1;
		o += take; n -= take; logical += (long long)take;
	}
	return 0;
}

static int img_open(Img *im, int fd)
{
	static const int stride[3] = { 2048, 2352, 2352 }, skip[3] = { 0, 16, 24 };
	for (int i = 0; i < 3; i++) {
		unsigned char b[6];
		if (pread_all(fd, b, 6, 16LL * stride[i] + skip[i]) == 0 && b[0] == 1 && memcmp(b + 1, "CD001", 5) == 0) {
			im->fd = fd; im->stride = stride[i]; im->skip = skip[i];
			return 1;
		}
	}
	return 0;
}

int rb_is_image(int fd) { Img im; return img_open(&im, fd); }

typedef struct { unsigned int lba, size; int is_dir; char name[256]; } Entry;

/* record name as text without ";1"; Joliet names are UTF-16BE */
static void rec_name(const unsigned char *r, int joliet, char *out, int cap)
{
	int nlen = r[32], n = 0;
	const unsigned char *name = r + 33;
	if (joliet) { for (int i = 0; i + 1 < nlen && n < cap - 1; i += 2) out[n++] = name[i] ? '?' : (char)name[i + 1]; }
	else for (int i = 0; i < nlen && n < cap - 1; i++) out[n++] = (char)name[i];
	out[n] = 0;
	char *semi = strchr(out, ';');
	if (semi) { *semi = 0; n = (int)(semi - out); }
	if (n > 0 && out[n - 1] == '.') out[n - 1] = 0;
}

static int list_dir(const Img *im, unsigned int lba, unsigned int size, int joliet, Entry *out, int max)
{
	unsigned char *buf = malloc(size ? size : 1);
	int count = 0;
	if (!buf || img_read(im, buf, size, (long long)lba * SECTOR)) { free(buf); return -1; }
	unsigned int o = 0;
	while (o < size && count < max) {
		unsigned int len = buf[o];
		if (len == 0) { o = (o / SECTOR + 1) * SECTOR; continue; }
		if (o + len > size || len < 34) break;
		const unsigned char *r = buf + o;
		o += len;
		if (r[32] == 1 && (r[33] == 0 || r[33] == 1)) continue;
		out[count].lba = le32(r + 2);
		out[count].size = le32(r + 10);
		out[count].is_dir = (r[25] & 2) != 0;
		rec_name(r, joliet, out[count].name, sizeof out[count].name);
		count++;
	}
	free(buf);
	return count;
}

static void mkdirs(const char *path)
{
	char p[1024];
	snprintf(p, sizeof p, "%s", path);
	for (char *s = p + 1; *s; s++)
		if (*s == '/') { *s = 0; mkdir(p, 0755); *s = '/'; }
	mkdir(p, 0755);
}

static int is_cabinet_name(const char *n)
{
	size_t l = strlen(n);
	if (l < 9 || strncasecmp(n, "data", 4) != 0) return 0;
	if (strcasecmp(n + l - 4, ".cab") != 0 && strcasecmp(n + l - 4, ".hdr") != 0) return 0;
	for (size_t i = 4; i < l - 4; i++) if (!isdigit((unsigned char)n[i])) return 0;
	return 1;
}

int rb_image_copy_cabinets(int fd, const char *tmp_dir, RbImportProgress progress, void *user, char *err, int err_size)
{
	Img im;
	if (!img_open(&im, fd)) { snprintf(err, err_size, "That file isn't a CD image (ISO 9660 .iso or .bin)."); return 1; }
	unsigned char vd[SECTOR];
	unsigned int root_lba = 0, root_size = 0;
	int joliet = 0, have = 0;
	for (int s = 16; s < 32; s++) {
		if (img_read(&im, vd, SECTOR, (long long)s * SECTOR) || memcmp(vd + 1, "CD001", 5) || vd[0] == 255) break;
		int j = vd[0] == 2 && vd[88] == '%' && vd[89] == '/' && (vd[90] == '@' || vd[90] == 'C' || vd[90] == 'E');
		if ((vd[0] == 1 && !have) || j) { root_lba = le32(vd + 158); root_size = le32(vd + 166); joliet = j; have = 1; }
	}
	Entry *e = malloc(sizeof(Entry) * 512);
	int n = have && e ? list_dir(&im, root_lba, root_size, joliet, e, 512) : -1;
	long long total = 0, done = 0;
	int found = 0, r = 0;
	for (int i = 0; i < n; i++) if (!e[i].is_dir && is_cabinet_name(e[i].name)) total += e[i].size;
	mkdirs(tmp_dir);
	static unsigned char buf[1 << 20];
	for (int i = 0; i < n && !r; i++) {
		if (e[i].is_dir || !is_cabinet_name(e[i].name)) continue;
		char path[1024];
		snprintf(path, sizeof path, "%s/%s", tmp_dir, e[i].name);
		for (char *c = path + strlen(tmp_dir) + 1; *c; c++) *c = (char)tolower((unsigned char)*c);
		FILE *f = fopen(path, "wb");
		if (!f) { r = 1; break; }
		long long off = (long long)e[i].lba * SECTOR, left = e[i].size;
		while (left > 0 && !r) {
			size_t k = left > (long long)sizeof buf ? sizeof buf : (size_t)left;
			if (img_read(&im, buf, k, off) || fwrite(buf, 1, k, f) != k) r = 1;
			off += (long long)k; left -= (long long)k; done += (long long)k;
			if (!r && progress && progress(user, 0, done, total)) r = 2;
		}
		fclose(f);
		found++;
	}
	free(e);
	if (r == 2) { snprintf(err, err_size, "Cancelled."); return 1; }
	if (r) { snprintf(err, err_size, "Couldn't read the files off the CD image (is there enough free space?)."); return 1; }
	if (!found) { snprintf(err, err_size, "No InstallShield files (data1.hdr / data1.cab) in the root of that CD image."); return 1; }
	return 0;
}

int rb_import_cabinets(const char *cab_dir, const char *dest_dir, const char *language,
                       RbImportProgress progress, void *user, char *err, int err_size)
{
	char cab[1024], want[128];
	snprintf(cab, sizeof cab, "%s/data1.cab", cab_dir);
	snprintf(want, sizeof want, "CodeAndData(%s)", language ? language : "english");
	unshield_set_log_level(0);
	Unshield *u = unshield_open(cab);
	if (!u) { snprintf(err, err_size, "The installer files (data1.hdr / data1.cab) couldn't be read."); return 1; }
	UnshieldFileGroup *g = unshield_file_group_find(u, want);   /* the disc's file groups: CodeAndData(<language>) */
	int saved = 0, have_exe = 0, rc = 0;
	if (!g) {
		snprintf(err, err_size, "That disc has no %s version of the game.", want);
		unshield_close(u);
		return 1;
	}
	int todo = (int)(g->last_file - g->first_file + 1);
	mkdirs(dest_dir);
	for (unsigned int i = g->first_file; i <= g->last_file && !rc; i++) {
		if (!unshield_file_is_valid(u, (int)i)) continue;
		const char *name = unshield_file_name(u, (int)i);
		if (!name || strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) continue;
		char path[1536];
		snprintf(path, sizeof path, "%s/%s", dest_dir, name);
		if (!unshield_file_save(u, (int)i, path) && !unshield_file_save_old(u, (int)i, path)) {
			snprintf(err, err_size, "Couldn't unpack %s (is there enough free space?).", name);
			rc = 1; break;
		}
		saved++;
		if (strcasecmp(name, "rollerbot.exe") == 0) have_exe = 1;
		if (progress && progress(user, 1, saved, todo)) { snprintf(err, err_size, "Cancelled."); rc = 1; }
	}
	unshield_close(u);
	if (!rc && !have_exe) { snprintf(err, err_size, "The disc's installer has no Rollerbot.exe."); rc = 1; }
	return rc;
}

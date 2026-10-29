/* Installs the game's files from the user's own CD: reads the InstallShield 5 cabinet
   (data1.hdr + data1.cab + data2.cab ...) with unshield (MIT) and extracts the "CodeAndData(<language>)"
   folders into a game folder. Also reads the cabinet files out of a CD image (.iso, or a raw 2352-byte
   .bin) so the app can take roller.bin directly. Original code; no game data is embedded. */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/* stage 0 = reading the image, 1 = extracting; done/total in bytes (0) or files (1). Return non-zero to cancel. */
typedef int (*RbImportProgress)(void *user, int stage, long long done, long long total);

/* 1 if the file open on fd is an ISO 9660 image (2048-byte sectors, or raw 2352-byte MODE1/MODE2 sectors). */
int rb_is_image(int fd);

/* Copies every dataN.hdr / dataN.cab in the image's root directory into tmp_dir (created).
   Returns 0, or non-zero with a message for the player in err. */
int rb_image_copy_cabinets(int fd, const char *tmp_dir, RbImportProgress progress, void *user, char *err, int err_size);

/* Extracts CodeAndData(<language>) (language: english, french, german, italian, spanish) from the cabinet
   data1.cab in cab_dir into dest_dir (the folder then holds Rollerbot.exe). Returns 0 or non-zero + message. */
int rb_import_cabinets(const char *cab_dir, const char *dest_dir, const char *language,
                       RbImportProgress progress, void *user, char *err, int err_size);

#ifdef __cplusplus
}
#endif

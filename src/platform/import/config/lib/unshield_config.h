/* unshield configuration for the Android / Linux builds of the game importer (instead of the
   CMake-generated one). No iconv on Android API 24, so file names stay as stored (ASCII here). */
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_FNMATCH 1
#define USE_OUR_OWN_MD5 1
#define SIZE_FORMAT "zu"
#define PACKAGE "libunshield"
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME "libunshield"
#define PACKAGE_STRING "libunshield 1.5.1"
#define PACKAGE_TARNAME "libunshield"
#define PACKAGE_URL ""
#define PACKAGE_VERSION "1.5.1"
#define VERSION "1.5.1"

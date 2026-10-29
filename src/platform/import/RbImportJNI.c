/* JNI entry points for io.github.sirbilliams.rollerbot.GameImporter (see RbImport.h). */
#include "RbImport.h"
#include <jni.h>

typedef struct { JNIEnv *env; jobject cb; jmethodID m; } Cb;

static int progress(void *user, int stage, long long done, long long total)
{
	Cb *c = (Cb *)user;
	if (!c->cb) return 0;
	jboolean cancel = (*c->env)->CallBooleanMethod(c->env, c->cb, c->m, (jint)stage, (jlong)done, (jlong)total);
	if ((*c->env)->ExceptionCheck(c->env)) { (*c->env)->ExceptionClear(c->env); return 1; }
	return cancel ? 1 : 0;
}

static void init_cb(JNIEnv *env, jobject cb, Cb *c)
{
	c->env = env; c->cb = cb; c->m = NULL;
	if (cb) {
		jclass k = (*env)->GetObjectClass(env, cb);
		c->m = (*env)->GetMethodID(env, k, "onProgress", "(IJJ)Z");
		if (!c->m) { (*env)->ExceptionClear(env); c->cb = NULL; }
	}
}

JNIEXPORT jboolean JNICALL Java_io_github_sirbilliams_rollerbot_GameImporter_isImage(JNIEnv *env, jclass cls, jint fd)
{
	(void)env; (void)cls;
	return rb_is_image(fd) ? JNI_TRUE : JNI_FALSE;
}

/* null on success, else the message for the player */
JNIEXPORT jstring JNICALL Java_io_github_sirbilliams_rollerbot_GameImporter_copyCabinets(JNIEnv *env, jclass cls, jint fd,
	jstring tmp, jobject cb)
{
	(void)cls;
	Cb c; init_cb(env, cb, &c);
	const char *t = (*env)->GetStringUTFChars(env, tmp, NULL);
	char err[512] = "";
	int r = rb_image_copy_cabinets(fd, t, progress, &c, err, sizeof err);
	(*env)->ReleaseStringUTFChars(env, tmp, t);
	return r ? (*env)->NewStringUTF(env, err) : NULL;
}

JNIEXPORT jstring JNICALL Java_io_github_sirbilliams_rollerbot_GameImporter_extract(JNIEnv *env, jclass cls, jstring cabDir,
	jstring dest, jstring lang, jobject cb)
{
	(void)cls;
	Cb c; init_cb(env, cb, &c);
	const char *a = (*env)->GetStringUTFChars(env, cabDir, NULL), *d = (*env)->GetStringUTFChars(env, dest, NULL),
	           *l = (*env)->GetStringUTFChars(env, lang, NULL);
	char err[512] = "";
	int r = rb_import_cabinets(a, d, l, progress, &c, err, sizeof err);
	(*env)->ReleaseStringUTFChars(env, cabDir, a);
	(*env)->ReleaseStringUTFChars(env, dest, d);
	(*env)->ReleaseStringUTFChars(env, lang, l);
	return r ? (*env)->NewStringUTF(env, err) : NULL;
}

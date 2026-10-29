// JNI side of rb::Bridge for the Android companion panel (class io.github.sirbilliams.rollerbot.Native).
#include "platform/Bridge.h"
#include <jni.h>

extern "C" {
JNIEXPORT jstring JNICALL Java_io_github_sirbilliams_rollerbot_Native_status(JNIEnv* env, jclass) {
  return env->NewStringUTF(rb::Bridge::statusLine().c_str());
JNIEXPORT void JNICALL Java_io_github_sirbilliams_rollerbot_Native_requestVolume(JNIEnv*, jclass, jint v) { rb::Bridge::requestVolume(v); }
JNIEXPORT void JNICALL Java_io_github_sirbilliams_rollerbot_Native_requestLang(JNIEnv*, jclass, jint i) { rb::Bridge::requestLang(i); }
}
JNIEXPORT void JNICALL Java_io_github_sirbilliams_rollerbot_Native_requestLevel(JNIEnv*, jclass, jint n) { rb::Bridge::requestLevel(n); }
JNIEXPORT void JNICALL Java_io_github_sirbilliams_rollerbot_Native_requestPause(JNIEnv*, jclass, jboolean on) { rb::Bridge::requestPause(on); }
JNIEXPORT void JNICALL Java_io_github_sirbilliams_rollerbot_Native_requestWidescreen(JNIEnv*, jclass, jint m) { rb::Bridge::requestWidescreen(m); }
JNIEXPORT void JNICALL Java_io_github_sirbilliams_rollerbot_Native_requestRestart(JNIEnv*, jclass) { rb::Bridge::requestRestart(); }
}

// Minimal stand-in for <jni.h> so the desktop build compiles the Android JNI glue (catches broken edits
// before the Android CI job does). Only what src/platform/BridgeJNI.cpp uses.
#pragma once
typedef int jint;
typedef unsigned char jboolean;
typedef struct _jobject* jobject;
typedef jobject jclass;
typedef jobject jstring;
struct JNIEnv { jstring NewStringUTF(const char*) { return nullptr; } };
#define JNIEXPORT
#define JNICALL

#pragma once

#include <jni.h>
#include <memory>

namespace mobileclock::renderer {
    class NativeRenderer;
}

namespace mobileclock::native {
    // Композиционный корень нативной части приложения.
    class NativeApplication {
    public:
        NativeApplication();
        ~NativeApplication();

        NativeApplication(const NativeApplication&) = delete;
        NativeApplication& operator=(const NativeApplication&) = delete;

        void setLogFile(JNIEnv* env, jstring javaLogFilePath);
        void flushLogs();
        void setAssetManager(JNIEnv* env, jobject javaAssetManager);
        void surfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height);
        void surfaceDestroyed();
        void touch(jint action, jfloat x, jfloat y);

    private:
        std::unique_ptr<mobileclock::renderer::NativeRenderer> _renderer;
    };
}
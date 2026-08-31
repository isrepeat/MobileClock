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

        void SetLogFile(JNIEnv* env, jstring javaLogFilePath);
        void FlushLogs();
        void SetAssetManager(JNIEnv* env, jobject javaAssetManager);
        void SurfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height);
        void SurfaceDestroyed();
        void Touch(jint action, jfloat x, jfloat y);
        void Render();

    private:
        std::unique_ptr<mobileclock::renderer::NativeRenderer> renderer;
    };
}
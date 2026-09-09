#include <Helpers.Logging/Logging.h>

#include "Renderer/NativeRenderer.h"
#include "NativeApplication.h"

namespace mobileclock::native {
    NativeApplication::NativeApplication()
        : renderer(std::make_unique<mobileclock::renderer::NativeRenderer>()) {
        // Не логируем конструктор: он вызывается до nativeSetLogFile и должен
        // позволить настроить файл логов до первого обращения к логгеру.
    }

    NativeApplication::~NativeApplication() = default;

    //
    // API
    //
    void NativeApplication::SetLogFile(JNIEnv* env, jstring javaLogFilePath) {
        this->renderer->SetLogFile(env, javaLogFilePath);
    }

    void NativeApplication::FlushLogs() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeApplication::FlushLogs");
        this->renderer->FlushLogs();
    }

    void NativeApplication::Log(JNIEnv* env, jstring javaCategory, jstring javaMessage) {
        const char* category = env->GetStringUTFChars(javaCategory, nullptr);
        if (category == nullptr) {
            return;
        }
        const char* message = env->GetStringUTFChars(javaMessage, nullptr);
        if (message == nullptr) {
            env->ReleaseStringUTFChars(javaCategory, category);
            return;
        }
        LOG_INFO("MobileClock.Kotlin", "{}: {}", category, message);
        env->ReleaseStringUTFChars(javaMessage, message);
        env->ReleaseStringUTFChars(javaCategory, category);
    }

    void NativeApplication::SetAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeApplication::SetAssetManager");
        this->renderer->SetAssetManager(env, javaAssetManager);
    }

    void NativeApplication::SetCommandDispatcher(JNIEnv* env, jobject javaDispatcher) {
        this->renderer->SetCommandDispatcher(env, javaDispatcher);
    }

    void NativeApplication::SetStatus(JNIEnv* env, jstring javaStatus) {
        this->renderer->SetStatus(env, javaStatus);
    }

    void NativeApplication::SurfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeApplication::SurfaceChanged: {}x{}", width, height);
        this->renderer->SurfaceChanged(env, androidSurface, width, height);
    }

    void NativeApplication::SurfaceDestroyed() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeApplication::SurfaceDestroyed");
        this->renderer->SurfaceDestroyed();
    }

    void NativeApplication::Touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeApplication::Touch: action={}, x={}, y={}", action, x, y);
        this->renderer->Touch(action, x, y);
    }

    void NativeApplication::Render() {
        this->renderer->Render();
    }
}
#include "NativeApplication.h"
#include "Renderer/NativeRenderer.h"

#include <Helpers/platform/Android/Logging.h>

namespace mobileclock::native {
    NativeApplication::NativeApplication()
        : _renderer(std::make_unique<mobileclock::renderer::NativeRenderer>()) {
        // Не логируем конструктор: он вызывается до nativeSetLogFile и должен
        // позволить настроить файл логов до первого обращения к логгеру.
    }

    NativeApplication::~NativeApplication() = default;

    void NativeApplication::SetLogFile(JNIEnv* env, jstring javaLogFilePath) {
        _renderer->SetLogFile(env, javaLogFilePath);
    }

    void NativeApplication::FlushLogs() {
        LOG_FUNCTION_SCOPE("NativeApplication::FlushLogs");
        _renderer->FlushLogs();
    }

    void NativeApplication::SetAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("NativeApplication::SetAssetManager");
        _renderer->SetAssetManager(env, javaAssetManager);
    }

    void NativeApplication::SurfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height) {
        LOG_FUNCTION_SCOPE("NativeApplication::SurfaceChanged: {}x{}", width, height);
        _renderer->SurfaceChanged(env, androidSurface, width, height);
    }

    void NativeApplication::SurfaceDestroyed() {
        LOG_FUNCTION_SCOPE("NativeApplication::SurfaceDestroyed");
        _renderer->SurfaceDestroyed();
    }

    void NativeApplication::Touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("NativeApplication::Touch: action={}, x={}, y={}", action, x, y);
        _renderer->Touch(action, x, y);
    }
}
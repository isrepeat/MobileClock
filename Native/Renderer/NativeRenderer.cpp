#include <Helpers.Logging/Logging.h>
#include <ESRenderer/OpenGlRenderer.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <EGL/egl.h>

#include "../UI/PageManager.h"
#include "NativeRenderer.h"

#include <string_view>
#include <filesystem>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>

namespace mobileclock::renderer {
    struct NativeRenderer::State {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
        ANativeWindow* window = nullptr;
        AAssetManager* assetManager = nullptr;
        mobileclock::ui::PageManager pageManager;
        std::unique_ptr<es_renderer::OpenGlRenderer> renderer;
    };
}

namespace mobileclock::renderer::_details {
    std::vector<unsigned char> ReadAsset(AAssetManager* assetManager, std::string_view path) {
        if (assetManager == nullptr) {
            throw std::runtime_error("AssetManager was not passed from Kotlin");
        }
        AAsset* asset = AAssetManager_open(
            assetManager,
            std::string(path).c_str(),
            AASSET_MODE_BUFFER);
        if (asset == nullptr) {
            throw std::runtime_error("Cannot open asset");
        }
        const auto size = static_cast<size_t>(AAsset_getLength(asset));
        std::vector<unsigned char> data(size);
        const int bytesRead = AAsset_read(asset, data.data(), size);
        AAsset_close(asset);
        if (bytesRead != static_cast<int>(size)) {
            throw std::runtime_error("Cannot read asset");
        }
        return data;
    }

    void DrawPage(NativeRenderer::State& state) {
        if (state.renderer == nullptr) {
            return;
        }
        state.renderer->BeginFrame();
        state.pageManager.UpdateClock();
        state.pageManager.Render(*state.renderer);
        eglSwapBuffers(state.display, state.surface);
    }

    void DestroyRenderer(NativeRenderer::State& state) {
        if (state.display != EGL_NO_DISPLAY) {
            if (state.context != EGL_NO_CONTEXT && state.surface != EGL_NO_SURFACE) {
                eglMakeCurrent(state.display, state.surface, state.surface, state.context);
            }
            state.renderer.reset();
            eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (state.context != EGL_NO_CONTEXT) {
                eglDestroyContext(state.display, state.context);
            }
            if (state.surface != EGL_NO_SURFACE) {
                eglDestroySurface(state.display, state.surface);
            }
            eglTerminate(state.display);
        }
        else {
            state.renderer.reset();
        }
        if (state.window != nullptr) {
            ANativeWindow_release(state.window);
        }
        state.display = EGL_NO_DISPLAY;
        state.surface = EGL_NO_SURFACE;
        state.context = EGL_NO_CONTEXT;
        state.window = nullptr;
    }
}

namespace mobileclock::renderer {
    NativeRenderer::NativeRenderer()
        : state(std::make_unique<State>()) {
    }

    NativeRenderer::~NativeRenderer() {
        this->SurfaceDestroyed();
    }

    //
    // API
    //
    void NativeRenderer::SetLogFile(JNIEnv* env, jstring javaLogFilePath) {
        const char* utf8Path = env->GetStringUTFChars(javaLogFilePath, nullptr);
        if (utf8Path == nullptr) {
            return;
        }
        utility_helpers::logging::Configure({
            std::filesystem::path(utf8Path),
        });
        utility_helpers::logging::Initialize("MobileClock");
        env->ReleaseStringUTFChars(javaLogFilePath, utf8Path);
    }

    void NativeRenderer::FlushLogs() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::FlushLogs");
        utility_helpers::logging::Flush();
    }

    void NativeRenderer::SetAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SetAssetManager");
        utility_helpers::logging::Initialize("MobileClock");
        this->state->assetManager = AAssetManager_fromJava(env, javaAssetManager);
        LOG_INFO("MobileClock", "Android AssetManager connected");
    }

    void NativeRenderer::SurfaceChanged(
        JNIEnv* env,
        jobject androidSurface,
        jint width,
        jint height) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SurfaceChanged: {}x{}", width, height);
        utility_helpers::logging::Initialize("MobileClock");
        State& state = *this->state;
        _details::DestroyRenderer(state);
        state.window = ANativeWindow_fromSurface(env, androidSurface);
        state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(state.display, nullptr, nullptr);

        const EGLint configurationAttributes[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_NONE,
        };
        EGLConfig configuration = nullptr;
        EGLint configurationCount = 0;
        eglChooseConfig(state.display, configurationAttributes, &configuration, 1, &configurationCount);
        const EGLint contextAttributes[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE,
        };
        state.context = eglCreateContext(
            state.display,
            configuration,
            EGL_NO_CONTEXT,
            contextAttributes);
        state.surface = eglCreateWindowSurface(
            state.display,
            configuration,
            state.window,
            nullptr);
        eglMakeCurrent(state.display, state.surface, state.surface, state.context);

        state.pageManager.Initialize({
            static_cast<float>(width),
            static_cast<float>(height),
        });
        const std::vector<unsigned char> regularFontData = _details::ReadAsset(
            state.assetManager,
            "Roboto-Regular.ttf");
        const std::vector<unsigned char> boldFontData = _details::ReadAsset(
            state.assetManager,
            "Roboto-Bold.ttf");
        const std::vector<unsigned char> blackFontData = _details::ReadAsset(
            state.assetManager,
            "Roboto-Black.ttf");
        state.renderer = std::make_unique<es_renderer::OpenGlRenderer>(
            width,
            height,
            regularFontData.data(),
            regularFontData.size(),
            boldFontData.data(),
            boldFontData.size(),
            blackFontData.data(),
            blackFontData.size(),
            [&state](std::string_view source) {
                return _details::ReadAsset(state.assetManager, source);
            });
        _details::DrawPage(state);
    }

    void NativeRenderer::SurfaceDestroyed() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SurfaceDestroyed");
        _details::DestroyRenderer(*this->state);
    }

    void NativeRenderer::Touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::Touch: action={}, x={}, y={}", action, x, y);
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            this->state->pageManager.HandleTouchDown(x, y);
            return;
        }
        if (action == AMOTION_EVENT_ACTION_CANCEL) {
            this->state->pageManager.CancelTouch();
            return;
        }
        if (action != AMOTION_EVENT_ACTION_UP) {
            return;
        }
        if (!this->state->pageManager.HandleTouchUp(x, y)) {
            return;
        }
        _details::DrawPage(*this->state);
    }

    void NativeRenderer::Render() {
        if (this->state->display == EGL_NO_DISPLAY
            || this->state->surface == EGL_NO_SURFACE) {
            return;
        }
        _details::DrawPage(*this->state);
    }
}
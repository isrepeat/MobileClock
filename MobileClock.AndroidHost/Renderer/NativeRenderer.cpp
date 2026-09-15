#include "NativeRenderer.h"

#include <Helpers.Logging/Logging.h>
#include <ESRenderer/OpenGlRenderer.h>
#include <JsonParser/json_struct/json_struct.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/input.h>
#include <EGL/egl.h>

#include "MobileClock.Presentation/Core/Registrations.h"
#include "MobileClock.Application/Model/AlarmRepository.h"
#include "MobileClock.Application/Core/AppSessionController.h"

#include "AndroidCommandDispatcher.h"
#include "AssetsManager.h"

#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace mobileclock::android_host::renderer {
    struct NativeRenderer::State {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
        ANativeWindow* window = nullptr;
        std::unique_ptr<AssetsManager> assetsManager;
        AndroidCommandDispatcher commandDispatcher;
        std::unique_ptr<mobileclock::application::core::ApplicationStateStore> stateStore;
        std::unique_ptr<mobileclock::application::model::AlarmRepository> alarmRepository;
        std::unique_ptr<mobileclock::application::model::AlarmMelodyRepository> alarmMelodyRepository;
        std::unique_ptr<mobileclock::application::core::AppSessionController> appSessionController;
        std::unique_ptr<es_renderer::OpenGlRenderer> renderer;
        bool isSessionInitialized = false;
    };
}

namespace mobileclock::android_host::renderer::_details {
    mobileclock::application::model::ApplicationStateDocument LoadStorage(const std::filesystem::path& path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return {};
        }
        const std::string json{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        mobileclock::application::model::ApplicationStateDocument document;
        JS::ParseContext context(json.data(), json.size());
        return context.parseTo(document) == JS::Error::NoError
            ? document : mobileclock::application::model::ApplicationStateDocument{};
    }

    bool SaveStorage(const std::filesystem::path& path, const mobileclock::application::model::ApplicationStateDocument& data) {
        const std::filesystem::path temporaryPath = path.string() + ".tmp";
        {
            std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
            stream << JS::serializeStruct(data);
            stream.flush();
            if (!stream) {
                return false;
            }
        }
        std::error_code error;
        std::filesystem::rename(temporaryPath, path, error);
        if (error) {
            std::filesystem::remove(path, error);
            error.clear();
            std::filesystem::rename(temporaryPath, path, error);
        }
        return !error;
    }
    // Схема кадра для Button с renderer="wave-outline":
    // Choreographer.doFrame()
    // └─ NativeRenderSurfaceView.doFrame()
    //    ├─ NativeRenderer.render() [Kotlin]
    //    │  └─ nativeRender() [JNI]
    //    │     └─ NativeApplication::Render()
    //    │        └─ NativeRenderer::Render()
    //    │           └─ DrawPage()
    //    │              ├─ PageManager::UpdateClock()
    //    │              │  └─ AnimationController::Update() обновляет поля состояния эффекта хоста
    //    │              ├─ PageManager::Render()
    //    │              │  └─ MainPageViewModel::Render()
    //    │              │     └─ xaml::Render()
    //    │              │        └─ xaml::_details::RenderElement()
    //    │              │           └─ RendererRegistry::Render()
    //    │              │              └─ RenderWaveOutline()
    //    │              │                 ├─ context.RenderDefaultElement()
    //    │              │                 │  ├─ RenderChrome()
    //    │              │                 │  └─ DrawText()
    //    │              │                 ├─ mobileclock::presentation::effects::RenderWave()
    //    │              │                 └─ DrawRoundedRectOutline() для дополнительной обводки
    //    │              └─ eglSwapBuffers() показывает завершённый кадр.
    //    └─ Choreographer.postFrameCallback() планирует следующий VSync.
    void DrawPage(NativeRenderer::State& state) {
        if (state.renderer == nullptr) {
            return;
        }
        state.renderer->BeginFrame();
        state.appSessionController->Session().Update();
        state.appSessionController->Session().Render(*state.renderer);
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

} // namespace _details

namespace mobileclock::android_host::renderer {
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
        const auto storagePath = std::filesystem::path(utf8Path).parent_path().parent_path() / "mobileclock-state.json";
        this->state->stateStore = std::make_unique<mobileclock::application::core::ApplicationStateStore>(
            _details::LoadStorage(storagePath),
            [storagePath](const mobileclock::application::model::ApplicationStateDocument& data) {
                return _details::SaveStorage(storagePath, data);
            });
        this->state->alarmRepository = std::make_unique<mobileclock::application::model::AlarmRepository>(*this->state->stateStore);
        this->state->alarmMelodyRepository = std::make_unique<mobileclock::application::model::AlarmMelodyRepository>(*this->state->stateStore);
        this->state->appSessionController = std::make_unique<mobileclock::application::core::AppSessionController>(*this->state->alarmRepository, *this->state->alarmMelodyRepository);
        this->state->appSessionController->SetHostEventHandler([this](
            mobileclock::application::core::AppSessionSignal signal,
            const mobileclock::application::core::AppSessionSignalData& data) {
            this->state->commandDispatcher.Dispatch(signal, data);
        });
        env->ReleaseStringUTFChars(javaLogFilePath, utf8Path);
    }

    void NativeRenderer::FlushLogs() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::FlushLogs");
        utility_helpers::logging::Flush();
    }

    void NativeRenderer::SetAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SetAssetManager");
        utility_helpers::logging::Initialize("MobileClock");
        this->state->assetsManager = std::make_unique<AssetsManager>(env, javaAssetManager);
        LOG_INFO("MobileClock", "Android AssetManager connected");
    }

    void NativeRenderer::SetCommandDispatcher(JNIEnv* env, jobject javaDispatcher) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SetCommandDispatcher");
        this->state->commandDispatcher.SetDispatcher(env, javaDispatcher);
    }

    void NativeRenderer::DispatchSessionSignal(
        JNIEnv* env,
        jint javaSignal,
        jstring javaValue,
        jstring javaAdditionalValue) {
        const char* value = env->GetStringUTFChars(javaValue, nullptr);
        if (value == nullptr) {
            return;
        }
        const char* additionalValue = env->GetStringUTFChars(javaAdditionalValue, nullptr);
        if (additionalValue == nullptr) {
            env->ReleaseStringUTFChars(javaValue, value);
            return;
        }
        this->state->appSessionController->Dispatch(
            static_cast<mobileclock::application::core::AppSessionSignal>(javaSignal),
            {value, additionalValue});
        env->ReleaseStringUTFChars(javaAdditionalValue, additionalValue);
        env->ReleaseStringUTFChars(javaValue, value);
    }

    void NativeRenderer::SurfaceChanged(
        JNIEnv* env,
        jobject androidSurface,
        jint width,
        jint height) {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SurfaceChanged: {}x{}", width, height);
        utility_helpers::logging::Initialize("MobileClock");
        State& state = *this->state;
        if (state.assetsManager == nullptr) {
            throw std::logic_error("AssetsManager must be set before creating a surface");
        }
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

        const xaml::Size availableSize{
            static_cast<float>(width),
            static_cast<float>(height),
        };
        if (state.isSessionInitialized) {
            state.appSessionController->Session().Resize(availableSize);
        } else {
            state.appSessionController->Session().Initialize(availableSize);
            state.isSessionInitialized = true;
        }
        const std::vector<unsigned char> regularFontData = state.assetsManager->ReadBytes("Fonts/Roboto-Regular.ttf");
        const std::vector<unsigned char> boldFontData = state.assetsManager->ReadBytes("Fonts/Roboto-Bold.ttf");
        const std::vector<unsigned char> blackFontData = state.assetsManager->ReadBytes("Fonts/Roboto-Black.ttf");
        state.renderer = std::make_unique<es_renderer::OpenGlRenderer>(
            width,
            height,
            regularFontData.data(),
            regularFontData.size(),
            boldFontData.data(),
            boldFontData.size(),
            blackFontData.data(),
            blackFontData.size(),
            mobileclock::presentation::core::CreateShaderPrograms(),
            [&assetsManager = *state.assetsManager](std::string_view source) {
                return assetsManager.ReadBytes(source);
            });
        _details::DrawPage(state);
    }

    void NativeRenderer::SurfaceDestroyed() {
        LOG_FUNCTION_SCOPE("MobileClock", "NativeRenderer::SurfaceDestroyed");
        _details::DestroyRenderer(*this->state);
    }

    void NativeRenderer::Touch(jint action, jfloat x, jfloat y) {
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            LOG_DEBUG("MobileClock.Touch", "Touch down received: point=({}, {})", x, y);
            this->state->appSessionController->Session().PointerDown(x, y);
            return;
        }
        if (action == AMOTION_EVENT_ACTION_CANCEL) {
            LOG_DEBUG("MobileClock.Touch", "Touch cancelled");
            this->state->appSessionController->Session().CancelPointer();
            return;
        }
        if (action == AMOTION_EVENT_ACTION_MOVE) {
            this->state->appSessionController->Session().PointerMove(x, y);
            return;
        }
        if (action != AMOTION_EVENT_ACTION_UP) {
            return;
        }
        LOG_DEBUG("MobileClock.Touch", "Touch up received: point=({}, {})", x, y);
        this->state->appSessionController->Session().PointerUp(x, y);
    }

    void NativeRenderer::Render() {
        if (this->state->display == EGL_NO_DISPLAY
            || this->state->surface == EGL_NO_SURFACE) {
            return;
        }
        _details::DrawPage(*this->state);
    }
}
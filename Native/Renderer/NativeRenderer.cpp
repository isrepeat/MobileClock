// Android NDK: доступ к Surface, EGL/OpenGL ES и файлам из папки assets.
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <jni.h>

#include <cstdlib>
#include <string>

#include <Helpers/platform/Android/Logging.h>
#include "Renderer/NativeRenderer.h"
#include "Renderer/ControlRenderer.h"
#include "XamlRuntime/XamlLayout.h"
#include "UI/MainPageController.h"

// stb_truetype создаёт обычную текстуру-атлас глифов; рисование делает OpenGL ES.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb_truetype.h"

namespace mobileclock::renderer {
    struct NativeRenderer::State {
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface surface = EGL_NO_SURFACE;
        EGLContext context = EGL_NO_CONTEXT;
        ANativeWindow* window = nullptr;
        AAssetManager* assetManager = nullptr;
        GLuint program = 0;
        GLuint solidProgram = 0;
        GLuint vertexBuffer = 0;
        GLuint fontTexture = 0;
        stbtt_bakedchar glyphs[96]{};
        mobileclock::ui::MainPageController mainPageController;
        std::unique_ptr<ControlRenderer> controlRenderer;
        int renderWidth = 0;
        int renderHeight = 0;
    };
}

namespace {
    constexpr int kFirstGlyph = 32; // ASCII-пробел.
    constexpr int kGlyphCount = 96; // Символы ASCII от 32 до 127.
    constexpr int kAtlasWidth = 512;
    constexpr int kAtlasHeight = 512;

    // Вершина хранит позицию на экране и координату в текстурном атласе.
    constexpr char kVertexShader[] = R"(#version 300 es
        layout (location = 0) in vec2 position;
        layout (location = 1) in vec2 textureCoordinate;
        out vec2 uv;
        void main() {
            uv = textureCoordinate;
            gl_Position = vec4(position, 0.0, 1.0);
        }
        )";

    // В красном канале текстуры находится непрозрачность символа.
    constexpr char kFragmentShader[] = R"(#version 300 es
        precision mediump float;
        in vec2 uv;
        uniform sampler2D fontAtlas;
        uniform vec4 textColor;
        out vec4 color;
        void main() {
            float alpha = texture(fontAtlas, uv).r;
            color = vec4(textColor.rgb, textColor.a * alpha);
        }
        )";

    constexpr char kSolidVertexShader[] = R"(#version 300 es
        layout (location = 0) in vec2 position;
        void main() { gl_Position = vec4(position, 0.0, 1.0); }
        )";

    constexpr char kSolidFragmentShader[] = R"(#version 300 es
        precision mediump float;
        uniform vec4 color;
        out vec4 fragmentColor;
        void main() { fragmentColor = color; }
        )";

    GLuint compileShader(GLenum type, const char* source) {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        return shader;
    }

    void makeProgram(mobileclock::renderer::NativeRenderer::State& state) {
        const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader);
        const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
        state.program = glCreateProgram();
        glAttachShader(state.program, vertex);
        glAttachShader(state.program, fragment);
        glLinkProgram(state.program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glGenBuffers(1, &state.vertexBuffer);

        const GLuint solidVertex = compileShader(GL_VERTEX_SHADER, kSolidVertexShader);
        const GLuint solidFragment = compileShader(GL_FRAGMENT_SHADER, kSolidFragmentShader);
        state.solidProgram = glCreateProgram();
        glAttachShader(state.solidProgram, solidVertex);
        glAttachShader(state.solidProgram, solidFragment);
        glLinkProgram(state.solidProgram);
        glDeleteShader(solidVertex);
        glDeleteShader(solidFragment);
    }

    bool makeFontAtlas(mobileclock::renderer::NativeRenderer::State& state) {
        if (state.assetManager == nullptr) {
        LOG_ERROR("AssetManager was not passed from Kotlin");
            return false;
        }

        // TTF упакован в APK из app/src/main/assets. NDK читает его без файлового пути.
        AAsset* fontAsset = AAssetManager_open(state.assetManager, "Roboto-Regular.ttf", AASSET_MODE_BUFFER);
        if (fontAsset == nullptr) {
        LOG_ERROR("Cannot open Roboto-Regular.ttf from assets");
            return false;
        }

        const auto fontSize = static_cast<size_t>(AAsset_getLength(fontAsset));
        auto* fontData = static_cast<unsigned char*>(std::malloc(fontSize));
        if (fontData == nullptr) {
            AAsset_close(fontAsset);
            return false;
        }
        const int bytesRead = AAsset_read(fontAsset, fontData, fontSize);
        AAsset_close(fontAsset);
        if (bytesRead != static_cast<int>(fontSize)) {
            std::free(fontData);
        LOG_ERROR("Cannot read Roboto-Regular.ttf");
            return false;
        }

        // Растеризуем ASCII в bitmap. Для Unicode и нескольких размеров атлас далее
        // должен пополняться динамически, но принцип отрисовки останется тем же.
        auto* atlas = static_cast<unsigned char*>(std::malloc(kAtlasWidth * kAtlasHeight));
        if (atlas == nullptr) {
            std::free(fontData);
            return false;
        }
        const int bakeResult = stbtt_BakeFontBitmap(
            fontData, 0, 64.0f, atlas, kAtlasWidth, kAtlasHeight,
            kFirstGlyph, kGlyphCount, state.glyphs);
        std::free(fontData);
        if (bakeResult <= 0) {
            std::free(atlas);
        LOG_ERROR("Font atlas is too small");
            return false;
        }

        glGenTextures(1, &state.fontTexture);
        glBindTexture(GL_TEXTURE_2D, state.fontTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasWidth, kAtlasHeight, 0,
            GL_RED, GL_UNSIGNED_BYTE, atlas);
        std::free(atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    void appendVertex(float* vertices, int& offset, float pixelX, float pixelY,
        float textureX, float textureY, int width, int height) {
        // stb использует пиксели от левого верхнего угла, OpenGL — NDC снизу слева.
        vertices[offset++] = pixelX * 2.0f / width - 1.0f;
        vertices[offset++] = 1.0f - pixelY * 2.0f / height;
        vertices[offset++] = textureX;
        vertices[offset++] = textureY;
    }

    void appendQuad(float* vertices, int& offset, const stbtt_aligned_quad& quad,
        int width, int height) {
        appendVertex(vertices, offset, quad.x0, quad.y0, quad.s0, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y0, quad.s1, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y1, quad.s1, quad.t1, width, height);
        appendVertex(vertices, offset, quad.x0, quad.y0, quad.s0, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y1, quad.s1, quad.t1, width, height);
        appendVertex(vertices, offset, quad.x0, quad.y1, quad.s0, quad.t1, width, height);
    }

    void drawText(mobileclock::renderer::NativeRenderer::State& state, const char* text, int width, int height,
        mobileclock::ui::Color color) {
        // Сначала измеряем строку, чтобы центрировать её. Эта минимальная версия
        // поддерживает ASCII; для кириллицы и emoji нужен Unicode shaping-движок.
        float textWidth = 0.0f;
        for (const char* character = text; *character != '\0'; ++character) {
            const int index = static_cast<unsigned char>(*character) - kFirstGlyph;
            if (index >= 0 && index < kGlyphCount) textWidth += state.glyphs[index].xadvance;
        }

        float cursorX = (width - textWidth) * 0.5f;
        float cursorY = height * 0.5f + 22.0f; // Baseline текста.
        // До 256 ASCII-символов по 6 вершин × 4 числа; для демо этого достаточно.
        float vertices[256 * 6 * 4]{};
        int vertexDataSize = 0;
        for (const char* character = text; *character != '\0'; ++character) {
            const int index = static_cast<unsigned char>(*character) - kFirstGlyph;
            if (index < 0 || index >= kGlyphCount) continue;
            stbtt_aligned_quad quad{};
            stbtt_GetBakedQuad(state.glyphs, kAtlasWidth, kAtlasHeight, index,
                &cursorX, &cursorY, &quad, 1);
            appendQuad(vertices, vertexDataSize, quad, width, height);
        }

        glUseProgram(state.program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state.fontTexture);
        glUniform1i(glGetUniformLocation(state.program, "fontAtlas"), 0);
        glUniform4f(glGetUniformLocation(state.program, "textColor"),
            color.red, color.green, color.blue, color.alpha);

        glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexDataSize * sizeof(float), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
            reinterpret_cast<void*>(2 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, vertexDataSize / 4);
    }

    void drawButtonOutline(mobileclock::renderer::NativeRenderer::State& state, const mobileclock::ui::Rect& bounds,
        mobileclock::ui::Color color) {
        const float left = bounds.x * 2.0f / state.renderWidth - 1.0f;
        const float right = (bounds.x + bounds.width) * 2.0f / state.renderWidth - 1.0f;
        const float top = 1.0f - bounds.y * 2.0f / state.renderHeight;
        const float bottom = 1.0f - (bounds.y + bounds.height) * 2.0f / state.renderHeight;
        const float vertices[] = {left, top, right, top, right, bottom, left, bottom};

        glUseProgram(state.solidProgram);
        glUniform4f(glGetUniformLocation(state.solidProgram, "color"),
            color.red, color.green, color.blue, color.alpha);
        glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    void drawPage(mobileclock::renderer::NativeRenderer::State& state) {
        glClear(GL_COLOR_BUFFER_BIT);
        if (state.fontTexture == 0 || state.controlRenderer == nullptr) {
            return;
        }
        state.mainPageController.render(*state.controlRenderer);
        eglSwapBuffers(state.display, state.surface);
    }

    void destroyRenderer(mobileclock::renderer::NativeRenderer::State& state) {
        // Удаляем OpenGL-ресурсы, пока state.context ещё привязан к потоку.
        state.controlRenderer.reset();
        if (state.display != EGL_NO_DISPLAY && state.context != EGL_NO_CONTEXT) {
            eglMakeCurrent(state.display, state.surface, state.surface, state.context);
            if (state.fontTexture != 0) glDeleteTextures(1, &state.fontTexture);
            if (state.vertexBuffer != 0) glDeleteBuffers(1, &state.vertexBuffer);
            if (state.program != 0) glDeleteProgram(state.program);
            if (state.solidProgram != 0) glDeleteProgram(state.solidProgram);
            eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        if (state.display != EGL_NO_DISPLAY) {
            if (state.context != EGL_NO_CONTEXT) eglDestroyContext(state.display, state.context);
            if (state.surface != EGL_NO_SURFACE) eglDestroySurface(state.display, state.surface);
            eglTerminate(state.display);
        }
        if (state.window != nullptr) ANativeWindow_release(state.window);
        state.display = EGL_NO_DISPLAY;
        state.surface = EGL_NO_SURFACE;
        state.context = EGL_NO_CONTEXT;
        state.window = nullptr;
        state.program = 0;
        state.solidProgram = 0;
        state.vertexBuffer = 0;
        state.fontTexture = 0;
    }

} // namespace

// Этот файл владеет состоянием EGL/OpenGL ES и преобразует UI-модель в команды GL.
// JNI и Android Activity lifecycle остаются за пределами renderer namespace.
namespace mobileclock::renderer {
    NativeRenderer::NativeRenderer() : _state(std::make_unique<State>()) {
        // См. NativeApplication: до nativeSetLogFile логгер ещё не настроен.
    }

    NativeRenderer::~NativeRenderer() {
        surfaceDestroyed();
    }

    void NativeRenderer::setLogFile(JNIEnv* env, jstring javaLogFilePath) {
        const char* utf8Path = env->GetStringUTFChars(javaLogFilePath, nullptr);
        if (utf8Path == nullptr) return;
        utility_helpers::android::configureLogFile(utf8Path);
        env->ReleaseStringUTFChars(javaLogFilePath, utf8Path);
    }

    void NativeRenderer::flushLogs() {
        LOG_FUNCTION_SCOPE("NativeRenderer::flushLogs");
        utility_helpers::android::flushLogging();
    }

    // Kotlin передаёт Android AssetManager один раз при старте Activity.
    void NativeRenderer::setAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("NativeRenderer::setAssetManager");
        utility_helpers::android::initializeLogging("MobileClock");
        _state->assetManager = AAssetManager_fromJava(env, javaAssetManager);
        LOG_INFO("Android AssetManager connected");
    }

    void NativeRenderer::surfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height) {
        LOG_FUNCTION_SCOPE("NativeRenderer::surfaceChanged: {}x{}", width, height);
        utility_helpers::android::initializeLogging("MobileClock");
        LOG_INFO("Surface changed: {}x{}", width, height);
        State& state = *_state;
        destroyRenderer(state);
        state.window = ANativeWindow_fromSurface(env, androidSurface);
        state.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(state.display, nullptr, nullptr);

        const EGLint configAttributes[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
            EGL_NONE
        };
        EGLConfig config;
        EGLint count;
        eglChooseConfig(state.display, configAttributes, &config, 1, &count);

        const EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        state.context = eglCreateContext(state.display, config, EGL_NO_CONTEXT, contextAttributes);
        state.surface = eglCreateWindowSurface(state.display, config, state.window, nullptr);
        eglMakeCurrent(state.display, state.surface, state.surface, state.context);

        glViewport(0, 0, width, height);
        state.renderWidth = width;
        state.renderHeight = height;
        makeProgram(state);
        // Текстурный атлас хранит форму букв в alpha-канале. Без blending OpenGL
        // всё равно записывает RGB даже при alpha = 0 — это выглядело бы как
        // заполненные прямоугольники вокруг глифов.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.40f, 0.40f, 0.40f, 1.0f);
        state.mainPageController.initialize({static_cast<float>(width), static_cast<float>(height)});
        makeFontAtlas(state);
        state.controlRenderer = std::make_unique<ControlRenderer>(
            [&state](const mobileclock::ui::Rect& bounds, mobileclock::ui::Color color) {
                drawButtonOutline(state, bounds, color);
            },
            [&state](std::string_view text, mobileclock::ui::Color color) {
                const std::string textCopy(text);
                drawText(state, textCopy.c_str(), state.renderWidth, state.renderHeight, color);
            });
        drawPage(state);
    }

    void NativeRenderer::surfaceDestroyed() {
        LOG_FUNCTION_SCOPE("NativeRenderer::surfaceDestroyed");
        LOG_INFO("Surface destroyed");
        destroyRenderer(*_state);
    }

    void NativeRenderer::touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("NativeRenderer::touch: action={}, x={}, y={}", action, x, y);
        // MotionEvent.ACTION_UP: меняем состояние только после завершённого тапа.
        if (action != 1) {
            return;
        }
        if (!_state->mainPageController.handleTap(x, y)) {
            return;
        }
        drawPage(*_state);
    }
}
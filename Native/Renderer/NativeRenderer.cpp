// Android NDK: доступ к Surface, EGL/OpenGL ES и файлам из папки assets.
#include <Helpers/platform/Android/Logging.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/asset_manager.h>
#include <android/input.h>
#include <XamlRuntime/XamlLayout.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <jni.h>

#include "Renderer/ControlRenderer.h"
#include "Renderer/NativeRenderer.h"
#include "UI/MainPageViewModel.h"

// stb_truetype создаёт обычную текстуру-атлас глифов; рисование делает OpenGL ES.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "ThirdParty/stb_truetype.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <cmath>

namespace mobileclock::renderer {
    // State содержит весь ресурсный граф рендерера. Он живёт между кадрами,
    // создаётся один раз вместе с NativeRenderer и очищается при смене Surface.
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
        stbtt_packedchar asciiGlyphs[96]{};
        stbtt_packedchar cyrillicGlyphs[256]{};
        stbtt_packedchar settingsGlyph[1]{};
        mobileclock::ui::MainPageViewModel mainPageViewModel;
        std::unique_ptr<ControlRenderer> controlRenderer;
        int renderWidth = 0;
        int renderHeight = 0;
    };
}

namespace {
    constexpr int kFirstAsciiGlyph = 32;
    constexpr int kAsciiGlyphCount = 96;
    constexpr int kFirstCyrillicGlyph = 0x0400;
    constexpr int kCyrillicGlyphCount = 256;
    constexpr int kSettingsGlyph = 0x2699;
    constexpr float kAtlasFontSize = 64.0f;
    constexpr int kAtlasWidth = 1024;
    constexpr int kAtlasHeight = 1024;
    constexpr int kMaxTextGlyphs = 256;

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
        // Для текста используется текстурный шейдер, для фонов и рамок —
        // отдельный одноцветный шейдер. Оба используют один динамический VBO.
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

        // ASCII и кириллица укладываются в один атлас. Размер текста при выводе
        // масштабируется относительно базовых 64 px без пересоздания текстуры.
        auto* atlas = static_cast<unsigned char*>(std::malloc(kAtlasWidth * kAtlasHeight));
        if (atlas == nullptr) {
            std::free(fontData);
            return false;
        }
        std::fill(atlas, atlas + kAtlasWidth * kAtlasHeight, 0);
        stbtt_pack_context context{};
        const int packStarted = stbtt_PackBegin(
            &context,
            atlas,
            kAtlasWidth,
            kAtlasHeight,
            0,
            1,
            nullptr);
        const int asciiPacked = packStarted == 0 ? 0 : stbtt_PackFontRange(
            &context,
            fontData,
            0,
            kAtlasFontSize,
            kFirstAsciiGlyph,
            kAsciiGlyphCount,
            state.asciiGlyphs);
        const int cyrillicPacked = asciiPacked == 0 ? 0 : stbtt_PackFontRange(
            &context,
            fontData,
            0,
            kAtlasFontSize,
            kFirstCyrillicGlyph,
            kCyrillicGlyphCount,
            state.cyrillicGlyphs);
        const int settingsPacked = cyrillicPacked == 0 ? 0 : stbtt_PackFontRange(
            &context,
            fontData,
            0,
            kAtlasFontSize,
            kSettingsGlyph,
            1,
            state.settingsGlyph);
        if (packStarted != 0) {
            stbtt_PackEnd(&context);
        }
        std::free(fontData);
        if (asciiPacked == 0 || cyrillicPacked == 0 || settingsPacked == 0) {
            std::free(atlas);
            LOG_ERROR("Font atlas is too small");
            return false;
        }

        glGenTextures(1, &state.fontTexture);
        glBindTexture(GL_TEXTURE_2D, state.fontTexture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_R8,
            kAtlasWidth,
            kAtlasHeight,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            atlas);
        std::free(atlas);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        return true;
    }

    void appendVertex(
        float* vertices,
        int& offset,
        float pixelX,
        float pixelY,
        float textureX,
        float textureY,
        int width,
        int height) {
        // stb использует пиксели от левого верхнего угла, OpenGL — NDC снизу слева.
        vertices[offset++] = pixelX * 2.0f / width - 1.0f;
        vertices[offset++] = 1.0f - pixelY * 2.0f / height;
        vertices[offset++] = textureX;
        vertices[offset++] = textureY;
    }

    void appendQuad(
        float* vertices,
        int& offset,
        const stbtt_aligned_quad& quad,
        int width,
        int height) {
        appendVertex(vertices, offset, quad.x0, quad.y0, quad.s0, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y0, quad.s1, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y1, quad.s1, quad.t1, width, height);
        appendVertex(vertices, offset, quad.x0, quad.y0, quad.s0, quad.t0, width, height);
        appendVertex(vertices, offset, quad.x1, quad.y1, quad.s1, quad.t1, width, height);
        appendVertex(vertices, offset, quad.x0, quad.y1, quad.s0, quad.t1, width, height);
    }

    uint32_t decodeUtf8(const char*& current, const char* end) {
        const auto lead = static_cast<unsigned char>(*current++);
        if (lead < 0x80) {
            return lead;
        }
        if ((lead & 0xE0) == 0xC0 && current < end) {
            const auto second = static_cast<unsigned char>(*current++);
            return static_cast<uint32_t>(lead & 0x1F) << 6
                | static_cast<uint32_t>(second & 0x3F);
        }
        if ((lead & 0xF0) == 0xE0 && end - current >= 2) {
            const auto second = static_cast<unsigned char>(*current++);
            const auto third = static_cast<unsigned char>(*current++);
            return static_cast<uint32_t>(lead & 0x0F) << 12
                | static_cast<uint32_t>(second & 0x3F) << 6
                | static_cast<uint32_t>(third & 0x3F);
        }
        if ((lead & 0xF8) == 0xF0 && end - current >= 3) {
            const auto second = static_cast<unsigned char>(*current++);
            const auto third = static_cast<unsigned char>(*current++);
            const auto fourth = static_cast<unsigned char>(*current++);
            return static_cast<uint32_t>(lead & 0x07) << 18
                | static_cast<uint32_t>(second & 0x3F) << 12
                | static_cast<uint32_t>(third & 0x3F) << 6
                | static_cast<uint32_t>(fourth & 0x3F);
        }
        return '?';
    }

    struct GlyphReference {
        const stbtt_packedchar* glyphs = nullptr;
        int index = 0;
    };

    GlyphReference glyphReference(
        const mobileclock::renderer::NativeRenderer::State& state,
        uint32_t codepoint) {
        if (codepoint >= kFirstAsciiGlyph
            && codepoint < kFirstAsciiGlyph + kAsciiGlyphCount) {
            return {state.asciiGlyphs, static_cast<int>(codepoint) - kFirstAsciiGlyph};
        }
        if (codepoint >= kFirstCyrillicGlyph
            && codepoint < kFirstCyrillicGlyph + kCyrillicGlyphCount) {
            return {state.cyrillicGlyphs, static_cast<int>(codepoint) - kFirstCyrillicGlyph};
        }
        if (codepoint == kSettingsGlyph) {
            return {state.settingsGlyph, 0};
        }
        return {state.asciiGlyphs, '?' - kFirstAsciiGlyph};
    }

    void drawText(
        mobileclock::renderer::NativeRenderer::State& state,
        const xaml::Rect& bounds,
        std::string_view text,
        int width,
        xaml::attr::Color color,
        float fontSize,
        std::string_view fontWeight) {
        const float scale = fontSize / kAtlasFontSize;
        float textWidth = 0.0f;
        float minimumY = std::numeric_limits<float>::max();
        float maximumY = std::numeric_limits<float>::lowest();
        int glyphCount = 0;
        const char* current = text.data();
        const char* const end = text.data() + text.size();
        while (current < end && glyphCount < kMaxTextGlyphs) {
            const GlyphReference reference = glyphReference(state, decodeUtf8(current, end));
            const stbtt_packedchar& glyph = reference.glyphs[reference.index];
            textWidth += glyph.xadvance * scale;
            minimumY = std::min(minimumY, glyph.yoff * scale);
            maximumY = std::max(maximumY, glyph.yoff2 * scale);
            ++glyphCount;
        }
        if (glyphCount == 0) {
            return;
        }

        float cursorX = (bounds.x + (bounds.width - textWidth) * 0.5f) / scale;
        float cursorY = (
            bounds.y + (bounds.height - (maximumY - minimumY)) * 0.5f - minimumY) / scale;
        float vertices[kMaxTextGlyphs * 6 * 4 * 2]{};
        int vertexDataSize = 0;
        current = text.data();
        int renderedGlyphs = 0;
        const bool isBold = fontWeight == "Bold" || fontWeight == "SemiBold";
        while (current < end && renderedGlyphs < kMaxTextGlyphs) {
            const GlyphReference reference = glyphReference(state, decodeUtf8(current, end));
            stbtt_aligned_quad quad{};
            stbtt_GetPackedQuad(
                reference.glyphs,
                kAtlasWidth,
                kAtlasHeight,
                reference.index,
                &cursorX,
                &cursorY,
                &quad,
                1);
            quad.x0 *= scale;
            quad.x1 *= scale;
            quad.y0 *= scale;
            quad.y1 *= scale;
            appendQuad(vertices, vertexDataSize, quad, width, state.renderHeight);
            if (isBold) {
                quad.x0 += 1.0f;
                quad.x1 += 1.0f;
                appendQuad(vertices, vertexDataSize, quad, width, state.renderHeight);
            }
            ++renderedGlyphs;
        }

        glUseProgram(state.program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, state.fontTexture);
        glUniform1i(glGetUniformLocation(state.program, "fontAtlas"), 0);
        glUniform4f(
            glGetUniformLocation(state.program, "textColor"),
            color.red,
            color.green,
            color.blue,
            color.alpha);

        glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, vertexDataSize * sizeof(float), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,
            2,
            GL_FLOAT,
            GL_FALSE,
            4 * sizeof(float),
            reinterpret_cast<void*>(2 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, vertexDataSize / 4);
    }

    void drawButtonOutline(
        mobileclock::renderer::NativeRenderer::State& state,
        const xaml::Rect& bounds,
        xaml::attr::Color color) {
        const float left = bounds.x * 2.0f / state.renderWidth - 1.0f;
        const float right = (bounds.x + bounds.width) * 2.0f / state.renderWidth - 1.0f;
        const float top = 1.0f - bounds.y * 2.0f / state.renderHeight;
        const float bottom = 1.0f - (bounds.y + bounds.height) * 2.0f / state.renderHeight;
        const float vertices[] = {left, top, right, top, right, bottom, left, bottom};

        glUseProgram(state.solidProgram);
        glUniform4f(
            glGetUniformLocation(state.solidProgram, "color"),
            color.red,
            color.green,
            color.blue,
            color.alpha);
        glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
    }

    void drawRoundedRect(
        mobileclock::renderer::NativeRenderer::State& state,
        const xaml::Rect& bounds,
        xaml::attr::Color color,
        float cornerRadius) {
        // Веер треугольников: первая вершина в центре, остальные описывают
        // контур четырёх скруглённых углов по часовой стрелке.
        const float radius = std::min({cornerRadius, bounds.width / 2.0f, bounds.height / 2.0f});
        constexpr int segmentsPerCorner = 8;
        constexpr int vertexCount = 1 + segmentsPerCorner * 4 + 1;
        float vertices[vertexCount * 2]{};
        int offset = 0;
        const auto appendPosition = [&state, &vertices, &offset](float x, float y) {
            vertices[offset++] = x * 2.0f / state.renderWidth - 1.0f;
            vertices[offset++] = 1.0f - y * 2.0f / state.renderHeight;
        };

        appendPosition(bounds.x + bounds.width / 2.0f, bounds.y + bounds.height / 2.0f);
        const float centers[][2] = {
            {bounds.x + bounds.width - radius, bounds.y + radius},
            {bounds.x + bounds.width - radius, bounds.y + bounds.height - radius},
            {bounds.x + radius, bounds.y + bounds.height - radius},
            {bounds.x + radius, bounds.y + radius},
        };
        constexpr float kPi = 3.14159265358979323846f;
        for (int corner = 0; corner < 4; ++corner) {
            const float startAngle = -kPi / 2.0f + corner * kPi / 2.0f;
            for (int segment = 0; segment < segmentsPerCorner; ++segment) {
                const float angle = startAngle + segment * kPi / (2.0f * segmentsPerCorner);
                appendPosition(
                    centers[corner][0] + std::cos(angle) * radius,
                    centers[corner][1] + std::sin(angle) * radius);
            }
        }
        appendPosition(bounds.x + bounds.width - radius, bounds.y);

        glUseProgram(state.solidProgram);
        glUniform4f(
            glGetUniformLocation(state.solidProgram, "color"),
            color.red,
            color.green,
            color.blue,
            color.alpha);
        glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertexCount);
    }

    void drawRoundedRectOutline(
        mobileclock::renderer::NativeRenderer::State& state,
        const xaml::Rect& bounds,
        xaml::attr::Color color,
        float cornerRadius,
        float thickness) {
        // В отличие от drawRoundedRect здесь нет центральной вершины: GL_LINE_LOOP
        // рисует только контур. Это нужно для border без заданного background.
        const float radius = std::min({cornerRadius, bounds.width / 2.0f, bounds.height / 2.0f});
        constexpr int segmentsPerCorner = 8;
        constexpr int vertexCount = segmentsPerCorner * 4;
        float vertices[vertexCount * 2]{};
        int offset = 0;
        const auto appendPosition = [&state, &vertices, &offset](float x, float y) {
            vertices[offset++] = x * 2.0f / state.renderWidth - 1.0f;
            vertices[offset++] = 1.0f - y * 2.0f / state.renderHeight;
        };
        const float centers[][2] = {
            {bounds.x + bounds.width - radius, bounds.y + radius},
            {bounds.x + bounds.width - radius, bounds.y + bounds.height - radius},
            {bounds.x + radius, bounds.y + bounds.height - radius},
            {bounds.x + radius, bounds.y + radius},
        };
        constexpr float kPi = 3.14159265358979323846f;
        for (int corner = 0; corner < 4; ++corner) {
            const float startAngle = -kPi / 2.0f + corner * kPi / 2.0f;
            for (int segment = 0; segment < segmentsPerCorner; ++segment) {
                const float angle = startAngle + segment * kPi / (2.0f * segmentsPerCorner);
                appendPosition(
                    centers[corner][0] + std::cos(angle) * radius,
                    centers[corner][1] + std::sin(angle) * radius);
            }
        }

        glUseProgram(state.solidProgram);
        glUniform4f(
            glGetUniformLocation(state.solidProgram, "color"),
            color.red,
            color.green,
            color.blue,
            color.alpha);
        glBindBuffer(GL_ARRAY_BUFFER, state.vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glLineWidth(thickness);
        glDrawArrays(GL_LINE_LOOP, 0, vertexCount);
    }

    void drawPage(mobileclock::renderer::NativeRenderer::State& state) {
        // Один кадр: очистить буфер, обновить данные VM, отрисовать дерево
        // контролов и показать готовый буфер через EGL.
        glClear(GL_COLOR_BUFFER_BIT);
        if (state.fontTexture == 0 || state.controlRenderer == nullptr) {
            return;
        }
        state.mainPageViewModel.UpdateClock();
        state.mainPageViewModel.Render(*state.controlRenderer);
        eglSwapBuffers(state.display, state.surface);
    }

    void destroyRenderer(mobileclock::renderer::NativeRenderer::State& state) {
        // Удаляем OpenGL-ресурсы, пока state.context ещё привязан к потоку.
        // Затем освобождаем EGL и ANativeWindow в обратном порядке владения.
        state.controlRenderer.reset();
        if (state.display != EGL_NO_DISPLAY && state.context != EGL_NO_CONTEXT) {
            eglMakeCurrent(state.display, state.surface, state.surface, state.context);
            if (state.fontTexture != 0) {
                glDeleteTextures(1, &state.fontTexture);
            }
            if (state.vertexBuffer != 0) {
                glDeleteBuffers(1, &state.vertexBuffer);
            }
            if (state.program != 0) {
                glDeleteProgram(state.program);
            }
            if (state.solidProgram != 0) {
                glDeleteProgram(state.solidProgram);
            }
            eglMakeCurrent(state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
        if (state.display != EGL_NO_DISPLAY) {
            if (state.context != EGL_NO_CONTEXT) {
                eglDestroyContext(state.display, state.context);
            }
            if (state.surface != EGL_NO_SURFACE) {
                eglDestroySurface(state.display, state.surface);
            }
            eglTerminate(state.display);
        }
        if (state.window != nullptr) {
            ANativeWindow_release(state.window);
        }
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
    NativeRenderer::NativeRenderer()
        : state(std::make_unique<State>()) {
        // См. NativeApplication: до nativeSetLogFile логгер ещё не настроен.
    }

    NativeRenderer::~NativeRenderer() {
        SurfaceDestroyed();
    }

    //
    // API
    //
    void NativeRenderer::SetLogFile(JNIEnv* env, jstring javaLogFilePath) {
        const char* utf8Path = env->GetStringUTFChars(javaLogFilePath, nullptr);
        if (utf8Path == nullptr) {
            return;
        }
        utility_helpers::android::configureLogFile(utf8Path);
        env->ReleaseStringUTFChars(javaLogFilePath, utf8Path);
    }

    void NativeRenderer::FlushLogs() {
        LOG_FUNCTION_SCOPE("NativeRenderer::FlushLogs");
        utility_helpers::android::flushLogging();
    }

    // Kotlin передаёт Android AssetManager один раз при старте Activity.
    void NativeRenderer::SetAssetManager(JNIEnv* env, jobject javaAssetManager) {
        LOG_FUNCTION_SCOPE("NativeRenderer::SetAssetManager");
        utility_helpers::android::initializeLogging("MobileClock");
        this->state->assetManager = AAssetManager_fromJava(env, javaAssetManager);
        LOG_INFO("Android AssetManager connected");
    }

    void NativeRenderer::SurfaceChanged(JNIEnv* env, jobject androidSurface, jint width, jint height) {
        LOG_FUNCTION_SCOPE("NativeRenderer::SurfaceChanged: {}x{}", width, height);
        utility_helpers::android::initializeLogging("MobileClock");
        LOG_INFO("Surface changed: {}x{}", width, height);
        State& state = *this->state;
        // Surface Android может быть пересоздан при повороте, сворачивании или
        // возвращении в приложение; прежние EGL-ресурсы к нему больше не годятся.
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

        // Размер viewport и layout совпадают с фактическим размером Android Surface.
        glViewport(0, 0, width, height);
        state.renderWidth = width;
        state.renderHeight = height;
        makeProgram(state);
        // Текстурный атлас хранит форму букв в alpha-канале. Без blending OpenGL
        // всё равно записывает RGB даже при alpha = 0 — это выглядело бы как
        // заполненные прямоугольники вокруг глифов.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        state.mainPageViewModel.Initialize({static_cast<float>(width), static_cast<float>(height)});
        makeFontAtlas(state);
        state.controlRenderer = std::make_unique<ControlRenderer>(
            // ControlRenderer отделяет UI-контролы от деталей OpenGL: контрол
            // передаёт прямоугольник и стиль, а callback строит GL-геометрию.
            [&state](const xaml::Rect& bounds, xaml::attr::Color color) {
                drawButtonOutline(state, bounds, color);
            },
            [&state](const xaml::Rect& bounds, xaml::attr::Color color, float cornerRadius) {
                drawRoundedRect(state, bounds, color, cornerRadius);
            },
            [&state](
                const xaml::Rect& bounds,
                xaml::attr::Color color,
                float cornerRadius,
                float thickness) {
                drawRoundedRectOutline(
                    state,
                    bounds,
                    color,
                    cornerRadius,
                    thickness);
            },
            [&state](
                const xaml::Rect& bounds,
                std::string_view text,
                xaml::attr::Color color,
                float fontSize,
                std::string_view fontWeight) {
                drawText(state, bounds, text, state.renderWidth, color, fontSize, fontWeight);
            },
            [](const xaml::Rect&, std::string_view, xaml::attr::Color) {
            },
            [&state](const xaml::Rect& bounds) {
                const int left = std::max(0, static_cast<int>(std::floor(bounds.x)));
                const int right = std::min(
                    state.renderWidth,
                    static_cast<int>(std::ceil(bounds.x + bounds.width)));
                const int top = std::max(0, static_cast<int>(std::floor(bounds.y)));
                const int bottom = std::min(
                    state.renderHeight,
                    static_cast<int>(std::ceil(bounds.y + bounds.height)));
                glEnable(GL_SCISSOR_TEST);
                glScissor(
                    left,
                    state.renderHeight - bottom,
                    std::max(0, right - left),
                    std::max(0, bottom - top));
            },
            []() {
                glDisable(GL_SCISSOR_TEST);
            });
        drawPage(state);
    }

    void NativeRenderer::SurfaceDestroyed() {
        LOG_FUNCTION_SCOPE("NativeRenderer::SurfaceDestroyed");
        LOG_INFO("Surface destroyed");
        destroyRenderer(*this->state);
    }

    void NativeRenderer::Touch(jint action, jfloat x, jfloat y) {
        LOG_FUNCTION_SCOPE("NativeRenderer::Touch: action={}, x={}, y={}", action, x, y);
        // Контрол захватывается на ACTION_DOWN: жест, начавшийся вне него,
        // не может активировать его при ACTION_UP.
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            this->state->mainPageViewModel.HandleTouchDown(x, y);
            return;
        }
        if (action == AMOTION_EVENT_ACTION_CANCEL) {
            this->state->mainPageViewModel.CancelTouch();
            return;
        }
        if (action != AMOTION_EVENT_ACTION_UP) {
            return;
        }
        if (!this->state->mainPageViewModel.HandleTouchUp(x, y)) {
            return;
        }
        drawPage(*this->state);
    }

    void NativeRenderer::Render() {
        if (this->state->display == EGL_NO_DISPLAY || this->state->surface == EGL_NO_SURFACE) {
            return;
        }
        drawPage(*this->state);
    }
}
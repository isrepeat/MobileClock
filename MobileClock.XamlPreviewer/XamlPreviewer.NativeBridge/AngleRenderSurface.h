#pragma once

#include <memory>
#include <string_view>

namespace xaml {
    class Element;
}

namespace mobileclock::ui {
    class ApplicationSession;
}

namespace xaml::bridge {
    // Изолирует EGL pbuffer и OpenGL ES ресурсы от C ABI native bridge.
    class AngleRenderSurface {
    public:
        AngleRenderSurface(
            int width,
            int height,
            std::string_view fontPath,
            std::string_view resourceRoot);
        ~AngleRenderSurface();

        AngleRenderSurface(const AngleRenderSurface&) = delete;
        AngleRenderSurface& operator=(const AngleRenderSurface&) = delete;

        void Render(
            Element& root,
            unsigned char* destination,
            int destinationStride);
        void Render(
            const mobileclock::ui::ApplicationSession& session,
            unsigned char* destination,
            int destinationStride);

    private:
        class Implementation;

    private:
        std::unique_ptr<Implementation> implementation;
    };
}
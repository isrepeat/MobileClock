#pragma once
#include <string_view>
#include <memory>

namespace xaml {
    class Element;
}

namespace mobileclock::application::core {
    class ApplicationSession;
}

namespace mobileclock::preview::rendering {
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
            xaml::Element& root,
            unsigned char* destination,
            int destinationStride);
        void Render(
            const mobileclock::application::core::ApplicationSession& session,
            unsigned char* destination,
            int destinationStride);

    private:
        class Implementation;

    private:
        std::unique_ptr<Implementation> implementation;
    };
}
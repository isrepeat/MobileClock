#pragma once

#include "XamlRuntime/RenderEngine.h"

#include <functional>
#include <string_view>

namespace mobileclock::renderer {
    // Общий GPU-интерфейс, доступный всем controls во время одного кадра.
    class ControlRenderer final : public xaml::IRenderBackend {
    public:
        using DrawOutlineCallback = std::function<void(const xaml::Rect&, xaml::attr::Color)>;
        using DrawRoundedRectCallback = std::function<void(const xaml::Rect&, xaml::attr::Color, float)>;
        using DrawRoundedRectOutlineCallback = std::function<void(
            const xaml::Rect&,
            xaml::attr::Color,
            float,
            float)>;
        using DrawTextCallback = std::function<void(
            const xaml::Rect&,
            std::string_view,
            xaml::attr::Color,
            float,
            std::string_view)>;
        using DrawImageCallback = std::function<void(
            const xaml::Rect&,
            std::string_view,
            xaml::attr::Color)>;
        using BeginClipCallback = std::function<void(const xaml::Rect&)>;
        using EndClipCallback = std::function<void()>;

        ControlRenderer(
            DrawOutlineCallback drawOutline,
            DrawRoundedRectCallback drawRoundedRect,
            DrawRoundedRectOutlineCallback drawRoundedRectOutline,
            DrawTextCallback drawText,
            DrawImageCallback drawImage,
            BeginClipCallback beginClip,
            EndClipCallback endClip);

        //
        // IRenderBackend
        //
        void BeginClip(const xaml::Rect& bounds) override;
        void EndClip() override;
        void DrawOutline(const xaml::Rect& bounds, xaml::attr::Color color) override;
        void DrawRoundedRect(
            const xaml::Rect& bounds,
            xaml::attr::Color color,
            float cornerRadius) override;
        void DrawRoundedRectOutline(
            const xaml::Rect& bounds,
            xaml::attr::Color color,
            float cornerRadius,
            float thickness) override;
        void DrawText(
            const xaml::Rect& bounds,
            std::string_view text,
            xaml::attr::Color color,
            float fontSize,
            std::string_view fontWeight) override;
        void DrawImage(
            const xaml::Rect& bounds,
            std::string_view source,
            xaml::attr::Color tint) override;

    private:
        DrawOutlineCallback drawOutline;
        DrawRoundedRectCallback drawRoundedRect;
        DrawRoundedRectOutlineCallback drawRoundedRectOutline;
        DrawTextCallback drawText;
        DrawImageCallback drawImage;
        BeginClipCallback beginClip;
        EndClipCallback endClip;
    };
}
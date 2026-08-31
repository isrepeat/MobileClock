#pragma once

#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <string_view>

namespace mobileclock::renderer {
    // Общий GPU-интерфейс, доступный всем controls во время одного кадра.
    class ControlRenderer {
    public:
        using DrawOutlineCallback = std::function<void(const xaml::Rect&, xaml::attr::Color)>;
        using DrawRoundedRectCallback = std::function<void(const xaml::Rect&, xaml::attr::Color, float)>;
        using DrawToggleSwitchCallback = std::function<void(const xaml::Rect&, bool)>;
        using DrawTextCallback = std::function<void(const xaml::Rect&, std::string_view, xaml::attr::Color)>;

        ControlRenderer(DrawOutlineCallback drawOutline, DrawRoundedRectCallback drawRoundedRect,
            DrawToggleSwitchCallback drawToggleSwitch, DrawTextCallback drawText);

        void DrawOutline(const xaml::Rect& bounds, xaml::attr::Color color) const;
        void DrawRoundedRect(const xaml::Rect& bounds, xaml::attr::Color color,
            float cornerRadius) const;
        void DrawToggleSwitch(const xaml::Rect& bounds, bool isOn) const;
        void DrawText(const xaml::Rect& bounds, std::string_view text,
            xaml::attr::Color color) const;

    private:
        DrawOutlineCallback drawOutline;
        DrawRoundedRectCallback drawRoundedRect;
        DrawToggleSwitchCallback drawToggleSwitch;
        DrawTextCallback drawText;
    };
}
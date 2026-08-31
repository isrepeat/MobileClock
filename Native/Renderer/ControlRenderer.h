#pragma once

#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <string_view>

namespace mobileclock::renderer {
    // Общий GPU-интерфейс, доступный всем controls во время одного кадра.
    class ControlRenderer {
    public:
        using DrawOutlineCallback = std::function<void(const mobileclock::ui::Rect&, mobileclock::ui::attr::Color)>;
        using DrawRoundedRectCallback = std::function<void(const mobileclock::ui::Rect&, mobileclock::ui::attr::Color, float)>;
        using DrawToggleSwitchCallback = std::function<void(const mobileclock::ui::Rect&, bool)>;
        using DrawTextCallback = std::function<void(const mobileclock::ui::Rect&, std::string_view, mobileclock::ui::attr::Color)>;

        ControlRenderer(DrawOutlineCallback drawOutline, DrawRoundedRectCallback drawRoundedRect,
            DrawToggleSwitchCallback drawToggleSwitch, DrawTextCallback drawText);

        //
        // API
        //
        void DrawOutline(const mobileclock::ui::Rect& bounds, mobileclock::ui::attr::Color color) const;
        void DrawRoundedRect(const mobileclock::ui::Rect& bounds, mobileclock::ui::attr::Color color,
            float cornerRadius) const;
        void DrawToggleSwitch(const mobileclock::ui::Rect& bounds, bool isOn) const;
        void DrawText(const mobileclock::ui::Rect& bounds, std::string_view text,
            mobileclock::ui::attr::Color color) const;

    private:
        DrawOutlineCallback drawOutline;
        DrawRoundedRectCallback drawRoundedRect;
        DrawToggleSwitchCallback drawToggleSwitch;
        DrawTextCallback drawText;
    };
}
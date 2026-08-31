#pragma once

#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <string_view>

namespace mobileclock::renderer {
    // Общий GPU-интерфейс, доступный всем controls во время одного кадра.
    class ControlRenderer {
    public:
        using DrawOutlineCallback = std::function<void(const mobileclock::ui::Rect&, mobileclock::ui::Color)>;
        using DrawTextCallback = std::function<void(std::string_view, mobileclock::ui::Color)>;

        ControlRenderer(DrawOutlineCallback drawOutline, DrawTextCallback drawText);

        void DrawOutline(const mobileclock::ui::Rect& bounds, mobileclock::ui::Color color) const;
        void DrawText(std::string_view text, mobileclock::ui::Color color) const;

    private:
        DrawOutlineCallback _drawOutline;
        DrawTextCallback _drawText;
    };
}
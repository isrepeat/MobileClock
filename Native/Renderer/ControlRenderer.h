#pragma once

#include "XamlRuntime/XamlLayout.h"

#include <functional>
#include <string_view>

namespace mobileclock::renderer {
    // Общий GPU-интерфейс, доступный всем controls во время одного кадра.
    class ControlRenderer {
    public:
        using DrawOutline = std::function<void(const mobileclock::ui::Rect&, mobileclock::ui::Color)>;
        using DrawText = std::function<void(std::string_view, mobileclock::ui::Color)>;

        ControlRenderer(DrawOutline drawOutline, DrawText drawText);

        void drawOutline(const mobileclock::ui::Rect& bounds, mobileclock::ui::Color color) const;
        void drawText(std::string_view text, mobileclock::ui::Color color) const;

    private:
        DrawOutline _drawOutline;
        DrawText _drawText;
    };
}
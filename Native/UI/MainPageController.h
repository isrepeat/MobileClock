#pragma once

#include "XamlRuntime/XamlLayout.h"
#include "Interfaces/IControl.h"

#include <vector>

namespace mobileclock::ui {
    class MainPageController {
    public:
        // Создаёт сгенерированное XAML-дерево и рассчитывает его расположение.
        void initialize(Size availableSize);
        // Меняет UI-состояние только при попадании тапа в кнопку.
        bool handleTap(float x, float y);
        void render(mobileclock::renderer::ControlRenderer& renderer) const;

    private:
        std::unique_ptr<Element> _page;
        std::vector<std::unique_ptr<IControl>> _controls;
    };
}
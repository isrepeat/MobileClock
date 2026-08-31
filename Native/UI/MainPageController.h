#pragma once

#include "XamlRuntime/XamlLayout.h"
#include "Interfaces/IControl.h"

#include <vector>

namespace mobileclock::ui {
    class MainPageController {
    public:
        // Создаёт сгенерированное XAML-дерево и рассчитывает его расположение.
        void Initialize(Size availableSize);
        void HandleTouchDown(float x, float y);
        bool HandleTouchUp(float x, float y);
        void CancelTouch();
        void Render(mobileclock::renderer::ControlRenderer& renderer) const;

    private:
        std::unique_ptr<Element> page;
        std::vector<std::unique_ptr<IControl>> controls;
        IControl* capturedControl = nullptr;
    };
}
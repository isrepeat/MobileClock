#include "UI/MainPageController.h"
#include "BuildVersion.h"
#include "Controls/ButtonControl.h"
#include "Renderer/ControlRenderer.h"

#include <Helpers/platform/Android/Logging.h>

#include <string>

namespace mobileclock::ui {
    void MainPageController::initialize(Size availableSize) {
        LOG_FUNCTION_SCOPE("MainPageController::initialize: {}x{}", availableSize.width, availableSize.height);
        // XAML-компилятор создаёт дерево, контроллер отвечает за его состояние.
        _page = createMainPage();
        _controls.clear();
        for (const auto& child : _page->children()) {
            if (child->type() == ElementType::button) {
                child->setText(std::string("Hello World v") + MOBILECLOCK_PACKAGE_VERSION);
                _controls.push_back(std::make_unique<ButtonControl>(*child));
            }
        }
        layout(*_page, availableSize);
    }

    bool MainPageController::handleTap(float x, float y) {
        LOG_FUNCTION_SCOPE("MainPageController::handleTap: x={}, y={}", x, y);
        for (const auto& control : _controls) {
            if (control->handleTap(x, y)) return true;
        }
        return false;
    }

    void MainPageController::render(mobileclock::renderer::ControlRenderer& renderer) const {
        for (const auto& control : _controls) {
            control->render(renderer);
        }
    }
}
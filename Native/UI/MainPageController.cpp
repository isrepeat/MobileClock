#include "UI/MainPageController.h"
#include "BuildVersion.h"
#include "!Generated/Xaml/MainPage.xaml.h"
#include "Controls/ButtonControl.h"
#include "Renderer/ControlRenderer.h"

#include <Helpers/platform/Android/Logging.h>

#include <string>

namespace mobileclock::ui {
    void MainPageController::Initialize(Size availableSize) {
        LOG_FUNCTION_SCOPE("MainPageController::Initialize: {}x{}", availableSize.width, availableSize.height);
        // XAML-компилятор создаёт дерево, контроллер отвечает за его состояние.
        _page = generated::MainPage::Create();
        _controls.clear();
        for (const auto& child : _page->Children()) {
            if (child->Type() == ElementType::button) {
                child->SetText(std::string("Hello World v") + MOBILECLOCK_PACKAGE_VERSION);
                _controls.push_back(std::make_unique<ButtonControl>(*child));
            }
        }
        layout(*_page, availableSize);
    }

    bool MainPageController::HandleTap(float x, float y) {
        LOG_FUNCTION_SCOPE("MainPageController::HandleTap: x={}, y={}", x, y);
        for (const auto& control : _controls) {
            if (control->HandleTap(x, y)) return true;
        }
        return false;
    }

    void MainPageController::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        for (const auto& control : _controls) {
            control->Render(renderer);
        }
    }
}
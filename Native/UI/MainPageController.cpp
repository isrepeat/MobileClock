#include "UI/MainPageController.h"
#include "BuildVersion.h"
#include "!Generated/Xaml/MainPage.xaml.h"
#include "Controls/ButtonControl.h"
#include "Controls/BorderControl.h"
#include "Controls/PageControl.h"
#include "Controls/ToggleSwitchControl.h"
#include "Renderer/ControlRenderer.h"

#include <Helpers/platform/Android/Logging.h>

#include <string>

namespace mobileclock::ui {
    void MainPageController::Initialize(Size availableSize) {
        LOG_FUNCTION_SCOPE("MainPageController::Initialize: {}x{}", availableSize.width, availableSize.height);
        // XAML-компилятор создаёт дерево, контроллер отвечает за его состояние.
        this->page = generated::MainPage::Create();
        this->controls.clear();
        this->capturedControl = nullptr;
        layout(*this->page, availableSize);
        const auto addControls = [this](const Element& element, const auto& visit) -> void {
            if (element.Type() == ElementType::page) {
                this->controls.push_back(std::make_unique<PageControl>(element));
            } else if (element.Type() == ElementType::button) {
                this->controls.push_back(std::make_unique<ButtonControl>(element));
            } else if (element.Type() == ElementType::border) {
                this->controls.push_back(std::make_unique<BorderControl>(element));
            } else if (element.Type() == ElementType::toggleSwitch) {
                this->controls.push_back(std::make_unique<ToggleSwitchControl>(element));
            }
            for (const auto& child : element.Children()) {
                visit(*child, visit);
            }
        };
        addControls(*this->page, addControls);
    }

    void MainPageController::HandleTouchDown(float x, float y) {
        LOG_FUNCTION_SCOPE("MainPageController::HandleTouchDown: x={}, y={}", x, y);
        this->capturedControl = nullptr;
        for (auto control = this->controls.rbegin(); control != this->controls.rend(); ++control) {
            if ((*control)->HitTest(x, y)) {
                this->capturedControl = control->get();
                return;
            }
        }
    }

    bool MainPageController::HandleTouchUp(float x, float y) {
        LOG_FUNCTION_SCOPE("MainPageController::HandleTouchUp: x={}, y={}", x, y);
        IControl* const control = this->capturedControl;
        this->capturedControl = nullptr;
        if (control == nullptr) {
            return false;
        }
        return control->HandleTap(x, y);
    }

    void MainPageController::CancelTouch() {
        this->capturedControl = nullptr;
    }

    void MainPageController::Render(mobileclock::renderer::ControlRenderer& renderer) const {
        for (const auto& control : this->controls) {
            control->Render(renderer);
        }
    }
}
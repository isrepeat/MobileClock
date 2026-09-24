#include "TimelineTabs.h"

#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif

namespace mobileclock::ui::control {
#if defined(ANDROID_APP_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view TimelineTabs::RuntimeClassName() const {
        return "mobileclock::ui::control::TimelineTabs";
    }

    bool TimelineTabs::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& runtimeBindingContext, std::string& diagnostics) {
        try {
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(templateNode.children.at(0), runtimeBindingContext,
                {this->Bounds().width, this->Bounds().height});
            if (runtimeBindingContext.prepareTree) {
                runtimeBindingContext.prepareTree(*result.root);
            }
              if (runtimeBindingContext.beforeCommit) {
                  runtimeBindingContext.beforeCommit();
              }
            this->ReplaceContent(std::move(result.root), std::move(result.bindings));
            diagnostics.clear();
            return true;
        } catch (const std::exception& error) {
            diagnostics = error.what();
            return false;
        }
    }
#endif
    void TimelineTabs::OnInitialized() {
    }
}
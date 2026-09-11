#include "MobileClock.UI/Controls/TimelineTabs.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif

namespace mobileclock::ui::controls {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view TimelineTabs::RuntimeClassName() const {
        return "mobileclock::ui::controls::TimelineTabs";
    }

    bool TimelineTabs::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) {
        try {
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(templateNode.children.at(0), context,
                {this->Bounds().width, this->Bounds().height});
            if (context.prepareTree) {
                context.prepareTree(*result.root);
            }
              if (context.beforeCommit) {
                  context.beforeCommit();
              }
              // TimelineTabs остаётся тем же native-экземпляром; меняется только
              // дерево его шаблона и связанный с ним набор runtime-подписок.
              this->ReplaceContent(std::move(result.root));
            this->runtimeBindings = std::move(result.bindings);
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
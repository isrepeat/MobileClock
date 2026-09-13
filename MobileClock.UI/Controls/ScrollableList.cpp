#include "MobileClock.UI/Controls/ScrollableList.h"

namespace mobileclock::ui::controls {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    std::string_view ScrollableList::RuntimeClassName() const {
        return "mobileclock::ui::controls::ScrollableList";
    }
#endif

    //
    // API
    //
    const xaml::DependentProperty<const void*>& ScrollableList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    //
    // Internal
    //
    std::string_view ScrollableList::ScrollViewerId() const {
        return "scrollableListScrollViewer";
    }
}
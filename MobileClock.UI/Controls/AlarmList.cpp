#include "MobileClock.UI/Controls/AlarmList.h"

namespace mobileclock::ui::controls {
    //
    // API
    //
    const xaml::DependentProperty<const void*>& AlarmList::ItemsSourceProperty() const {
        return this->itemsSource;
    }

    //
    // Internal
    //
    void AlarmList::OnInitialized() {
    }
}
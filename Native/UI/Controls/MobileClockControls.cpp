#include "UI/Controls/AlarmList.h"
#include "UI/Controls/ControlsRuntime.h"
#include "UI/Controls/MobileClockControls.h"

namespace mobileclock::ui::controls {
    void RegisterMobileClockControls(ControlsRuntime& runtime) {
        runtime.Register(AlarmList::CreateRebuildParticipant());
    }
}
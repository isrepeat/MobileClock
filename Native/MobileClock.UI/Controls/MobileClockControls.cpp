#include "MobileClockControls.h"

#include "ControlsRuntime.h"
#include "AlarmList.h"

namespace mobileclock::ui::controls {
    void RegisterMobileClockControls(ControlsRuntime& runtime) {
        runtime.Register(AlarmList::CreateRebuildParticipant());
    }
}
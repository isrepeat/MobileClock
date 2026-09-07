#include "Renderer/AndroidApplicationActions.h"
#include "Renderer/AndroidCommandDispatcher.h"

namespace mobileclock::renderer {
    AndroidApplicationActions::AndroidApplicationActions(AndroidCommandDispatcher& dispatcher)
        : dispatcher(dispatcher) {
    }

    //
    // IApplicationActions
    //
    void AndroidApplicationActions::CreateAlarm() {
        this->dispatcher.Dispatch(AndroidAction::createAlarm);
    }

    void AndroidApplicationActions::ToggleAlarm() {
        this->dispatcher.Dispatch(AndroidAction::toggleAlarm);
    }

    void AndroidApplicationActions::UpdateApplication() {
        this->dispatcher.Dispatch(AndroidAction::updateApplication);
    }

    void AndroidApplicationActions::UploadScreenshot() {
        this->dispatcher.Dispatch(AndroidAction::uploadScreenshot);
    }

    void AndroidApplicationActions::ShareLogs() {
        this->dispatcher.Dispatch(AndroidAction::shareLogs);
    }

    void AndroidApplicationActions::ExportLogs() {
        this->dispatcher.Dispatch(AndroidAction::exportLogs);
    }
}
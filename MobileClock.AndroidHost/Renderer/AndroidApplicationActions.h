#pragma once
#include "UI/ApplicationActions.h"

namespace mobileclock::renderer {
    class AndroidCommandDispatcher;

    class AndroidApplicationActions final : public ui::IApplicationActions {
    public:
        explicit AndroidApplicationActions(AndroidCommandDispatcher& dispatcher);
        ~AndroidApplicationActions() override = default;

        AndroidApplicationActions(const AndroidApplicationActions&) = delete;
        AndroidApplicationActions& operator=(const AndroidApplicationActions&) = delete;

        //
        // IApplicationActions
        //
        void CreateAlarm() override;
        void ToggleAlarm() override;
        void UpdateApplication() override;
        void UploadScreenshot() override;
        void ShareLogs() override;
        void ExportLogs() override;

    private:
        AndroidCommandDispatcher& dispatcher;
    };
}
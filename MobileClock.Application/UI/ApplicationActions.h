#pragma once

namespace mobileclock::ui {
    class IApplicationActions {
    public:
        virtual ~IApplicationActions() = default;

        virtual void CreateAlarm() = 0;
        virtual void ChooseAlarmMelody() = 0;
        virtual void ResetAlarmMelodySelection() = 0;
        virtual void ToggleAlarm() = 0;
        virtual void UpdateApplication() = 0;
        virtual void UploadScreenshot() = 0;
        virtual void ShareLogs() = 0;
        virtual void ExportLogs() = 0;
    };
}
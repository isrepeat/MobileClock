#pragma once
#include "../Model/AlarmRepository.h"
#include "Navigation.h"

#include <string_view>
#include <memory>
#include <string>

namespace mobileclock::application::core {
    class AlarmEditNavigationState final : public NavigationState<AlarmEditNavigationState> {
    public:
        inline static constexpr std::string_view DataTypeId = "mobileclock.alarm-edit";
        inline static constexpr bool IsRequired = true;

        AlarmEditNavigationState(std::string alarmId, model::Alarm settings);

#if defined(ANDROID_APP_PREVIEWER)
        static std::unique_ptr<base::NavigationStateBase> preview_CreatePreviewDefault();
#endif

#if defined(ANDROID_APP_PREVIEWER)
        std::string Serialize() const override;
        bool Deserialize(std::string_view json, std::string& error) override;
#endif

        const std::string& AlarmId() const;
        const model::Alarm& Settings() const;

    private:
        std::string alarmId;
        model::Alarm settings;
    };

#if defined(ANDROID_APP_PREVIEWER)
    class preview_AlarmMelodyNavigationState final : public NavigationState<preview_AlarmMelodyNavigationState> {
    public:
        inline static constexpr std::string_view DataTypeId = "mobileclock.alarm-melody";
        inline static constexpr bool IsRequired = false;

        explicit preview_AlarmMelodyNavigationState(model::AlarmMelody alarmMelody);

#if defined(ANDROID_APP_PREVIEWER)
        static std::unique_ptr<base::NavigationStateBase> preview_CreatePreviewDefault();
#endif

        std::string Serialize() const override;
        bool Deserialize(std::string_view json, std::string& error) override;
        const model::AlarmMelody& Melody() const;

    private:
        model::AlarmMelody alarmMelody;
    };
#endif
}
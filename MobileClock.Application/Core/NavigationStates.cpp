#include "NavigationStates.h"

#include <JsonParser/json_struct/json_struct.h>

#include <utility>
#include <format>

namespace mobileclock::application::core {
    namespace _details {
        struct AlarmEditNavigationStateDocument final {
            std::string AlarmId;
            int Hour = 7;
            int Minute = 30;

            JS_OBJECT(
                JS_MEMBER_ALIASES(AlarmId, "alarmId"),
                JS_MEMBER_ALIASES(Hour, "hour"),
                JS_MEMBER_ALIASES(Minute, "minute")
            );
        };

        struct AlarmMelodyNavigationStateDocument final {
            std::string Id;
            std::string Name;
            std::string Uri;

            JS_OBJECT(
                JS_MEMBER_ALIASES(Id, "id"),
                JS_MEMBER_ALIASES(Name, "name"),
                JS_MEMBER_ALIASES(Uri, "uri")
            );
        };
    } // namespace _details

    AlarmEditNavigationState::AlarmEditNavigationState(std::string alarmId, model::Alarm settings)
        : alarmId(std::move(alarmId))
        , settings(std::move(settings)) {
    }

#if defined(ANDROID_APP_PREVIEWER)
    std::unique_ptr<base::NavigationStateBase> AlarmEditNavigationState::preview_CreatePreviewDefault() {
        model::Alarm alarm;
        alarm.id = "preview-alarm";
        return std::make_unique<AlarmEditNavigationState>(alarm.id, alarm);
    }
#endif

#if defined(ANDROID_APP_PREVIEWER)
    std::string AlarmEditNavigationState::Serialize() const {
        _details::AlarmEditNavigationStateDocument alarmEditNavigationStateDocument;
        alarmEditNavigationStateDocument.AlarmId = this->alarmId;
        alarmEditNavigationStateDocument.Hour = this->settings.hour;
        alarmEditNavigationStateDocument.Minute = this->settings.minute;
        return JS::serializeStruct(alarmEditNavigationStateDocument);
    }

    bool AlarmEditNavigationState::Deserialize(std::string_view json, std::string& error) {
        _details::AlarmEditNavigationStateDocument alarmEditNavigationStateDocument;
        JS::ParseContext parseContext(json.data(), json.size());
        if (parseContext.parseTo(alarmEditNavigationStateDocument) != JS::Error::NoError || alarmEditNavigationStateDocument.AlarmId.empty()) {
            error = alarmEditNavigationStateDocument.AlarmId.empty()
                ? "Navigation data requires alarmId"
                : std::format("Invalid alarm edit navigation data: {}", parseContext.makeErrorString());
            return false;
        }
        this->alarmId = std::move(alarmEditNavigationStateDocument.AlarmId);
        this->settings = model::Alarm{};
        this->settings.id = this->alarmId;
        this->settings.hour = alarmEditNavigationStateDocument.Hour;
        this->settings.minute = alarmEditNavigationStateDocument.Minute;
        return true;
    }
#endif

    //
    // API
    //
    const std::string& AlarmEditNavigationState::AlarmId() const {
        return this->alarmId;
    }

    const model::Alarm& AlarmEditNavigationState::Settings() const {
        return this->settings;
    }

#if defined(ANDROID_APP_PREVIEWER)
    preview_AlarmMelodyNavigationState::preview_AlarmMelodyNavigationState(model::AlarmMelody alarmMelody)
        : alarmMelody(std::move(alarmMelody)) {
    }

#if defined(ANDROID_APP_PREVIEWER)
    std::unique_ptr<base::NavigationStateBase> preview_AlarmMelodyNavigationState::preview_CreatePreviewDefault() {
        return std::make_unique<preview_AlarmMelodyNavigationState>(model::AlarmMelody{
            "preview-melody",
            "Preview melody",
            "preview://navigation/default-melody",
        });
    }
#endif

    std::string preview_AlarmMelodyNavigationState::Serialize() const {
        _details::AlarmMelodyNavigationStateDocument alarmMelodyNavigationStateDocument;
        alarmMelodyNavigationStateDocument.Id = this->alarmMelody.id;
        alarmMelodyNavigationStateDocument.Name = this->alarmMelody.name;
        alarmMelodyNavigationStateDocument.Uri = this->alarmMelody.uri;
        return JS::serializeStruct(alarmMelodyNavigationStateDocument);
    }

    bool preview_AlarmMelodyNavigationState::Deserialize(std::string_view json, std::string& error) {
        _details::AlarmMelodyNavigationStateDocument alarmMelodyNavigationStateDocument;
        JS::ParseContext parseContext(json.data(), json.size());
        if (parseContext.parseTo(alarmMelodyNavigationStateDocument) != JS::Error::NoError || alarmMelodyNavigationStateDocument.Id.empty()) {
            error = alarmMelodyNavigationStateDocument.Id.empty()
                ? "Navigation data requires id"
                : std::format("Invalid alarm melody navigation data: {}", parseContext.makeErrorString());
            return false;
        }
        this->alarmMelody = {std::move(alarmMelodyNavigationStateDocument.Id), std::move(alarmMelodyNavigationStateDocument.Name), std::move(alarmMelodyNavigationStateDocument.Uri)};
        return true;
    }

    //
    // API
    //
    const model::AlarmMelody& preview_AlarmMelodyNavigationState::Melody() const {
        return this->alarmMelody;
    }
#endif
}
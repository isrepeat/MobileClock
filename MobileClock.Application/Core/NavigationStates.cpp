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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    std::unique_ptr<base::NavigationStateBase> AlarmEditNavigationState::CreatePreviewDefault() {
        model::Alarm alarm;
        alarm.id = "preview-alarm";
        return std::make_unique<AlarmEditNavigationState>(alarm.id, alarm);
    }
#endif

    std::string AlarmEditNavigationState::Serialize() const {
        _details::AlarmEditNavigationStateDocument document;
        document.AlarmId = this->alarmId;
        document.Hour = this->settings.hour;
        document.Minute = this->settings.minute;
        return JS::serializeStruct(document);
    }

    bool AlarmEditNavigationState::Deserialize(std::string_view json, std::string& error) {
        _details::AlarmEditNavigationStateDocument document;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(document) != JS::Error::NoError || document.AlarmId.empty()) {
            error = document.AlarmId.empty()
                ? "Navigation data requires alarmId"
                : std::format("Invalid alarm edit navigation data: {}", context.makeErrorString());
            return false;
        }
        this->alarmId = std::move(document.AlarmId);
        this->settings = model::Alarm{};
        this->settings.id = this->alarmId;
        this->settings.hour = document.Hour;
        this->settings.minute = document.Minute;
        return true;
    }

    //
    // API
    //
    const std::string& AlarmEditNavigationState::AlarmId() const {
        return this->alarmId;
    }

    const model::Alarm& AlarmEditNavigationState::Settings() const {
        return this->settings;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    AlarmMelodyNavigationState::AlarmMelodyNavigationState(model::AlarmMelody alarmMelody)
        : alarmMelody(std::move(alarmMelody)) {
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    std::unique_ptr<base::NavigationStateBase> AlarmMelodyNavigationState::CreatePreviewDefault() {
        return std::make_unique<AlarmMelodyNavigationState>(model::AlarmMelody{
            "preview-melody",
            "Preview melody",
            "preview://navigation/default-melody",
        });
    }
#endif

    std::string AlarmMelodyNavigationState::Serialize() const {
        _details::AlarmMelodyNavigationStateDocument document;
        document.Id = this->alarmMelody.id;
        document.Name = this->alarmMelody.name;
        document.Uri = this->alarmMelody.uri;
        return JS::serializeStruct(document);
    }

    bool AlarmMelodyNavigationState::Deserialize(std::string_view json, std::string& error) {
        _details::AlarmMelodyNavigationStateDocument document;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(document) != JS::Error::NoError || document.Id.empty()) {
            error = document.Id.empty()
                ? "Navigation data requires id"
                : std::format("Invalid alarm melody navigation data: {}", context.makeErrorString());
            return false;
        }
        this->alarmMelody = {std::move(document.Id), std::move(document.Name), std::move(document.Uri)};
        return true;
    }

    //
    // API
    //
    const model::AlarmMelody& AlarmMelodyNavigationState::Melody() const {
        return this->alarmMelody;
    }
#endif
}
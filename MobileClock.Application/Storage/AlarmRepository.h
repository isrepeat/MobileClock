#pragma once
#define JS_STL_ARRAY
#include <JsonParser/json_struct/json_struct.h>

#include "Storage/ApplicationStateStore.h"

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace mobileclock::ui {
    struct AlarmMelody final {
        std::string id;
        std::string name = "Рассвет";
        std::string uri;

        JS_OBJECT(
            JS_MEMBER_ALIASES(id, "Id", "id"),
            JS_MEMBER_ALIASES(name, "Name", "name"),
            JS_MEMBER_ALIASES(uri, "Uri", "uri")
        );

        bool operator==(const AlarmMelody&) const = default;
    };

    struct Alarm final {
        std::string id;
        int hour = 7;
        int minute = 30;
        std::array<bool, 7> days{true, true, true, true, true, false, false};
        std::string melodyId;
        bool vibration = true;
        bool isEnabled = true;

        JS_OBJECT(
            JS_MEMBER_ALIASES(id, "Id", "id"),
            JS_MEMBER_ALIASES(hour, "Hour", "hour"),
            JS_MEMBER_ALIASES(minute, "Minute", "minute"),
            JS_MEMBER_ALIASES(days, "Days", "days"),
            JS_MEMBER_ALIASES(melodyId, "MelodyId", "melodyId"),
            JS_MEMBER_ALIASES(vibration, "Vibration", "vibration"),
            JS_MEMBER_ALIASES(isEnabled, "IsEnabled", "isEnabled")
        );

        bool operator==(const Alarm&) const = default;
    };

    struct ApplicationStateDocument final {
        int version = 1;
        std::vector<Alarm> alarms;
        std::vector<AlarmMelody> alarmMelodies;

        JS_OBJECT(
            JS_MEMBER(version),
            JS_MEMBER(alarms),
            JS_MEMBER(alarmMelodies)
        );
    };

    class AlarmRepository final {
    public:
        explicit AlarmRepository(ApplicationStateStore& store);
        ~AlarmRepository() = default;

        AlarmRepository(const AlarmRepository&) = delete;
        AlarmRepository& operator=(const AlarmRepository&) = delete;

        const std::vector<Alarm>& Alarms() const;
        bool CreateAlarm(const Alarm& alarm);
        bool UpdateAlarm(std::string_view id, const Alarm& alarm);
        bool SetAlarmEnabled(std::string_view id, bool value);
        bool RemoveAlarm(std::string_view id);

    private:
        ApplicationStateStore& store;
        std::vector<Alarm> alarms;
        bool Commit(std::vector<Alarm> candidate);
    };

    class AlarmMelodyRepository final {
    public:
        using Unsubscribe = std::function<void()>;
        using ChangeHandler = std::function<void()>;

        explicit AlarmMelodyRepository(ApplicationStateStore& store);
        ~AlarmMelodyRepository() = default;

        AlarmMelodyRepository(const AlarmMelodyRepository&) = delete;
        AlarmMelodyRepository& operator=(const AlarmMelodyRepository&) = delete;

        const std::vector<AlarmMelody>& Melodies() const;
        bool SaveMelody(AlarmMelody& value);
        bool DeleteMelody(std::string_view uri);
        bool ClearMelodies();
        Unsubscribe Subscribe(ChangeHandler handler);

    private:
        bool Commit(std::vector<AlarmMelody> candidate);
        void Notify();

    private:
        ApplicationStateStore& store;
        std::vector<AlarmMelody> melodies;
        std::vector<ChangeHandler> handlers;
    };
}
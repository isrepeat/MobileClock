#include "AlarmViewModel.h"

#include <algorithm>
#include <utility>
#include <format>
#include <chrono>
#include <atomic>
#include <array>

namespace mobileclock::application::ui::view_model::_details {
    std::string CreateAlarmViewModelId() {
        static std::atomic_uint64_t sequence;
        return std::format(
            "alarm-{}-{}",
            std::chrono::steady_clock::now().time_since_epoch().count(),
            ++sequence);
    }
} // namespace _details

namespace mobileclock::application::ui::view_model {
    AlarmViewModel::AlarmViewModel(
        std::string time,
        std::string repeat,
        bool isEnabled,
        model::AlarmMelody melody)
        : time(std::move(time))
        , repeat(std::move(repeat))
        , isEnabled(isEnabled) {
        this->id = _details::CreateAlarmViewModelId();
        if (this->time.size() == 5) {
            this->alarm.hour = (this->time[0] - '0') * 10 + this->time[1] - '0';
            this->alarm.minute = (this->time[3] - '0') * 10 + this->time[4] - '0';
        }
        this->alarm.days.fill(this->repeat == "Ежедневно");
        constexpr std::array<std::string_view, 7> names{"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
        for (size_t index = 0; index < names.size(); ++index) {
            this->alarm.days[index] = this->alarm.days[index] || this->repeat.find(names[index]) != std::string::npos;
        }
        if (!melody.name.empty()) {
            this->alarm.melodyId = melody.id;
        }
    }

    AlarmViewModel::AlarmViewModel(std::string id, model::Alarm alarm, bool isEnabled)
        : id(id.empty() ? _details::CreateAlarmViewModelId() : std::move(id))
        , alarm(std::move(alarm))
        , time(std::format("{:02}:{:02}", this->alarm.hour, this->alarm.minute))
        , isEnabled(isEnabled) {
        constexpr std::array<std::string_view, 7> names{"Пн", "Вт", "Ср", "Чт", "Пт", "Сб", "Вс"};
        for (size_t index = 0; index < this->alarm.days.size(); ++index) {
            if (this->alarm.days[index]) {
                if (!this->repeat.empty()) {
                    this->repeat += ", ";
                }
                this->repeat += names[index];
            }
        }
        if (this->repeat.empty()) {
            this->repeat = "Однократно";
        }
        else if (std::all_of(this->alarm.days.begin(), this->alarm.days.end(), [](bool day) {
            return day;
        })) {
            this->repeat = "Ежедневно";
        }
    }

    AlarmViewModel::AlarmViewModel(const model::Alarm& alarm)
        : AlarmViewModel({}, alarm, true) {
    }

    //
    // API
    //
    const std::string& AlarmViewModel::Id() const {
        return this->id;
    }

    const model::Alarm& AlarmViewModel::Settings() const {
        return this->alarm;
    }

    const std::string& AlarmViewModel::Time() const {
        return this->time;
    }

    const std::string& AlarmViewModel::Repeat() const {
        return this->repeat;
    }

    bool AlarmViewModel::IsEnabled() const {
        return this->isEnabled;
    }

    xaml::Element::Command AlarmViewModel::AlarmBlockCommand() const {
        return this->alarmBlockCommand;
    }

    xaml::Element::Command AlarmViewModel::ToggleAlarmCommand() const {
        return this->toggleAlarmCommand;
    }

    void AlarmViewModel::SetIsEnabled(bool value) {
        this->isEnabled = value;
    }

    void AlarmViewModel::SetAlarmBlockCommand(xaml::Element::Command value) {
        this->alarmBlockCommand = std::move(value);
    }

    void AlarmViewModel::SetToggleAlarmCommand(xaml::Element::Command value) {
        this->toggleAlarmCommand = std::move(value);
    }
}
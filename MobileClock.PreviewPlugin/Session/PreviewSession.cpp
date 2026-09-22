#include "PreviewSession.h"

#include <JsonParser/json_struct/json_struct.h>

#include "MobileClock.Application/Model/AlarmRepository.h"
#include <Helpers.Logging/Logging.h>
#include <HelpersNew/Platform/Windows/WindowsApi.h>

#include "PreviewNavigationController.h"

#include <string_view>
#include <filesystem>
#include <stdexcept>
#include <fstream>
#include <utility>
#include <cctype>
#include <string>
#include <vector>

namespace mobileclock::preview::session::_details {
    namespace model = mobileclock::application::model;

    std::filesystem::path PreviewerStatePath() {
        std::vector<wchar_t> executablePath(MAX_PATH);
        DWORD length = 0;
        do {
            length = GetModuleFileNameW(nullptr, executablePath.data(), static_cast<DWORD>(executablePath.size()));
            if (length == 0) {
                throw std::runtime_error("Cannot locate AndroidAppPreviewer executable");
            }
            if (length < executablePath.size() - 1) {
                break;
            }
            executablePath.resize(executablePath.size() * 2);
        } while (true);
        return std::filesystem::path(std::wstring(executablePath.data(), length)).parent_path() / "mobileclock-storage-previewer.json";
    }

    class PreviewerStateStorage final {
    public:
        explicit PreviewerStateStorage(std::filesystem::path path)
            : path(std::move(path)) {
        }

        model::ApplicationStateDocument Load() const {
            std::ifstream stream(this->path, std::ios::binary);
            if (!stream) {
                return {};
            }
            const std::string json{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
            model::ApplicationStateDocument document;
            JS::ParseContext context(json.data(), json.size());
            return context.parseTo(document) == JS::Error::NoError
                ? document : model::ApplicationStateDocument{};
        }

        bool Save(const model::ApplicationStateDocument& data) const {
            const std::filesystem::path temporaryPath = this->path.string() + ".tmp";
            std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
            if (!stream) {
                LOG_WARNING("AndroidAppPreviewer.State", "Cannot save previewer state '{}'", this->path.string());
                return false;
            }
            stream << JS::serializeStruct(data);
            stream.flush();
            if (!stream) {
                LOG_WARNING("AndroidAppPreviewer.State", "Cannot flush previewer state '{}'", temporaryPath.string());
                return false;
            }
            stream.close();
            if (MoveFileExW(temporaryPath.c_str(), this->path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
                LOG_WARNING("AndroidAppPreviewer.State", "Cannot replace previewer state '{}'", this->path.string());
                return false;
            }
            return true;
        }

    private:
        std::filesystem::path path;
    };
}

namespace mobileclock::preview::session {
    class PreviewSession::State final {
    public:
        explicit State(int width, int height)
            : stateStorage(_details::PreviewerStatePath())
            , stateStore(stateStorage.Load(), [this](const application::model::ApplicationStateDocument& data) {
                return this->stateStorage.Save(data);
            })
            , alarmRepository(stateStore)
            , alarmMelodyRepository(stateStore)
            , appSessionController(alarmRepository, alarmMelodyRepository)
            , navigation(appSessionController.Session()) {
            if (width <= 0 || height <= 0) {
                throw std::invalid_argument("Session dimensions must be positive");
            }
            this->appSessionController.SetHostEventHandler([this](
                application::core::AppSessionSignal signal,
                const application::core::AppSessionSignalData&) {
                if (signal != application::core::AppSessionSignal::requestAlarmMelody) {
                    return;
                }
                std::string error;
                if (!this->appSessionController.Session().NavigatePreviewRoute(
                    application::ui::page::XiaomiThemesPageViewModel::PageName,
                    error)) {
                    LOG_WARNING("AndroidAppPreviewer.Session", "Cannot open Xiaomi Themes: {}", error);
                }
            });
            this->appSessionController.Session().Initialize({static_cast<float>(width), static_cast<float>(height)});
        }

        _details::PreviewerStateStorage stateStorage;
        application::core::ApplicationStateStore stateStore;
        application::model::AlarmRepository alarmRepository;
        application::model::AlarmMelodyRepository alarmMelodyRepository;
        application::core::AppSessionController appSessionController;
        PreviewNavigationController navigation;
    };

    PreviewSession::PreviewSession(int width, int height)
        : state(std::make_unique<State>(width, height)) {
    }

    PreviewSession::~PreviewSession() = default;

    mobileclock::application::core::ApplicationSession& PreviewSession::Session() {
        return this->state->appSessionController.Session();
    }

    mobileclock::application::core::AppSessionController& PreviewSession::Controller() {
        return this->state->appSessionController;
    }

    PreviewNavigationController& PreviewSession::Navigation() {
        return this->state->navigation;
    }

    bool PreviewSession::ExportState() {
        if (!this->state->alarmRepository.SavePreviewStateToPersistentStorage()) {
            return false;
        }
        this->state->alarmMelodyRepository.ReloadFromStateStore();
        return true;
    }

    bool PreviewSession::CanSaveState() const {
        return !this->state->alarmRepository.IsPreviewSessionDocumentEquivalentTo(this->state->stateStorage.Load());
    }

    void PreviewSession::Resize(int width, int height) {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("Session dimensions must be positive");
        }
        this->Session().Initialize({static_cast<float>(width), static_cast<float>(height)});
    }

    std::vector<std::string> PreviewSession::ParseNavigationTransitionIds(std::string_view json) {
        const size_t property = json.find("\"transitionIds\"");
        if (property == std::string_view::npos) {
            throw std::invalid_argument("Navigation request does not contain transitionIds");
        }
        const size_t arrayStart = json.find('[', property);
        const size_t arrayEnd = arrayStart == std::string_view::npos ? std::string_view::npos : json.find(']', arrayStart);
        if (arrayStart == std::string_view::npos || arrayEnd == std::string_view::npos) {
            throw std::invalid_argument("Navigation request contains an invalid transitionIds array");
        }
        std::vector<std::string> result;
        size_t position = arrayStart + 1;
        while (position < arrayEnd) {
            while (position < arrayEnd && std::isspace(static_cast<unsigned char>(json[position]))) {
                ++position;
            }
            if (position == arrayEnd) {
                break;
            }
            if (json[position] != '\"') {
                throw std::invalid_argument("Navigation transition ID must be a JSON string");
            }
            const size_t valueStart = ++position;
            const size_t valueEnd = json.find('\"', valueStart);
            if (valueEnd == std::string_view::npos || valueEnd > arrayEnd) {
                throw std::invalid_argument("Navigation transition ID is not terminated");
            }
            if (json.substr(valueStart, valueEnd - valueStart).find('\\') != std::string_view::npos) {
                throw std::invalid_argument("Navigation transition ID must not contain JSON escapes");
            }
            result.emplace_back(json.substr(valueStart, valueEnd - valueStart));
            position = valueEnd + 1;
            while (position < arrayEnd && std::isspace(static_cast<unsigned char>(json[position]))) {
                ++position;
            }
            if (position < arrayEnd) {
                if (json[position] != ',') {
                    throw std::invalid_argument("Navigation transition IDs must be comma-separated");
                }
                ++position;
            }
        }
        if (result.empty()) {
            throw std::invalid_argument("Navigation request does not contain transition IDs");
        }
        return result;
    }
}
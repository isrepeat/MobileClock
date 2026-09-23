#pragma once
#include "../Core/ApplicationStateStore.h"

#include <string>

namespace mobileclock::application::model {
    struct ApplicationStateDocument;
}

namespace mobileclock::application::base {
    class AppRepositoryBase {
    public:
        virtual ~AppRepositoryBase() = default;

#if defined(ANDROID_APP_PREVIEWER)
        void preview_LoadScenarioState(model::ApplicationStateDocument applicationStateDocument);
        bool preview_SaveStateToPersistentStorage();
        virtual bool preview_IsSessionDocumentEquivalentTo(const model::ApplicationStateDocument& applicationStateDocument) const = 0;
        virtual void preview_ReloadFromStateStore() = 0;
#endif

    protected:
        explicit AppRepositoryBase(core::ApplicationStateStore& applicationStateStore);

        const model::ApplicationStateDocument& State() const;
        bool Commit(model::ApplicationStateDocument applicationStateDocument);
        static std::string CreateAlarmId();
        static std::string CreateMelodyId();

    private:
        core::ApplicationStateStore& applicationStateStore;
    };
}
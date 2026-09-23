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
        void preview_LoadScenarioState(model::ApplicationStateDocument document);
        bool preview_SaveStateToPersistentStorage();
        virtual bool preview_IsSessionDocumentEquivalentTo(const model::ApplicationStateDocument& document) const = 0;
        virtual void preview_ReloadFromStateStore() = 0;
#endif

    protected:
        explicit AppRepositoryBase(core::ApplicationStateStore& store);

        const model::ApplicationStateDocument& State() const;
        bool Commit(model::ApplicationStateDocument document);
        static std::string CreateAlarmId();
        static std::string CreateMelodyId();

    private:
        core::ApplicationStateStore& store;
    };
}
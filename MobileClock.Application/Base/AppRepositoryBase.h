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

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        void LoadPreviewScenarioState(model::ApplicationStateDocument document);
        bool SavePreviewStateToPersistentStorage();
        virtual bool IsPreviewSessionDocumentEquivalentTo(const model::ApplicationStateDocument& document) const = 0;
        virtual void ReloadFromStateStore() = 0;
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
#pragma once
#include <functional>
#include <memory>

namespace mobileclock::application::model {
    struct ApplicationStateDocument;
}

namespace mobileclock::application::core {
    class ApplicationStateStore final {
    public:
        using Persistence = std::function<bool(const model::ApplicationStateDocument&)>;

        explicit ApplicationStateStore(model::ApplicationStateDocument document, Persistence persistence = {});
        ~ApplicationStateStore();

        ApplicationStateStore(const ApplicationStateStore&) = delete;
        ApplicationStateStore& operator=(const ApplicationStateStore&) = delete;

        const model::ApplicationStateDocument& State() const;
        bool Write(model::ApplicationStateDocument candidate);

    private:
        std::unique_ptr<model::ApplicationStateDocument> document;
        Persistence persistence;
    };
}
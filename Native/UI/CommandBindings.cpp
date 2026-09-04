#include "UI/CommandBindings.h"

namespace mobileclock::ui {
    //
    // API
    //
    void CommandBindings::Bind(std::string name, Handler handler) {
        this->handlers.insert_or_assign(std::move(name), std::move(handler));
    }

    bool CommandBindings::Execute(std::string_view name) const {
        const auto handler = this->handlers.find(std::string(name));
        if (handler == this->handlers.end()) {
            return false;
        }
        handler->second();
        return true;
    }
}
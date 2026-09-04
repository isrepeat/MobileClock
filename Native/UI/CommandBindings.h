#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace mobileclock::ui {
    class CommandBindings final {
    public:
        using Handler = std::function<void()>;

        void Bind(std::string name, Handler handler);
        bool Execute(std::string_view name) const;

    private:
        std::unordered_map<std::string, Handler> handlers;
    };
}
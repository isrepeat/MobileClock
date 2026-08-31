#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <variant>

namespace mobileclock::ui {
    using IViewModelValue = std::variant<bool, std::string>;

    class IViewModel {
    public:
        using PropertyChangedHandler = std::function<void(std::string_view)>;
        using Unsubscribe = std::function<void()>;

        virtual ~IViewModel() = default;

        virtual IViewModelValue GetValue(std::string_view path) const = 0;
        virtual void SetValue(std::string_view path, IViewModelValue value) = 0;
        virtual Unsubscribe Subscribe(PropertyChangedHandler handler) = 0;
    };
}
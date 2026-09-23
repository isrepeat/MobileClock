#pragma once
#include "../Interface/ISerializable.h"

#include <string_view>
#include <string>

namespace mobileclock::application::base {
    // Полиморфная нагрузка перехода. Данные существуют отдельно от страниц, чтобы
    // один и тот же контракт работал и для UI, и для инструментов построения графа.
    class NavigationStateBase : public interface::ISerializable {
    public:
        virtual ~NavigationStateBase() = default;

        virtual std::string_view TypeId() const = 0;
    };
}
#pragma once
#if defined(ANDROID_APP_PREVIEWER)
#include "../Interface/ISerializable.h"
#endif

#include <string_view>
#include <string>

namespace mobileclock::application::base {
    // Полиморфная нагрузка перехода. Данные существуют отдельно от страниц, чтобы
    // один и тот же контракт работал и для UI, и для инструментов построения графа.
    class NavigationStateBase
#if defined(ANDROID_APP_PREVIEWER)
        : public interface::ISerializable
#endif
    {
    public:
        virtual ~NavigationStateBase() = default;

        virtual std::string_view TypeId() const = 0;
    };
}
#pragma once
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Binding.h>

#include "UI/ISerializable.h"
#include "UI/PageRegistry.h"
#include "UI/Navigation.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class StatisticsPageViewModel final : public ISerializable {
    public:
        inline static constexpr std::string_view PageName = "StatisticsPage";
        inline static constexpr std::string_view PreviewGraphTitle = "Статистика";

        enum class Property {
            title,
            summary,
        };

        using PropertyChangedHandler = std::function<void(Property)>;
        using Unsubscribe = std::function<void()>;

        explicit StatisticsPageViewModel(PageContext& context);
        ~StatisticsPageViewModel() = default;

        StatisticsPageViewModel(const StatisticsPageViewModel&) = delete;
        StatisticsPageViewModel& operator=(const StatisticsPageViewModel&) = delete;

        const std::string& Title() const;
        const std::string& Summary() const;
        xaml::Element::Command NavigateToMainCommand() const;
        void Initialize(xaml::Size availableSize);
        void HandleTap(xaml::Element& element);
        void Update();
        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const;
        xaml::Element& Root();
        Unsubscribe Subscribe(PropertyChangedHandler handler);

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        bool Deserialize(std::string_view json, std::string& error) override;
        xaml::runtime::RuntimeBindingContext RuntimeContext();
        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result);
#endif

    private:
        void NotifyPropertyChanged(Property property);

    private:
        std::string title = "Статистика";
        std::string summary = "Нет данных";
        std::vector<PropertyChangedHandler> propertyChangedHandlers;
        std::unique_ptr<xaml::Element> page;
        xaml::BindingScope bindings;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        std::unique_ptr<xaml::BindingScope> runtimeBindings;
#endif
        xaml::Element::Command navigateToMainCommand;
    };
}
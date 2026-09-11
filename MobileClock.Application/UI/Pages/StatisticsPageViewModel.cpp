#include "UI/Pages/StatisticsPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#include <JsonParser/JsonParser.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/StatisticsPage.xaml.h"
#include "UI/Pages/MainPageViewModel.h"

#include <utility>
#include <format>

namespace mobileclock::ui {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    namespace _details {
        struct StatisticsPagePreviewScenario final {
            std::optional<std::string> Title = "Статистика";
            std::optional<std::string> Summary = "Нет данных";

            JS_OBJECT(JS_MEMBER(Title), JS_MEMBER(Summary));
        };
    }
#endif

    StatisticsPageViewModel::StatisticsPageViewModel(PageContext& context)
        : navigateToMainCommand([&context]() {
            context.navigator.Navigate<MainPageViewModel>();
        }) {
    }

    //
    // API
    //
    const std::string& StatisticsPageViewModel::Title() const {
        return this->title;
    }

    const std::string& StatisticsPageViewModel::Summary() const {
        return this->summary;
    }

    xaml::Element::Command StatisticsPageViewModel::NavigateToMainCommand() const {
        return this->navigateToMainCommand;
    }

    void StatisticsPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        this->runtimeBindings.reset();
#endif
        this->page = xaml::generated::StatisticsPage::Create(*this, this->bindings);
        xaml::layout(*this->page, availableSize);
    }

    void StatisticsPageViewModel::HandleTap(xaml::Element& element) {
        this->bindings.UpdateSource(element);
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        if (this->runtimeBindings) {
            this->runtimeBindings->UpdateSource(element);
        }
#endif
        element.ExecuteCommand();
    }

    void StatisticsPageViewModel::Update() {
    }

    void StatisticsPageViewModel::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        xaml::Render(*this->page, renderer, renderers);
    }

    xaml::Element& StatisticsPageViewModel::Root() {
        return *this->page;
    }

    StatisticsPageViewModel::Unsubscribe StatisticsPageViewModel::Subscribe(PropertyChangedHandler handler) {
        this->propertyChangedHandlers.push_back(std::move(handler));
        const size_t index = this->propertyChangedHandlers.size() - 1;
        return [this, index]() {
            this->propertyChangedHandlers[index] = nullptr;
        };
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    bool StatisticsPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::StatisticsPagePreviewScenario scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid preview scenario JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Title) {
            this->title = std::move(*scenario.Title);
            this->NotifyPropertyChanged(Property::title);
        }
        if (scenario.Summary) {
            this->summary = std::move(*scenario.Summary);
            this->NotifyPropertyChanged(Property::summary);
        }
        return true;
    }
#endif

    //
    // Internal
    //
    void StatisticsPageViewModel::NotifyPropertyChanged(Property property) {
        for (const PropertyChangedHandler& handler : this->propertyChangedHandlers) {
            if (handler) {
                handler(property);
            }
        }
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // API
    //
    xaml::runtime::RuntimeBindingContext StatisticsPageViewModel::RuntimeContext() {
        auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
        xaml::runtime::RuntimeBindingPublisher publisher{*registry, *this};
        publisher.Text("Title", Property::title, &StatisticsPageViewModel::Title);
        publisher.Text("Summary", Property::summary, &StatisticsPageViewModel::Summary);
        publisher.Command("NavigateToMainCommand", &StatisticsPageViewModel::NavigateToMainCommand);
        xaml::runtime::RuntimeBindingContext result{registry, "StatisticsPageViewModel", {}};

        return result;
    }

    void StatisticsPageViewModel::ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
    }
#endif
}
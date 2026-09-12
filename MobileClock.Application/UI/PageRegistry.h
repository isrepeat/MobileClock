#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include "UI/AlarmSettings.h"
#include "UI/ApplicationStorage.h"
#include "UI/Navigation.h"

#include <string_view>
#include <functional>
#include <utility>
#include <memory>
#include <tuple>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class AppSessionController;
    class IPage {
    public:
        virtual ~IPage() = default;

        virtual std::string_view Name() const = 0;
        virtual std::unique_ptr<NavigationState> OnNavigatingFrom(const NavigationRequest& request) = 0;
        virtual bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<NavigationState> state) = 0;
        virtual void Initialize(xaml::Size availableSize) = 0;
        virtual void HandleTap(xaml::Element& element) = 0;
        virtual void Update() = 0;
        virtual xaml::Element& Root() = 0;
        virtual void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const = 0;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        virtual std::string_view PreviewGraphTitle() const = 0;
        virtual bool ApplyScenario(std::string_view json, std::string& error) = 0;
        virtual xaml::runtime::RuntimeBindingContext RuntimeContext() = 0;
        virtual void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) = 0;
#endif
    };

    struct PageContext final {
        IPageNavigator& navigator;
        AppSessionController& appSessionController;
        ApplicationStorage& storage;
        std::function<void(const AlarmSettings&)> saveAlarm;
    };

    template <typename TViewModel>
    class PageAdapter final : public IPage {
    public:
        explicit PageAdapter(PageContext& context)
            : viewModel(context) {
        }

        std::string_view Name() const override {
            return TViewModel::PageName;
        }

        std::unique_ptr<NavigationState> OnNavigatingFrom(const NavigationRequest& request) override {
            return this->viewModel.OnNavigatingFrom(request);
        }

        bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<NavigationState> state) override {
            return this->viewModel.OnNavigatingTo(request, std::move(state));
        }

        void Initialize(xaml::Size availableSize) override {
            this->viewModel.Initialize(availableSize);
        }

        void HandleTap(xaml::Element& element) override {
            this->viewModel.HandleTap(element);
        }

        void Update() override {
            this->viewModel.Update();
        }

        xaml::Element& Root() override {
            return this->viewModel.Root();
        }

        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const override {
            this->viewModel.Render(renderer, renderers);
        }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
        //
        // IPage
        //
        std::string_view PreviewGraphTitle() const override {
            return TViewModel::PreviewGraphTitle;
        }

        xaml::runtime::RuntimeBindingContext RuntimeContext() override {
            return this->viewModel.RuntimeContext();
        }

        void ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) override {
            this->viewModel.ReplaceRuntimeTree(std::move(result));
        }

        bool ApplyScenario(std::string_view json, std::string& error) override {
            return this->viewModel.Deserialize(json, error);
        }
#endif

        TViewModel& ViewModel() {
            return this->viewModel;
        }

    private:
        TViewModel viewModel;
    };

    template <typename... TViewModels>
    class PageRegistry final {
    public:
        explicit PageRegistry(PageContext& context)
            : pages(std::make_unique<PageAdapter<TViewModels>>(context)...) {
        }

        template <typename THandler>
        void ForEach(THandler&& handler) {
            std::apply([&handler](auto&... pages) {
                (handler(static_cast<IPage&>(*pages)), ...);
            }, this->pages);
        }

        template <typename THandler>
        void ForEach(THandler&& handler) const {
            std::apply([&handler](const auto&... pages) {
                (handler(static_cast<const IPage&>(*pages)), ...);
            }, this->pages);
        }

        IPage* Find(std::string_view pageName) {
            IPage* result = nullptr;
            this->ForEach([&result, pageName](IPage& page) {
                if (result == nullptr && page.Name() == pageName) {
                    result = &page;
                }
            });
            return result;
        }

        const IPage* Find(std::string_view pageName) const {
            const IPage* result = nullptr;
            this->ForEach([&result, pageName](const IPage& page) {
                if (result == nullptr && page.Name() == pageName) {
                    result = &page;
                }
            });
            return result;
        }

        bool IsAnyAnimating() {
            bool result = false;
            this->ForEach([&result](IPage& page) {
                result = result || xaml::AnimationController::IsAnimating(page.Root());
            });
            return result;
        }

        template <typename TViewModel>
        TViewModel& Get() {
            return std::get<std::unique_ptr<PageAdapter<TViewModel>>>(this->pages)->ViewModel();
        }

        template <typename TViewModel>
        const TViewModel& Get() const {
            return std::get<std::unique_ptr<PageAdapter<TViewModel>>>(this->pages)->ViewModel();
        }

        template <typename TViewModel>
        IPage& GetPage() {
            return *std::get<std::unique_ptr<PageAdapter<TViewModel>>>(this->pages);
        }

    private:
        std::tuple<std::unique_ptr<PageAdapter<TViewModels>>...> pages;
    };
}
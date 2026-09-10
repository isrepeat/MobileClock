#pragma once
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>

#include "UI/ApplicationActions.h"
#include "UI/Navigation.h"

#include <string_view>
#include <utility>

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <string>
#endif

#include <memory>
#include <tuple>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::ui {
    class IPage {
    public:
        virtual ~IPage() = default;

        virtual std::string_view Name() const = 0;
        virtual void Initialize(xaml::Size availableSize) = 0;
        virtual void HandleTap(xaml::Element& element) = 0;
        virtual void Update() = 0;
        virtual xaml::Element& Root() = 0;
        virtual void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const = 0;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        virtual bool ApplyScenario(std::string_view json, std::string& error) = 0;
#endif
    };

    struct PageContext final {
        IPageNavigator& navigator;
        IApplicationActions& actions;
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

        IPage* Find(std::string_view pageName) {
            IPage* result = nullptr;
            this->ForEach([&result, pageName](IPage& page) {
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
        IPage& GetPage() {
            return *std::get<std::unique_ptr<PageAdapter<TViewModel>>>(this->pages);
        }

    private:
        std::tuple<std::unique_ptr<PageAdapter<TViewModels>>...> pages;
    };
}
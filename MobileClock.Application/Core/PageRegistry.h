#pragma once
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/XamlLayout.h>

#include "../Interface/IPage.h"
#include "../Interface/IPageNavigator.h"
#include "../Model/AlarmRepository.h"
#include "Navigation.h"

#include <string_view>
#include <functional>
#include <utility>
#include <memory>
#include <tuple>

namespace xaml {
    class IRenderBackend;
    class RendererRegistry;
}

namespace mobileclock::application::core {
    class AppSessionController;
    struct PageContext final {
        interface::IPageNavigator& navigator;
        AppSessionController& appSessionController;
        model::AlarmRepository& alarmRepository;
        model::AlarmMelodyRepository& alarmMelodyRepository;
    };

    template <typename TViewModel>
    class PageAdapter final : public interface::IPage {
    public:
        explicit PageAdapter(PageContext& context)
            : viewModel(context) {
        }

        std::string_view Name() const override {
            return TViewModel::PageName;
        }

        std::unique_ptr<base::NavigationStateBase> OnNavigatingFrom(const NavigationRequest& request) override {
            return this->viewModel.OnNavigatingFrom(request);
        }

        bool OnNavigatingTo(const NavigationRequest& request, std::unique_ptr<base::NavigationStateBase> state) override {
            return this->viewModel.OnNavigatingTo(request, std::move(state));
        }

        void Initialize(xaml::Size availableSize) override {
            if constexpr (requires(TViewModel& viewModel, xaml::Size size) { viewModel.preview_Initialize(size); }) {
                this->viewModel.preview_Initialize(availableSize);
            } else {
                this->viewModel.Initialize(availableSize);
            }
        }

        void HandleTap(xaml::Element& element) override {
            if constexpr (requires(TViewModel& viewModel, xaml::Element& value) { viewModel.preview_HandleTap(value); }) {
                this->viewModel.preview_HandleTap(element);
            } else {
                this->viewModel.HandleTap(element);
            }
        }

        void Update() override {
            if constexpr (requires(TViewModel& viewModel) { viewModel.preview_Update(); }) {
                this->viewModel.preview_Update();
            } else {
                this->viewModel.Update();
            }
        }

        xaml::Element& Root() override {
            if constexpr (requires(TViewModel& viewModel) { viewModel.preview_Root(); }) {
                return this->viewModel.preview_Root();
            } else {
                return this->viewModel.Root();
            }
        }

        void Render(xaml::IRenderBackend& renderer, const xaml::RendererRegistry& renderers) const override {
            if constexpr (requires(const TViewModel& viewModel, xaml::IRenderBackend& backend, const xaml::RendererRegistry& registry) {
                viewModel.preview_Render(backend, registry);
            }) {
                this->viewModel.preview_Render(renderer, renderers);
            } else {
                this->viewModel.Render(renderer, renderers);
            }
        }

#if defined(ANDROID_APP_PREVIEWER)
        //
        // IPage
        //
        std::string_view preview_GraphTitle() const override {
            return TViewModel::preview_GraphTitle;
        }

        xaml::runtime::RuntimeBindingContext preview_RuntimeContext() override {
            return this->viewModel.preview_RuntimeContext();
        }

        void preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) override {
            this->viewModel.preview_ReplaceRuntimeTree(std::move(result));
        }

        bool preview_ApplyScenario(std::string_view json, std::string& error) override {
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
                (handler(static_cast<interface::IPage&>(*pages)), ...);
            }, this->pages);
        }

        template <typename THandler>
        void ForEach(THandler&& handler) const {
            std::apply([&handler](const auto&... pages) {
                (handler(static_cast<const interface::IPage&>(*pages)), ...);
            }, this->pages);
        }

        interface::IPage* Find(std::string_view pageName) {
            interface::IPage* result = nullptr;
            this->ForEach([&result, pageName](interface::IPage& page) {
                if (result == nullptr && page.Name() == pageName) {
                    result = &page;
                }
            });
            return result;
        }

        const interface::IPage* Find(std::string_view pageName) const {
            const interface::IPage* result = nullptr;
            this->ForEach([&result, pageName](const interface::IPage& page) {
                if (result == nullptr && page.Name() == pageName) {
                    result = &page;
                }
            });
            return result;
        }

        bool IsAnyAnimating() {
            bool result = false;
            this->ForEach([&result](interface::IPage& page) {
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
        interface::IPage& GetPage() {
            return *std::get<std::unique_ptr<PageAdapter<TViewModel>>>(this->pages);
        }

    private:
        std::tuple<std::unique_ptr<PageAdapter<TViewModels>>...> pages;
    };
}
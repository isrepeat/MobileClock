#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "UI/Pages/XiaomiThemesPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <JsonParser/JsonParser.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/XiaomiThemesPage.xaml.h"
#include "UI/Pages/AddAlarmPageViewModel.h"
#include "UI/NavigationStates.h"

#include <format>
#include <utility>

namespace mobileclock::ui::_details {
    xaml::Element* FindXiaomiThemesElement(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (auto* found = FindXiaomiThemesElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    struct XiaomiThemesPreviewMelody final {
        std::string Name;
        std::string Uri;

        JS_OBJECT(JS_MEMBER(Name), JS_MEMBER(Uri));
    };

    struct XiaomiThemesPreviewScenario final {
        std::optional<std::vector<XiaomiThemesPreviewMelody>> Melodies;

        JS_OBJECT(JS_MEMBER(Melodies));
    };
#endif
}

namespace mobileclock::ui {
    XiaomiThemesPageViewModel::Melody::Melody(std::string name, std::string uri)
        : name(std::move(name))
        , uri(std::move(uri)) {
    }

    //
    // API
    //
    const std::string& XiaomiThemesPageViewModel::Melody::Name() const {
        return this->name;
    }

    const std::string& XiaomiThemesPageViewModel::Melody::Uri() const {
        return this->uri;
    }

    XiaomiThemesPageViewModel::XiaomiThemesPageViewModel(PageContext& context)
        : context(context) {
        this->melodies.emplace_back("Morning", "preview://xiaomi-themes/morning");
        this->melodies.emplace_back("Lone Grass, Solitary Flower", "preview://xiaomi-themes/lone-grass");
        this->melodies.emplace_back("Positive Uplift", "preview://xiaomi-themes/positive-uplift");
        this->selectedMelody = 0;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // ISerializable
    //
    bool XiaomiThemesPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::XiaomiThemesPreviewScenario scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid preview scenario JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Melodies) {
            this->melodies.clear();
            for (const _details::XiaomiThemesPreviewMelody& melody : *scenario.Melodies) {
                this->melodies.emplace_back(melody.Name, melody.Uri);
            }
            this->selectedMelody = this->melodies.empty() ? std::nullopt : std::optional<size_t>{0};
            this->RebuildMelodies();
            this->Refresh();
        }
        return true;
    }
#endif

    //
    // INavigationPage
    //
    std::unique_ptr<NavigationState> XiaomiThemesPageViewModel::OnNavigatingFrom(const NavigationRequest& request) {
        if (request.trigger != NavigationTrigger::applySelectedMelody || !this->selectedMelody) {
            return {};
        }
        const Melody& melody = this->melodies[*this->selectedMelody];
        return std::make_unique<AlarmMelodyNavigationState>(melody.Name(), melody.Uri());
    }

    bool XiaomiThemesPageViewModel::OnNavigatingTo(const NavigationRequest&, std::unique_ptr<NavigationState>) {
        return true;
    }

    //
    // API
    //
    void XiaomiThemesPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        this->runtimeBindings.reset();
#endif
        this->page = xaml::generated::XiaomiThemesPage::Create(*this, this->bindings);
        this->ConnectControls();
        xaml::layout(*this->page, availableSize);
    }

    void XiaomiThemesPageViewModel::HandleTap(xaml::Element& element) {
        element.ExecuteCommand();
    }

    void XiaomiThemesPageViewModel::Update() {
    }

    void XiaomiThemesPageViewModel::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        xaml::Render(*this->page, renderer, renderers);
    }

    xaml::Element& XiaomiThemesPageViewModel::Root() {
        return *this->page;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    xaml::runtime::RuntimeBindingContext XiaomiThemesPageViewModel::RuntimeContext() {
        return {std::make_shared<xaml::runtime::RuntimeBindingRegistry>(), "XiaomiThemesPageViewModel", {}};
    }

    void XiaomiThemesPageViewModel::ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
        this->ConnectControls();
    }
#endif

    //
    // Internal
    //
    void XiaomiThemesPageViewModel::ConnectControls() {
        if (auto* apply = this->Find("applyButton")) {
            apply->SetCommand([this]() {
                this->context.navigator.Trigger(NavigationTrigger::applySelectedMelody);
            });
        }
        this->RebuildMelodies();
        this->Refresh();
    }

    void XiaomiThemesPageViewModel::RebuildMelodies() {
        for (size_t index = 0; index < 3; ++index) {
            auto* const choice = this->Find(std::format("melody{}", index));
            if (choice == nullptr) {
                continue;
            }
            if (index >= this->melodies.size()) {
                choice->SetVisibility(xaml::attr::Visibility::collapsed);
                choice->SetCommand({});
                continue;
            }
            choice->SetText(this->melodies[index].Name());
            choice->SetVisibility(xaml::attr::Visibility::visible);
            choice->SetCommand([this, index]() {
                this->selectedMelody = index;
                this->Refresh();
            });
        }
    }

    void XiaomiThemesPageViewModel::Refresh() {
        if (!this->page) {
            return;
        }
        for (size_t index = 0; index < this->melodies.size() && index < 3; ++index) {
            if (auto* choice = this->Find(std::format("melody{}", index))) {
                choice->SetBackground(index == this->selectedMelody.value_or(this->melodies.size())
                    ? xaml::attr::Color{224.0f / 255, 182.0f / 255, 77.0f / 255, 1.0f}
                    : xaml::attr::Color{45.0f / 255, 48.0f / 255, 34.0f / 255, 1.0f});
                choice->SetForeground(index == this->selectedMelody.value_or(this->melodies.size())
                    ? xaml::attr::Color{23.0f / 255, 24.0f / 255, 19.0f / 255, 1.0f}
                    : xaml::attr::Color{234.0f / 255, 232.0f / 255, 220.0f / 255, 1.0f});
            }
        }
        if (auto* apply = this->Find("applyButton")) {
            apply->SetOpacity(this->selectedMelody ? 1.0f : 0.45f);
        }
    }

    xaml::Element* XiaomiThemesPageViewModel::Find(std::string_view id) const {
        return this->page ? _details::FindXiaomiThemesElement(*this->page, id) : nullptr;
    }
}
#endif
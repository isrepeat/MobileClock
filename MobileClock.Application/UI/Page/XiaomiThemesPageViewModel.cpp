#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include "XiaomiThemesPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>
#include <JsonParser/json_struct/json_struct.h>

#include "!Generated/MobileClock.Application/Xaml/Page/XiaomiThemesPage.xaml.h"
#include "../../Core/NavigationStates.h"
#include "AddAlarmPageViewModel.h"

#include <utility>
#include <format>

namespace mobileclock::application::ui::page::_details {
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

    struct XiaomiThemesPreviewMelody final {
        std::string Name;
        std::string Uri;

        JS_OBJECT(
            JS_MEMBER(Name),
            JS_MEMBER(Uri)
        );
    };

    struct XiaomiThemesSerializationDocument final {
        std::optional<std::vector<XiaomiThemesPreviewMelody>> Melodies;

        JS_OBJECT(
            JS_MEMBER(Melodies)
        );
    };
} // namespace _details

namespace mobileclock::application::ui::page {
    XiaomiThemesPageViewModel::XiaomiThemesPageViewModel(core::PageContext& context)
        : context(context) {
        this->melodies.emplace_back(model::AlarmMelody{"morning", "Morning", "preview://xiaomi-themes/morning"});
        this->melodies.emplace_back(model::AlarmMelody{"lone-grass", "Lone Grass, Solitary Flower", "preview://xiaomi-themes/lone-grass"});
        this->melodies.emplace_back(model::AlarmMelody{"positive-uplift", "Positive Uplift", "preview://xiaomi-themes/positive-uplift"});
        this->selectedMelody = 0;
    }

    //
    // ISerializable
    //
    std::string XiaomiThemesPageViewModel::Serialize() const {
        _details::XiaomiThemesSerializationDocument scenario;
        scenario.Melodies.emplace();
        for (const view_model::AlarmMelodyViewModel& melody : this->melodies) {
            scenario.Melodies->push_back({melody.Name(), melody.Uri()});
        }
        return JS::serializeStruct(scenario);
    }

    bool XiaomiThemesPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::XiaomiThemesSerializationDocument scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid serialized JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Melodies) {
            this->melodies.clear();
            for (const _details::XiaomiThemesPreviewMelody& melody : *scenario.Melodies) {
                this->melodies.emplace_back(model::AlarmMelody{"", melody.Name, melody.Uri});
            }
            this->selectedMelody = this->melodies.empty() ? std::nullopt : std::optional<size_t>{0};
            this->RebuildMelodies();
            this->Refresh();
        }
        return true;
    }

    //
    // INavigationPage
    //
    std::unique_ptr<base::NavigationStateBase> XiaomiThemesPageViewModel::OnNavigatingFrom(const core::NavigationRequest&) {
        return {};
    }

    bool XiaomiThemesPageViewModel::OnNavigatingTo(const core::NavigationRequest&, std::unique_ptr<base::NavigationStateBase>) {
        return true;
    }

    //
    // API
    //
    void XiaomiThemesPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
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

    xaml::runtime::RuntimeBindingContext XiaomiThemesPageViewModel::RuntimeContext() {
        xaml::runtime::RuntimeBindingContext result{
            std::make_shared<xaml::runtime::RuntimeBindingRegistry>(),
            "XiaomiThemesPageViewModel",
            {}};
        result.xamlNamespace = "urn:mobileclock:xaml";
        result.controlXmlNamespace = "using:mobileclock.ui.control";
        return result;
    }

    void XiaomiThemesPageViewModel::ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
        this->ConnectControls();
    }

    //
    // Internal
    //
    void XiaomiThemesPageViewModel::ConnectControls() {
        if (auto* back = this->Find("backNavigation")) {
            back->SetCommand([this]() {
                this->context.navigator.Trigger(core::NavigationTrigger::navigateBack);
            });
        }
        if (auto* apply = this->Find("applyButton")) {
            apply->SetCommand([this]() {
                this->ApplySelectedMelody();
            });
        }
        this->RebuildMelodies();
        this->Refresh();
    }

    void XiaomiThemesPageViewModel::ApplySelectedMelody() {
        if (!this->selectedMelody) {
            return;
        }
        // Применение и обычный возврат используют один маршрут истории. Payload нужен только
        // этому действию: стрелка «Назад» возвращается без изменения черновика будильника.
        const view_model::AlarmMelodyViewModel& melody = this->melodies[*this->selectedMelody];
        this->context.navigator.NavigateBack(std::make_unique<core::AlarmMelodyNavigationState>(melody.Value()));
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
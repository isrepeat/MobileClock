#if defined(ANDROID_APP_PREVIEWER)
#include "XiaomiThemesPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>
#include <JsonParser/json_struct/json_struct.h>

#include "!Generated/MobileClock.Application/Xaml/Page/XiaomiThemesPage.xaml.h"
#include "../../Core/NavigationStates.h"
#include "AddAlarmPageViewModel.h"

#include <utility>
#include <format>

namespace mobileclock::application::ui::page::_details {
    xaml::Element* preview_FindXiaomiThemesElement(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (auto* found = preview_FindXiaomiThemesElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    struct preview_XiaomiThemesMelody final {
        std::string Name;
        std::string Uri;

        JS_OBJECT(
            JS_MEMBER(Name),
            JS_MEMBER(Uri)
        );
    };

    struct preview_XiaomiThemesSerializationDocument final {
        std::optional<std::vector<preview_XiaomiThemesMelody>> Melodies;

        JS_OBJECT(
            JS_MEMBER(Melodies)
        );
    };
} // namespace _details

namespace mobileclock::application::ui::page {
    preview_XiaomiThemesPageViewModel::preview_XiaomiThemesPageViewModel(core::PageContext& context)
        : context(context) {
        this->melodies.emplace_back(model::AlarmMelody{"morning", "Morning", "preview://xiaomi-themes/morning"});
        this->melodies.emplace_back(model::AlarmMelody{"lone-grass", "Lone Grass, Solitary Flower", "preview://xiaomi-themes/lone-grass"});
        this->melodies.emplace_back(model::AlarmMelody{"positive-uplift", "Positive Uplift", "preview://xiaomi-themes/positive-uplift"});
        this->selectedMelody = 0;
    }

    //
    // ISerializable
    //
    std::string preview_XiaomiThemesPageViewModel::Serialize() const {
        _details::preview_XiaomiThemesSerializationDocument scenario;
        scenario.Melodies.emplace();
        for (const view_model::AlarmMelodyViewModel& melody : this->melodies) {
            scenario.Melodies->push_back({melody.Name(), melody.Uri()});
        }
        return JS::serializeStruct(scenario);
    }

    bool preview_XiaomiThemesPageViewModel::Deserialize(std::string_view json, std::string& error) {
        _details::preview_XiaomiThemesSerializationDocument scenario;
        JS::ParseContext context(json.data(), json.size());
        if (context.parseTo(scenario) != JS::Error::NoError) {
            error = std::format("Invalid serialized JSON: {}", context.makeErrorString());
            return false;
        }
        if (scenario.Melodies) {
            this->melodies.clear();
            for (const _details::preview_XiaomiThemesMelody& melody : *scenario.Melodies) {
                this->melodies.emplace_back(model::AlarmMelody{"", melody.Name, melody.Uri});
            }
            this->selectedMelody = this->melodies.empty() ? std::nullopt : std::optional<size_t>{0};
            this->preview_RebuildMelodies();
            this->preview_Refresh();
        }
        return true;
    }

    //
    // INavigationPage
    //
    std::unique_ptr<base::NavigationStateBase> preview_XiaomiThemesPageViewModel::OnNavigatingFrom(const core::NavigationRequest&) {
        return {};
    }

    bool preview_XiaomiThemesPageViewModel::OnNavigatingTo(const core::NavigationRequest&, std::unique_ptr<base::NavigationStateBase>) {
        return true;
    }

    //
    // API
    //
    void preview_XiaomiThemesPageViewModel::preview_Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = xaml::generated::XiaomiThemesPage::Create(*this, this->bindings);
        this->preview_ConnectControls();
        xaml::layout(*this->page, availableSize);
    }

    void preview_XiaomiThemesPageViewModel::preview_HandleTap(xaml::Element& element) {
        element.ExecuteCommand();
    }

    void preview_XiaomiThemesPageViewModel::preview_Update() {
    }

    void preview_XiaomiThemesPageViewModel::preview_Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        xaml::Render(*this->page, renderer, renderers);
    }

    xaml::Element& preview_XiaomiThemesPageViewModel::preview_Root() {
        return *this->page;
    }

    xaml::runtime::RuntimeBindingContext preview_XiaomiThemesPageViewModel::preview_RuntimeContext() {
        xaml::runtime::RuntimeBindingContext result{
            std::make_shared<xaml::runtime::RuntimeBindingRegistry>(),
            "preview_XiaomiThemesPageViewModel",
            {}};
        result.xamlNamespace = "urn:mobileclock:xaml";
        result.controlXmlNamespace = "using:mobileclock.ui.control";
        return result;
    }

    void preview_XiaomiThemesPageViewModel::preview_ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
        this->preview_ConnectControls();
    }

    //
    // Internal
    //
    void preview_XiaomiThemesPageViewModel::preview_ConnectControls() {
        if (auto* back = this->preview_Find("backNavigation")) {
            back->SetCommand([this]() {
                this->context.navigator.Trigger(core::NavigationTrigger::navigateBack);
            });
        }
        if (auto* apply = this->preview_Find("applyButton")) {
            apply->SetCommand([this]() {
                this->preview_ApplySelectedMelody();
            });
        }
        this->preview_RebuildMelodies();
        this->preview_Refresh();
    }

    void preview_XiaomiThemesPageViewModel::preview_ApplySelectedMelody() {
        if (!this->selectedMelody) {
            return;
        }
        // Применение и обычный возврат используют один маршрут истории. Payload нужен только
        // этому действию: стрелка «Назад» возвращается без изменения черновика будильника.
        const view_model::AlarmMelodyViewModel& melody = this->melodies[*this->selectedMelody];
        this->context.navigator.NavigateBack(std::make_unique<core::preview_AlarmMelodyNavigationState>(melody.Value()));
    }

    void preview_XiaomiThemesPageViewModel::preview_RebuildMelodies() {
        for (size_t index = 0; index < 3; ++index) {
            auto* const choice = this->preview_Find(std::format("melody{}", index));
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
                this->preview_Refresh();
            });
        }
    }

    void preview_XiaomiThemesPageViewModel::preview_Refresh() {
        if (!this->page) {
            return;
        }
        for (size_t index = 0; index < this->melodies.size() && index < 3; ++index) {
            if (auto* choice = this->preview_Find(std::format("melody{}", index))) {
                choice->SetBackground(index == this->selectedMelody.value_or(this->melodies.size())
                    ? xaml::attr::Color{224.0f / 255, 182.0f / 255, 77.0f / 255, 1.0f}
                    : xaml::attr::Color{45.0f / 255, 48.0f / 255, 34.0f / 255, 1.0f});
                choice->SetForeground(index == this->selectedMelody.value_or(this->melodies.size())
                    ? xaml::attr::Color{23.0f / 255, 24.0f / 255, 19.0f / 255, 1.0f}
                    : xaml::attr::Color{234.0f / 255, 232.0f / 255, 220.0f / 255, 1.0f});
            }
        }
        if (auto* apply = this->preview_Find("applyButton")) {
            apply->SetOpacity(this->selectedMelody ? 1.0f : 0.45f);
        }
    }

    xaml::Element* preview_XiaomiThemesPageViewModel::preview_Find(std::string_view id) const {
        return this->page ? _details::preview_FindXiaomiThemesElement(*this->page, id) : nullptr;
    }
}
#endif
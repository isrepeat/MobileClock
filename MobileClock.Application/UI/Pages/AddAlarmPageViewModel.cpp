#include "UI/Pages/AddAlarmPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/AddAlarmPage.xaml.h"
#include "UI/AppSessionController.h"
#include "UI/Pages/MainPageViewModel.h"
#include "UI/NavigationStates.h"

#include <algorithm>
#include <format>
#include <cmath>

namespace mobileclock::ui::_details {
    xaml::Element* FindAlarmElement(xaml::Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (auto* found = FindAlarmElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    int WrapAlarmValue(int value, int count) {
        return (value % count + count) % count;
    }
}

namespace mobileclock::ui {
    AddAlarmPageViewModel::Melody::Melody(std::string name, std::string uri, xaml::Element::Command selectCommand)
        : name(std::move(name))
        , uri(std::move(uri))
        , selectCommand(std::move(selectCommand)) {
    }

    const std::string& AddAlarmPageViewModel::Melody::Name() const {
        return this->name;
    }

    const std::string& AddAlarmPageViewModel::Melody::Uri() const {
        return this->uri;
    }

    xaml::Element::Command AddAlarmPageViewModel::Melody::SelectCommand() const {
        return this->selectCommand;
    }

    void AddAlarmPageViewModel::Melody::SetName(std::string value) {
        this->name = std::move(value);
    }

    AddAlarmPageViewModel::AddAlarmPageViewModel(PageContext& context)
        : context(context) {
        for (const AlarmMelody& melody : this->context.storage->alarmMelodies) {
            this->AddMelody(melody.name, melody.uri);
        }
        this->storageSubscription = this->context.storage.Subscribe([this](const StorageChange& change) {
            this->OnStorageChange(change);
        });
        this->RegisterGestureTarget();
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // ISerializable
    //
    bool AddAlarmPageViewModel::Deserialize(std::string_view json, std::string& error) {
        // The form has its own editable draft; no external preview scenario is required.
        error.clear();
        return true;
    }
#endif

    //
    // IGestureTarget
    //
    bool AddAlarmPageViewModel::CanHandlePan(const xaml::Element& element) const {
        return this->WheelColumn(element) >= 0;
    }

    bool AddAlarmPageViewModel::IsVerticalPan() const {
        return true;
    }

    xaml::Element* AddAlarmPageViewModel::FindScrollViewer(const xaml::Element& element) const {
        return nullptr;
    }

    void AddAlarmPageViewModel::BeginPan(const PanState& state) {
        this->activeColumn = this->WheelColumn(state.target);
        this->startValue = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
        const auto* wheel = this->Find(std::format("wheel{}", this->activeColumn));
        this->rowHeight = std::max(1.0f, wheel->Bounds().height / 5.0f);
        this->remainder = 0.0f;
        this->dragging = true;
    }

    void AddAlarmPageViewModel::UpdatePan(const PanState& state) {
        const float distance = state.downY - state.currentY;
        const int steps = static_cast<int>(std::round(distance / this->rowHeight));
        int& value = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
        value = _details::WrapAlarmValue(this->startValue + steps, this->activeColumn == 0 ? 24 : 60);
        this->remainder = -distance + steps * this->rowHeight;
        this->RefreshWheel(this->activeColumn, this->remainder);
    }

    bool AddAlarmPageViewModel::EndPan(const PanState& state, xaml::AnimationController& animations) {
        this->UpdatePan(state);
        this->dragging = false;
        this->remainder = 0.0f;
        this->RefreshWheel(this->activeColumn, 0.0f);
        return true;
    }

    void AddAlarmPageViewModel::CancelPan(xaml::Element& element) {
        if (this->dragging) {
            int& value = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
            value = this->startValue;
        }
        this->dragging = false;
        this->remainder = 0.0f;
        this->RefreshWheel(this->activeColumn, 0.0f);
    }

    void AddAlarmPageViewModel::UpdateGestures(xaml::Element& pageRoot, xaml::AnimationController& animations) {
    }

    //
    // INavigationPage
    //
    std::unique_ptr<NavigationState> AddAlarmPageViewModel::OnNavigatingFrom(const NavigationRequest&) {
        return {};
    }

    bool AddAlarmPageViewModel::OnNavigatingTo(
        const NavigationRequest& request,
        std::unique_ptr<NavigationState> state) {
        switch (request.trigger) {
        case NavigationTrigger::createAlarm:
            this->Reset();
            return true;
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        case NavigationTrigger::applySelectedMelody: {
            const auto* const melody = dynamic_cast<const AlarmMelodyNavigationState*>(state.get());
            if (melody == nullptr) {
                return false;
            }
            this->SetMelody(melody->Name(), melody->Uri());
            return true;
        }
#endif
        default:
            return true;
        }
    }

    //
    // API
    //
    void AddAlarmPageViewModel::Reset() {
        this->settings = AlarmSettings{};
        this->dragging = false;
        this->remainder = 0.0f;
        this->saved = false;
        this->Refresh();
    }

    const AlarmSettings& AddAlarmPageViewModel::Settings() const {
        return this->settings;
    }

    void AddAlarmPageViewModel::AddMelody(std::string name, std::string uri) {
        const auto existing = std::find_if(this->melodies.begin(), this->melodies.end(), [&uri](const Melody& melody) {
            return melody.Uri() == uri;
        });
        if (existing == this->melodies.end()) {
            const xaml::Element::Command selectCommand = [this, name, uri]() {
                this->SetMelody(name, uri);
            };
            this->melodies.EmplaceBack(std::move(name), std::move(uri), selectCommand);
        } else {
            existing->SetName(std::move(name));
        }
    }

    void AddAlarmPageViewModel::SetMelody(std::string name, std::string uri) {
        auto edit = this->context.storage.Edit();
        const auto existing = std::find_if(edit->alarmMelodies.begin(), edit->alarmMelodies.end(), [&uri](const AlarmMelody& melody) {
            return melody.uri == uri;
        });
        if (existing == edit->alarmMelodies.end()) {
            edit->alarmMelodies.push_back({name, uri});
        } else {
            existing->name = name;
        }
        if (!edit.Commit()) {
            return;
        }
        this->settings.melody = std::move(name);
        this->settings.melodyUri = std::move(uri);
        this->Refresh();
    }

    const xaml::ObservableCollection<AddAlarmPageViewModel::Melody>& AddAlarmPageViewModel::Melodies() const {
        return this->melodies;
    }

    void AddAlarmPageViewModel::ChooseAlarmMelody() {
        this->context.appSessionController.Dispatch(AppSessionSignal::requestAlarmMelody, {});
    }

    void AddAlarmPageViewModel::NavigateToMain() {
        this->context.navigator.Trigger(NavigationTrigger::navigateToMain);
    }

    void AddAlarmPageViewModel::Initialize(xaml::Size availableSize) {
        this->bindings.Clear();
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        this->runtimeBindings.reset();
#endif
        this->page = xaml::generated::AddAlarmPage::Create(*this, this->bindings);
        this->ConnectControls();
        xaml::layout(*this->page, availableSize);
    }

    void AddAlarmPageViewModel::HandleTap(xaml::Element& element) {
        element.ExecuteCommand();
    }

    void AddAlarmPageViewModel::Update() {
    }

    void AddAlarmPageViewModel::Render(
        xaml::IRenderBackend& renderer,
        const xaml::RendererRegistry& renderers) const {
        xaml::Render(*this->page, renderer, renderers);
    }

    xaml::Element& AddAlarmPageViewModel::Root() {
        return *this->page;
    }

#if defined(MOBILECLOCK_XAML_PREVIEWER)
    xaml::runtime::RuntimeBindingContext AddAlarmPageViewModel::RuntimeContext() {
        auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
        xaml::runtime::RuntimeBindingContext result{registry, "AddAlarmPageViewModel", {}};
        xaml::runtime::RuntimeCollectionDescriptor collection;
        collection.itemBindings = [](const void* value) {
            const auto* melody = static_cast<const Melody*>(value);
            auto item = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
            item->AddText("Name", [melody]() { return melody == nullptr ? "" : melody->Name(); });
            item->AddCommand("SelectCommand", melody == nullptr ? xaml::Element::Command{} : melody->SelectCommand());
            return item;
        };
        collection.bind = [this](xaml::Element& element, xaml::Element::ItemTemplate itemTemplate) {
            element.SetItemsSource(this->melodies, std::move(itemTemplate));
        };
        registry->AddCollection("Melodies", collection);
        return result;
    }

    void AddAlarmPageViewModel::ReplaceRuntimeTree(xaml::runtime::RuntimeBuildResult result) {
        this->bindings.Clear();
        this->runtimeBindings.reset();
        this->page = std::move(result.root);
        this->runtimeBindings = std::move(result.bindings);
        this->dragging = false;
        this->remainder = 0.0f;
        this->ConnectControls();
    }
#endif

    //
    // IGestureTarget
    //
    bool AddAlarmPageViewModel::IsIn(const xaml::Element& pageRoot) const {
        return this->page.get() == &pageRoot;
    }

    bool AddAlarmPageViewModel::Owns(const xaml::Element& element) const {
        const xaml::Element* root = &element;
        while (root->Parent() != nullptr) {
            root = root->Parent();
        }
        return root == this->page.get();
    }

    //
    // Internal
    //
    void AddAlarmPageViewModel::ConnectControls() {
        const auto connect = [this](std::string_view id, xaml::Element::Command command) {
            if (auto* element = this->Find(id)) {
                element->SetCommand(std::move(command));
            }
        };
        connect("backNavigation", [this]() {
            this->NavigateToMain();
        });
        connect("saveAlarmButton", [this]() {
            if (this->saved || !this->context.saveAlarm) {
                return;
            }
            this->context.saveAlarm(this->settings);
            this->saved = true;
            this->context.navigator.Trigger(NavigationTrigger::navigateToMain);
        });
        for (int column = 0; column < 2; ++column) {
            connect(std::format("wheel{}", column), []() {});
            for (int row = 0; row < 5; ++row) {
                connect(std::format("wheel{}Row{}", column, row), [this, column, row]() {
                    this->ChangeTime(column, row - 2);
                });
            }
        }
        for (size_t day = 0; day < this->settings.days.size(); ++day) {
            connect(std::format("day{}", day), [this, day]() {
                this->settings.days[day] = !this->settings.days[day];
                this->Refresh();
            });
        }
        connect("melodyButton", [this]() {
            this->ChooseAlarmMelody();
        });
        this->Refresh();
    }

    void AddAlarmPageViewModel::OnStorageChange(const StorageChange& change) {
        if (change.path.segments.empty() || change.path.segments.front() != "alarmMelodies") {
            return;
        }
        switch (change.action) {
        case StorageAction::add:
        case StorageAction::update:
            if (change.value) {
                this->AddMelody(change.value->name, change.value->uri);
            }
            return;
        case StorageAction::remove:
            if (change.previousValue) {
                this->RemoveMelody(change.previousValue->uri);
            }
            return;
        }
    }

    void AddAlarmPageViewModel::RemoveMelody(std::string_view uri) {
        const auto melody = std::find_if(this->melodies.begin(), this->melodies.end(), [uri](const Melody& value) {
            return value.Uri() == uri;
        });
        if (melody == this->melodies.end()) {
            return;
        }
        if (this->settings.melodyUri == uri) {
            this->settings.melody = AlarmSettings{}.melody;
            this->settings.melodyUri.clear();
        }
        this->melodies.Erase(melody);
        this->Refresh();
    }

    void AddAlarmPageViewModel::Refresh() {
        if (!this->page) {
            return;
        }
        this->RefreshWheel(0, 0.0f);
        this->RefreshWheel(1, 0.0f);
        const xaml::attr::Color accent{224.0f / 255, 182.0f / 255, 77.0f / 255, 1};
        const xaml::attr::Color dark{35.0f / 255, 37.0f / 255, 28.0f / 255, 1};
        const xaml::attr::Color muted{147.0f / 255, 147.0f / 255, 126.0f / 255, 1};
        for (size_t day = 0; day < this->settings.days.size(); ++day) {
            if (auto* button = this->Find(std::format("day{}", day))) {
                button->SetBackground(this->settings.days[day] ? accent : dark);
                button->SetForeground(this->settings.days[day] ? dark : muted);
            }
        }
        if (auto* summary = this->Find("repeatSummary")) {
            const auto count = std::count(this->settings.days.begin(), this->settings.days.end(), true);
            const std::array<bool, 7> weekdays{true, true, true, true, true, false, false};
            const std::array<bool, 7> weekend{false, false, false, false, false, true, true};
            summary->SetText(count == 0 ? "Однократно" : count == 7 ? "Каждый день"
                : this->settings.days == weekdays ? "По будням"
                : this->settings.days == weekend ? "По выходным" : "Выбранные дни");
        }
    }

    void AddAlarmPageViewModel::RefreshWheel(int column, float offset) {
        const int value = column == 0 ? this->settings.hour : this->settings.minute;
        for (int row = 0; row < 5; ++row) {
            if (auto* text = this->Find(std::format("wheel{}Row{}", column, row))) {
                const float distance = std::abs(row - 2 + offset / this->rowHeight);
                text->SetText(std::format("{:02}", _details::WrapAlarmValue(value + row - 2, column == 0 ? 24 : 60)));
                text->SetRenderOffsetY(offset);
                text->SetFontSize(48.0f + 52.0f * std::max(0.0f, 1.0f - distance));
                text->SetFontWeight(row == 2 ? "Bold" : "Normal");
                text->SetOpacity(std::clamp(1.0f - distance * 0.35f, 0.0f, 1.0f));
            }
        }
    }

    void AddAlarmPageViewModel::ChangeTime(int column, int steps) {
        int& value = column == 0 ? this->settings.hour : this->settings.minute;
        value = _details::WrapAlarmValue(value + steps, column == 0 ? 24 : 60);
        this->RefreshWheel(column, 0.0f);
    }

    int AddAlarmPageViewModel::WheelColumn(const xaml::Element& element) const {
        for (const xaml::Element* current = &element; current != nullptr; current = current->Parent()) {
            if (current->Id() == "wheel0") {
                return 0;
            }
            if (current->Id() == "wheel1") {
                return 1;
            }
        }
        return -1;
    }

    xaml::Element* AddAlarmPageViewModel::Find(std::string_view id) const {
        return this->page ? _details::FindAlarmElement(*this->page, id) : nullptr;
    }
}
#include "UI/Pages/AddAlarmPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#endif

#include "!Generated/MobileClock.Application/Xaml/Pages/AddAlarmPage.xaml.h"
#include "MobileClock.UI/Controls/AlarmMelodyList.h"
#include "UI/Pages/MainPageViewModel.h"
#include "UI/AppSessionController.h"
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

    bool AreEqual(const AlarmSettings& left, const AlarmSettings& right) {
        return left.hour == right.hour
            && left.minute == right.minute
            && left.days == right.days
            && left.melody == right.melody
            && left.melodyUri == right.melodyUri
            && left.vibration == right.vibration;
    }

    void RefreshMelodySelection(
        xaml::Element& element,
        const void* inheritedDataContext,
        std::string_view selectedUri) {
        const void* const dataContext = element.DataContext() == nullptr
            ? inheritedDataContext
            : element.DataContext();
        if (element.Id() == "melodyListItem") {
            const auto* const melody = static_cast<const AddAlarmPageViewModel::Melody*>(dataContext);
            const bool isSelected = melody != nullptr && melody->Uri() == selectedUri;
            element.SetBackground(isSelected
                ? xaml::attr::Color{42.0f / 255, 47.0f / 255, 33.0f / 255, 1}
                : xaml::attr::Color{26.0f / 255, 29.0f / 255, 22.0f / 255, 1});
            element.SetBorderColor(isSelected
                ? xaml::attr::Color{224.0f / 255, 182.0f / 255, 77.0f / 255, 1}
                : xaml::attr::Color{0, 0, 0, 0});
            element.SetBorderThickness(isSelected
                ? xaml::attr::Thickness{2, 2, 2, 2}
                : xaml::attr::Thickness{});
        }
        for (const auto& child : element.Children()) {
            RefreshMelodySelection(*child, dataContext, selectedUri);
        }
    }
}

namespace mobileclock::ui {
    AddAlarmPageViewModel::Melody::Melody(
        std::string name,
        std::string uri,
        xaml::Element::Command selectCommand,
        xaml::Element::Command deleteCommand)
        : name(std::move(name))
        , uri(std::move(uri))
        , selectCommand(std::move(selectCommand))
        , deleteCommand(std::move(deleteCommand)) {
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

    xaml::Element::Command AddAlarmPageViewModel::Melody::DeleteCommand() const {
        return this->deleteCommand;
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
    xaml::Element* AddAlarmPageViewModel::FindScrollViewer(const xaml::Element& element) const {
        return nullptr;
    }

    GestureHandling AddAlarmPageViewModel::ResolveGesture(
        const PanState& state,
        GestureDirection direction) const {
        return this->WheelColumn(state.target) >= 0
            && (direction == GestureDirection::up || direction == GestureDirection::down)
            ? GestureHandling::captured : GestureHandling::ignored;
    }

    void AddAlarmPageViewModel::BeginGesture(const PanState& state) {
        this->activeColumn = this->WheelColumn(state.target);
        this->startValue = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
        const auto* wheel = this->Find(std::format("wheel{}", this->activeColumn));
        this->rowHeight = std::max(1.0f, wheel->Bounds().height / 5.0f);
        this->remainder = 0.0f;
        this->dragging = true;
    }

    void AddAlarmPageViewModel::UpdateGesture(const PanState& state) {
        const float distance = state.downY - state.currentY;
        const int steps = static_cast<int>(std::round(distance / this->rowHeight));
        int& value = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
        const int nextValue = _details::WrapAlarmValue(this->startValue + steps, this->activeColumn == 0 ? 24 : 60);
        if (value != nextValue) {
            value = nextValue;
            this->MarkChanged();
        }
        this->remainder = -distance + steps * this->rowHeight;
        this->RefreshWheel(this->activeColumn, this->remainder);
    }

    bool AddAlarmPageViewModel::EndGesture(const PanState& state, xaml::AnimationController& animations) {
        this->UpdateGesture(state);
        this->dragging = false;
        this->remainder = 0.0f;
        this->RefreshWheel(this->activeColumn, 0.0f);
        return true;
    }

    void AddAlarmPageViewModel::CancelGesture(xaml::Element& element) {
        if (this->dragging) {
            int& value = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
            value = this->startValue;
        }
        this->dragging = false;
        this->remainder = 0.0f;
        this->MarkChanged();
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
        case NavigationTrigger::editAlarm: {
            const auto* const alarm = dynamic_cast<const AlarmEditNavigationState*>(state.get());
            if (alarm == nullptr || alarm->Alarm() == nullptr) {
                return false;
            }
            this->settings = alarm->Settings();
            this->initialSettings = this->settings;
            this->editingAlarm = alarm->Alarm();
            this->isEditing = true;
            this->hasChanges = false;
            this->saved = false;
            this->Refresh();
            return true;
        }
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
        this->initialSettings = this->settings;
        this->dragging = false;
        this->remainder = 0.0f;
        this->saved = false;
        this->isEditing = false;
        this->hasChanges = false;
        this->editingAlarm = nullptr;
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
            const xaml::Element::Command deleteCommand = [this, uri]() {
                this->DeleteMelody(uri);
            };
            this->melodies.EmplaceBack(std::move(name), std::move(uri), selectCommand, deleteCommand);
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
        const bool changed = this->settings.melody != name || this->settings.melodyUri != uri;
        this->settings.melody = std::move(name);
        this->settings.melodyUri = std::move(uri);
        if (changed) {
            this->MarkChanged();
        }
        this->Refresh();
    }

    void AddAlarmPageViewModel::DeleteMelody(std::string_view uri) {
        auto edit = this->context.storage.Edit();
        const auto melody = std::find_if(edit->alarmMelodies.begin(), edit->alarmMelodies.end(), [uri](const AlarmMelody& value) {
            return value.uri == uri;
        });
        if (melody == edit->alarmMelodies.end()) {
            return;
        }
        edit->alarmMelodies.erase(melody);
        edit.Commit();
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
            item->AddCommand("DeleteCommand", melody == nullptr ? xaml::Element::Command{} : melody->DeleteCommand());
            return item;
        };
        collection.bind = [this](xaml::Element& element, xaml::Element::ItemTemplate itemTemplate) {
            element.SetItemsSource(this->melodies, std::move(itemTemplate));
        };
        registry->AddCollection("Melodies", collection);
        registry->AddCollection("ItemsSource", collection);
        result.controls["AlarmMelodyList"] = [this](xaml::BindingScope& scope) {
            return controls::AlarmMelodyList::Create(*this, this->melodies, scope);
        };
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
            if (this->saved || (this->isEditing && !this->hasChanges) || !this->context.saveAlarm) {
                return;
            }
            if (!this->context.saveAlarm(this->editingAlarm, this->settings)) {
                return;
            }
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
                this->MarkChanged();
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
            this->MarkChanged();
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
        _details::RefreshMelodySelection(*this->page, nullptr, this->settings.melodyUri);
        this->RefreshSaveButton();
    }

    void AddAlarmPageViewModel::RefreshSaveButton() {
        if (auto* button = this->Find("saveAlarmButton")) {
            const bool isEnabled = !this->isEditing || this->hasChanges;
            button->SetIsEnabled(isEnabled);
            button->SetOpacity(isEnabled ? 1.0f : 0.45f);
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
                text->SetOpacity(1.0f);
            }
        }
    }

    void AddAlarmPageViewModel::ChangeTime(int column, int steps) {
        int& value = column == 0 ? this->settings.hour : this->settings.minute;
        value = _details::WrapAlarmValue(value + steps, column == 0 ? 24 : 60);
        this->MarkChanged();
        this->RefreshWheel(column, 0.0f);
    }

    void AddAlarmPageViewModel::MarkChanged() {
        const bool wasChanged = this->hasChanges;
        this->hasChanges = !_details::AreEqual(this->settings, this->initialSettings);
        if (wasChanged == this->hasChanges) {
            return;
        }
        this->RefreshSaveButton();
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
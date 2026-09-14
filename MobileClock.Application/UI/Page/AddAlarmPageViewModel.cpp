#include "AddAlarmPageViewModel.h"

#include <XamlRuntime/RenderEngine.h>
#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeBindingPublisher.h>
#endif

#include "MobileClock.UI/Control/AlarmMelodyList.h"
#include "!Generated/MobileClock.Application/Xaml/Page/AddAlarmPage.xaml.h"
#include "../../Core/AppSessionController.h"
#include "../../Core/NavigationStates.h"
#include "MainPageViewModel.h"

#include <algorithm>
#include <format>
#include <cmath>

namespace mobileclock::application::ui::page::_details {
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

    bool AreEqual(const model::Alarm& left, const model::Alarm& right) {
        return left == right;
    }

    void RefreshMelodySelection(
        xaml::Element& element,
        const void* inheritedDataContext,
        std::string_view selectedUri) {
        const void* const dataContext = element.DataContext() == nullptr
            ? inheritedDataContext
            : element.DataContext();
        if (element.Id() == "melodyListItem") {
            const auto* const melody = static_cast<const view_model::AlarmMelodyViewModel*>(dataContext);
            const bool isSelected = melody != nullptr && melody->Id() == selectedUri;
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
} // namespace _details

namespace mobileclock::application::ui::page {

    AddAlarmPageViewModel::AddAlarmPageViewModel(core::PageContext& context)
        : context(context) {
        for (const model::AlarmMelody& melody : this->context.alarmMelodyRepository.Melodies()) {
            this->AddMelody(melody);
        }
        this->storageSubscription = this->context.alarmMelodyRepository.Subscribe([this]() {
            this->OnRepositoryChange();
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
    // mobileclock::ui::interface::IGestureTarget
    //
    xaml::Element* AddAlarmPageViewModel::FindScrollViewer(const xaml::Element& element) const {
        return nullptr;
    }

    mobileclock::ui::interface::GestureHandling AddAlarmPageViewModel::ResolveGesture(
        const mobileclock::ui::interface::IGestureTarget::PanState& state,
        mobileclock::ui::interface::GestureDirection direction) const {
        return this->WheelColumn(state.target) >= 0
            && (direction == mobileclock::ui::interface::GestureDirection::up || direction == mobileclock::ui::interface::GestureDirection::down)
            ? mobileclock::ui::interface::GestureHandling::captured : mobileclock::ui::interface::GestureHandling::ignored;
    }

    void AddAlarmPageViewModel::BeginGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state) {
        this->activeColumn = this->WheelColumn(state.target);
        this->startValue = this->activeColumn == 0 ? this->settings.hour : this->settings.minute;
        const auto* wheel = this->Find(std::format("wheel{}", this->activeColumn));
        this->rowHeight = std::max(1.0f, wheel->Bounds().height / 5.0f);
        this->remainder = 0.0f;
        this->dragging = true;
    }

    void AddAlarmPageViewModel::UpdateGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state) {
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

    bool AddAlarmPageViewModel::EndGesture(const mobileclock::ui::interface::IGestureTarget::PanState& state, xaml::AnimationController& animations) {
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
    std::unique_ptr<base::NavigationStateBase> AddAlarmPageViewModel::OnNavigatingFrom(const core::NavigationRequest&) {
        return {};
    }

    bool AddAlarmPageViewModel::OnNavigatingTo(
        const core::NavigationRequest& request,
        std::unique_ptr<base::NavigationStateBase> state) {
        switch (request.trigger) {
        case core::NavigationTrigger::createAlarm:
            this->Reset();
            return true;
        case core::NavigationTrigger::editAlarm: {
            const auto* const alarm = dynamic_cast<const core::AlarmEditNavigationState*>(state.get());
            if (alarm == nullptr || alarm->AlarmId().empty()) {
                return false;
            }
            this->settings = alarm->Settings();
            this->initialSettings = this->settings;
            this->editingAlarmId = alarm->AlarmId();
            this->isEditing = true;
            this->hasChanges = false;
            this->saved = false;
            this->Refresh();
            return true;
        }
#if defined(MOBILECLOCK_XAML_PREVIEWER)
        case core::NavigationTrigger::applySelectedMelody: {
            const auto* const melody = dynamic_cast<const core::AlarmMelodyNavigationState*>(state.get());
            if (melody == nullptr) {
                return false;
            }
            this->SetMelody(melody->Melody());
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
        this->settings = model::Alarm{};
        this->initialSettings = this->settings;
        this->dragging = false;
        this->remainder = 0.0f;
        this->saved = false;
        this->isEditing = false;
        this->hasChanges = false;
        this->editingAlarmId.clear();
        this->Refresh();
    }

    const model::Alarm& AddAlarmPageViewModel::Settings() const {
        return this->settings;
    }

    void AddAlarmPageViewModel::AddMelody(model::AlarmMelody alarmMelody) {
        const auto existing = std::find_if(this->melodies.begin(), this->melodies.end(), [&alarmMelody](const view_model::AlarmMelodyViewModel& melody) {
            return melody.Uri() == alarmMelody.uri;
        });
        if (existing == this->melodies.end()) {
            const xaml::Element::Command selectCommand = [this, alarmMelody]() {
                this->SetMelody(alarmMelody);
            };
            const xaml::Element::Command deleteCommand = [this, uri = alarmMelody.uri]() {
                this->DeleteMelody(uri);
            };
            this->melodies.EmplaceBack(std::move(alarmMelody), selectCommand, deleteCommand);
        } else {
            existing->SetName(std::move(alarmMelody.name));
        }
    }

    void AddAlarmPageViewModel::SetMelody(model::AlarmMelody alarmMelody) {
        if (!this->context.alarmMelodyRepository.SaveMelody(alarmMelody)) {
            return;
        }
        const bool changed = this->settings.melodyId != alarmMelody.id;
        this->settings.melodyId = alarmMelody.id;
        if (changed) {
            this->MarkChanged();
        }
        this->Refresh();
    }

    void AddAlarmPageViewModel::DeleteMelody(std::string_view uri) {
        this->context.alarmMelodyRepository.DeleteMelody(uri);
    }

    const xaml::ObservableCollection<view_model::AlarmMelodyViewModel>& AddAlarmPageViewModel::Melodies() const {
        return this->melodies;
    }

    void AddAlarmPageViewModel::ChooseAlarmMelody() {
        this->context.appSessionController.Dispatch(AppSessionSignal::requestAlarmMelody, {});
    }

    void AddAlarmPageViewModel::NavigateToMain() {
        this->context.navigator.Trigger(core::NavigationTrigger::navigateToMain);
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
            const auto* melody = static_cast<const view_model::AlarmMelodyViewModel*>(value);
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
            return control::AlarmMelodyList::Create(*this, this->melodies, scope);
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
    // mobileclock::ui::interface::IGestureTarget
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
            if (this->saved || (this->isEditing && !this->hasChanges)) {
                return;
            }
            const bool saved = this->isEditing
                ? this->context.alarmRepository.UpdateAlarm(this->editingAlarmId, this->settings)
                : this->context.alarmRepository.CreateAlarm(this->settings);
            if (!saved) {
                return;
            }
            this->saved = true;
            this->context.navigator.Trigger(core::NavigationTrigger::navigateToMain);
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

    void AddAlarmPageViewModel::OnRepositoryChange() {
        this->melodies.Clear();
        for (const model::AlarmMelody& melody : this->context.alarmMelodyRepository.Melodies()) {
            this->AddMelody(melody);
        }
    }

    void AddAlarmPageViewModel::RemoveMelody(std::string_view uri) {
        const auto melody = std::find_if(this->melodies.begin(), this->melodies.end(), [uri](const view_model::AlarmMelodyViewModel& value) {
            return value.Uri() == uri;
        });
        if (melody == this->melodies.end()) {
            return;
        }
        if (melody != this->melodies.end() && this->settings.melodyId == melody->Id()) {
            this->settings.melodyId.clear();
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
        _details::RefreshMelodySelection(*this->page, nullptr, this->settings.melodyId);
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
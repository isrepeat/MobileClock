#include <XamlRuntime/RuntimeMarkup/RuntimeReloadTransaction.h>
#include <XamlRuntime/RuntimeMarkup/XamlParser.h>
#include <XamlRuntime/Input.h>

#include "MobileClock.UI/Controls/InteractiveList.h"
#include "UI/ApplicationSession.h"

#include <stdexcept>
#include <iostream>
#include <fstream>
#include <thread>

namespace mobileclock::tests::_details {
    class Actions final : public ui::IApplicationActions {
    public:
        //
        // IApplicationActions
        //
        void CreateAlarm() override;
        void ChooseAlarmMelody() override;
        void ToggleAlarm() override;
        void UpdateApplication() override;
        void UploadScreenshot() override;
        void ShareLogs() override;
        void ExportLogs() override;
    };

    //
    // IApplicationActions
    //
    void Actions::CreateAlarm() {}
    void Actions::ChooseAlarmMelody() {}
    void Actions::ToggleAlarm() {}
    void Actions::UpdateApplication() {}
    void Actions::UploadScreenshot() {}
    void Actions::ShareLogs() {}
    void Actions::ExportLogs() {}

    void Check(bool condition, const std::string& message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    std::string Read(const std::string& path) {
        std::ifstream stream(path, std::ios::binary);
        Check(static_cast<bool>(stream), "Cannot open " + path);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    xaml::Element* Find(xaml::Element& node, std::string_view id) {
        if (node.Id() == id) {
            return &node;
        }
        for (const auto& child : node.Children()) {
            if (auto* found = Find(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    ui::controls::InteractiveList* List(xaml::Element& node) {
        if (auto* control = dynamic_cast<ui::controls::InteractiveList*>(&node)) {
            return control;
        }
        for (const auto& child : node.Children()) {
            if (auto* found = List(*child)) {
                return found;
            }
        }
        return nullptr;
    }
}

namespace mobileclock::tests::_details {

    void FinishNavigation(ui::ApplicationSession& session) {
        for (int iteration = 0; iteration < 100; ++iteration) {
            session.Update();
            if (!session.IsTransitioning()) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        Check(false, "Page transition did not finish");
    }

    void CompleteAlarm(ui::ApplicationSession& session) {
        Find(session.Root(), "addAlarmButton")->ExecuteCommand();
        Check(session.Root().Id() == "addAlarmPage", "Add button must open the form");
        FinishNavigation(session);
        Find(session.Root(), "saveAlarmButton")->ExecuteCommand();
        Check(session.Root().Id() == "root", "Save must return to main page");
        FinishNavigation(session);
    }

    void CheckAlarmForm() {
        Actions actions;
        ui::ApplicationSession session(actions);
        session.Initialize({720, 1440});
        session.SetAnimationPlaybackRate(100.0f);
        Check(!xaml::AnimationController::IsAnimating(session.Root()), "Initial page must not animate");
        auto* mainRoot = &session.Root();
        auto* items = Find(*List(session.Root()), "interactiveListItems");
        const auto initialCount = items->Children().size();
        Find(session.Root(), "addAlarmButton")->ExecuteCommand();
        Check(session.Root().Id() == "addAlarmPage", "Add page navigation");
        Check(xaml::AnimationController::IsAnimating(session.Root()), "Add page Show must animate");
        Check(xaml::AnimationController::IsAnimating(*mainRoot), "Main page Hide must animate");
        Check(session.Root().State<xaml::VisualTransform>().offsetX > 0,
            "Forward page must enter from the right: offset="
                + std::to_string(session.Root().State<xaml::VisualTransform>().offsetX)
                + ", width=" + std::to_string(session.Root().Bounds().width));
        FinishNavigation(session);
        Check(session.Root().State<xaml::VisualTransform>().offsetX == 0, "Forward transition must finish at zero");
        Check(!mainRoot->CanReceiveInput(), "Hidden main page must not receive input");
        Check(items->Children().size() == initialCount, "Opening form created an alarm");
        Check(Find(session.Root(), "wheel0Row2")->Text() == "07", "Initial hour");
        Check(Find(session.Root(), "wheel1Row2")->Text() == "30", "Initial minute");
        const auto pan = [&session](int column, float rows, bool cancel) {
            xaml::layout(session.Root(), {720, 1440});
            const auto bounds = Find(session.Root(), "wheel" + std::to_string(column))->Bounds();
            const float x = bounds.x + bounds.width / 2;
            const float y = bounds.y + bounds.height / 2;
            const float end = y - rows * bounds.height / 5;
            session.PointerDown(x, y);
            session.PointerMove(x, end);
            if (cancel) {
                session.CancelPointer();
            }
            else {
                session.PointerUp(x, end);
            }
        };
        pan(0, -8, false);
        pan(1, -31, false);
        Check(Find(session.Root(), "wheel0Row2")->Text() == "23", "Hour wrap backwards");
        Check(Find(session.Root(), "wheel1Row2")->Text() == "59", "Minute wrap backwards");
        pan(0, 1, false);
        pan(1, 1, false);
        Check(Find(session.Root(), "wheel0Row2")->Text() == "00", "Hour wrap forwards");
        Check(Find(session.Root(), "wheel1Row2")->Text() == "00", "Minute wrap forwards");
        pan(0, 3, true);
        Check(Find(session.Root(), "wheel0Row2")->Text() == "00", "Cancelled pan must restore hour");
        for (int day = 0; day < 5; ++day) {
            Find(session.Root(), "day" + std::to_string(day))->ExecuteCommand();
        }
        Check(Find(session.Root(), "repeatSummary")->Text() == "Однократно", "No days means once");
        Find(session.Root(), "melodyButton")->ExecuteCommand();
        Check(Find(session.Root(), "melodyChoices")->VisibilityValue() == xaml::attr::Visibility::visible, "Melody chooser");
        Find(session.Root(), "melodyChoices")->Children().front()->ExecuteCommand();
        Check(session.CurrentPageName() == "XiaomiThemesPage", "Theme selection navigation");
        FinishNavigation(session);
        Find(session.Root(), "melody1")->ExecuteCommand();
        Find(session.Root(), "applyButton")->ExecuteCommand();
        Check(session.CurrentPageName() == "AddAlarmPage", "Selected theme must return to the form");
        FinishNavigation(session);
        Check(Find(session.Root(), "melodyName")->Text() == "Lone Grass, Solitary Flower", "Theme must update the draft");
        Find(session.Root(), "melody1")->ExecuteCommand();
        Check(Find(session.Root(), "melodyName")->Text() == "Классика", "Melody selection");
        xaml::layout(session.Root(), {720, 1440});
        const auto toggle = Find(session.Root(), "vibrationToggle")->Bounds();
        session.PointerDown(toggle.x + toggle.width / 2, toggle.y + toggle.height / 2);
        session.PointerUp(toggle.x + toggle.width / 2, toggle.y + toggle.height / 2);
        Check(!Find(session.Root(), "vibrationToggle")->IsOn(), "Vibration input");
        Find(session.Root(), "saveAlarmButton")->ExecuteCommand();
        Check(session.Root().Id() == "root", "Save navigation");
        Check(xaml::AnimationController::IsAnimating(session.Root()), "Main page Show must animate");
        Check(session.Root().State<xaml::VisualTransform>().offsetX < 0, "Back navigation must enter from the left");
        FinishNavigation(session);
        Check(items->Children().size() == initialCount + 1, "Save must create exactly one alarm");
        const auto* alarm = static_cast<const ui::MainPageViewModel::Alarm*>(items->Children().back()->DataContext());
        Check(alarm != nullptr && alarm->Time() == "00:00", "Saved time");
        Check(alarm->Repeat() == "Однократно", "Saved repeat");
        Check(alarm->Settings().melody == "Классика" && !alarm->Settings().vibration, "Saved sound settings");
        Find(session.Root(), "addAlarmButton")->ExecuteCommand();
        Check(Find(session.Root(), "wheel0Row2")->Text() == "07", "New form must reset draft");
        FinishNavigation(session);
        Find(session.Root(), "backNavigation")->ExecuteCommand();
        FinishNavigation(session);
        Check(items->Children().size() == initialCount + 1, "Cancel must not create an alarm");
        Check(session.Root().State<xaml::VisualTransform>().offsetX == 0, "Back transition must finish at zero");
    }

}

int main(int argc, char** argv) {
    using namespace mobileclock::tests::_details;
    try {
        Check(argc == 2, "Source directory argument required");
        const std::string project = argv[1];
        const auto ast = xaml::runtime::XamlParser{}.Parse(
            "<?xml version='1.0'?><Page xmlns='urn:mobileclock:xaml'>\n<TextBlock text='a &amp; b &#x1F600; > c'/></Page>", "parser.xaml");
        Check(ast.children[0].location.line == 2 && ast.children[0].location.column == 1, "Source location");
        Check(ast.children[0].attributes[0].value.find("a & b") == 0, "XML entities");
        CheckAlarmForm();
        Actions actions;
        mobileclock::ui::ApplicationSession session(actions);
        session.Initialize({1080, 1920});
        session.SetAnimationPlaybackRate(100.0f);
        std::string diagnostics;
        for (const std::string name : {"MainPage", "SettingsPage", "AddAlarmPage"}) {
            const auto path = project + "/MobileClock.Application/UI/Pages/" + name + ".xaml";
            const auto markup = Read(path);
            Check(session.ReloadMarkup(name, markup, path, diagnostics), diagnostics);
        }
        const auto mainPath = project + "/MobileClock.Application/UI/Pages/MainPage.xaml";
        const auto mainMarkup = Read(mainPath);
        auto* control = List(session.Root());
        Check(control != nullptr, "MainPage native list");
        Check(session.ReloadMarkup("MainPage", mainMarkup, mainPath, diagnostics), diagnostics);
        Check(List(session.Root()) == control, "InteractiveList identity must survive reload");
        auto* moreButton = Find(session.Root(), "timelineMoreIcon");
        Check(moreButton && moreButton->Type() == xaml::ElementType::button, "More action must be a Button");
        Check(moreButton->Children().size() == 1, "Button must retain its icon content");
        const auto& icon = *moreButton->Children().front();
        Check(icon.Type() == xaml::ElementType::svgImage, "Button content must be SVG");
        const auto buttonBounds = moreButton->Bounds();
        const auto iconBounds = icon.Bounds();
        Check(buttonBounds.width == 60 && buttonBounds.height == 60, "Button hit area must be 60x60");
        Check(iconBounds.width == 40 && iconBounds.height == 40, "SVG content must be measured");
        Check(iconBounds.x - buttonBounds.x == 10 && iconBounds.y - buttonBounds.y == 10,
            "SVG must be centered inside the button");
        Check(xaml::HitTest(session.Root(), buttonBounds.x + 1, buttonBounds.y + 1) == moreButton,
            "Button must receive input outside the icon");
        Check(xaml::HitTest(session.Root(), buttonBounds.x + 30, buttonBounds.y + 30) == moreButton,
            "Icon must not intercept button input");
        auto* previous = &session.Root();
        const std::string start = "<Page xmlns='urn:mobileclock:xaml'>";
        for (const auto& broken : {start + "<Grid></Page>",
            start + "<TextBlock text='{Binding UpcommingAlarms}'/></Page>",
            start + "<TextBlock bad='1'/></Page>",
            start + "<Border><TextBlock/><TextBlock/></Border></Page>",
            start + "<Button><SvgImage/><SvgImage/></Button></Page>",
            start + "<TextBlock text='1' text='2'/></Page>",
            start + "<Unknown/></Page>"}) {
            Check(!session.ReloadMarkup("MainPage", broken, "broken.xaml", diagnostics), "Bad markup accepted");
            Check(&session.Root() == previous, "Failed reload replaced the root");
            Check(diagnostics.find("broken.xaml:") != std::string::npos, "Missing source diagnostics: " + diagnostics);
            session.Update();
        }
        Check(session.ReloadMarkup("MainPage", start + "<StackPanel><TextBlock id='status' text='{Binding Status}'/>"
            "<Button id='create' command='{Binding CreateAlarmCommand}'/></StackPanel></Page>", "binding.xaml", diagnostics), diagnostics);
        session.SetStatus("runtime status");
        Check(Find(session.Root(), "status")->Text() == "runtime status", "Live property notification");
        for (int iteration = 0; iteration < 30; ++iteration) {
            Check(session.ReloadMarkup("MainPage", start + "<TextBlock id='status' text='{Binding Status}'/></Page>",
                "repeat.xaml", diagnostics), diagnostics);
            session.SetStatus(std::to_string(iteration));
            Check(Find(session.Root(), "status")->Text() == std::to_string(iteration), "Repeated subscriptions");
        }
        Check(session.ReloadMarkup("MainPage", mainMarkup, mainPath, diagnostics), diagnostics);
        const auto templatePath = project + "/MobileClock.UI/Controls/InteractiveList.xaml";
        Check(session.ReloadMarkup("MainPage", Read(templatePath), templatePath, diagnostics), diagnostics);
        auto* list = List(session.Root());
        auto* items = Find(*list, "interactiveListItems");
        const auto count = items->Children().size();
        CompleteAlarm(session);
        Check(items->Children().size() == count + 1, "Runtime template must observe collection insertion");
        auto* oldContent = list->Content();
        auto brokenTemplate = Read(templatePath);
        const auto timeBinding = brokenTemplate.find("{Binding Time}");
        brokenTemplate.replace(timeBinding, std::string("{Binding Time}").size(), "{Binding MissingTime}");
        Check(!session.ReloadMarkup("MainPage", brokenTemplate, templatePath, diagnostics), "Bad item binding accepted");
        Check(list->Content() == oldContent, "Failed template reload replaced content");
        CompleteAlarm(session);
        Check(items->Children().size() == count + 2, "Old template subscription must survive failure");
        auto changedTemplate = Read(templatePath);
        const auto height = changedTemplate.find("height=\"200\"");
        changedTemplate.replace(height, std::string("height=\"200\"").size(), "height=\"250\"");
        Check(session.ReloadMarkup("MainPage", changedTemplate, templatePath, diagnostics), diagnostics);
        Check(List(session.Root()) == list, "Template reload must preserve native control");
        Check(Find(*list, "interactiveListItem")->Height() == 250, "Template visual was not updated");
        const auto tabsPath = project + "/MobileClock.UI/Controls/TimelineTabs.xaml";
        Check(session.ReloadMarkup("MainPage", Read(tabsPath), tabsPath, diagnostics), diagnostics);
        for (int index = 0; index < 10; ++index) {
            CompleteAlarm(session);
        }
        xaml::layout(session.Root(), {1080, 1920});
        auto* scroll = Find(*list, "interactiveListScrollViewer");
        scroll->SetVerticalOffset(80);
        Check(scroll->VerticalOffset() == 80, "Scroll fixture extent=" + std::to_string(scroll->Extent().height) + " viewport=" + std::to_string(scroll->Viewport().height));
        Check(session.ReloadMarkup("MainPage", changedTemplate, templatePath, diagnostics), diagnostics);
        Check(Find(*list, "interactiveListScrollViewer")->VerticalOffset() == 80, "Scroll offset was lost: " + std::to_string(Find(*list, "interactiveListScrollViewer")->VerticalOffset()) + " extent=" + std::to_string(Find(*list, "interactiveListScrollViewer")->Extent().height) + " viewport=" + std::to_string(Find(*list, "interactiveListScrollViewer")->Viewport().height));
        Find(*list, "interactiveListScrollViewer")->SetVerticalOffset(0);
        xaml::layout(session.Root(), {1080, 1920});
        auto* gesture = Find(*list, "interactiveListGestureTarget");
        const auto bounds = gesture->Bounds();
        session.PointerDown(bounds.x + 12, bounds.y + 12);
        session.PointerMove(bounds.x + 72, bounds.y + 12);
        Check(gesture->RenderOffsetX() == 60, "Pan did not start");
        Check(session.ReloadMarkup("MainPage", changedTemplate, templatePath, diagnostics), diagnostics);
        Check(Find(*list, "interactiveListGestureTarget")->RenderOffsetX() == 60, "Active pan was lost on template reload");
        session.PointerUp(bounds.x + 72, bounds.y + 12);
        session.CancelPointer();
        gesture = Find(*list, "interactiveListGestureTarget");
        xaml::AnimationController removalAnimations;
        const auto beforeRemoval = Find(*list, "interactiveListItems")->Children().size();
        Check(list->EndPan({session.Root(), *gesture, 0, 0, 0, 0, 300, 0}, removalAnimations),
            "Native removal handler was lost");
        std::this_thread::sleep_for(std::chrono::milliseconds(240));
        list->UpdateGestures(session.Root(), removalAnimations);
        Check(Find(*list, "interactiveListItems")->Children().size() == beforeRemoval - 1,
            "Swipe removal did not update runtime items");
        // Empty collections still resolve every item binding before committing.
        Check(session.ApplyPreviewScenario("MainPage", "{\"Alarms\":[]}", diagnostics), diagnostics);
        oldContent = list->Content();
        Check(!session.ReloadMarkup("MainPage", brokenTemplate, templatePath, diagnostics), "Empty collection skipped template validation");
        Check(list->Content() == oldContent, "Empty collection failure replaced content");
        // A writable runtime boolean updates the source when the native input path invokes it.
        bool enabled = false;
        auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
        registry->AddBoolean("Enabled", [&enabled]() { return enabled; }, {}, [&enabled](bool value) { enabled = value; });
        const auto toggleAst = xaml::runtime::XamlParser{}.Parse(start
            + "<ToggleSwitch id='toggle' isOn='{Binding Enabled, Mode=TwoWay}'/></Page>", "toggle.xaml");
        auto togglePage = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(toggleAst, {registry, "Test", {}}, {300, 300});
        auto* toggle = Find(*togglePage.root, "toggle");
        toggle->SetIsOn(true);
        toggle->ExecuteCommand();
        Check(enabled, "Two-way boolean did not update source");
        std::cout << "Runtime markup tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
#include <XamlRuntime/RuntimeMarkup/RuntimeReloadTransaction.h>
#include <XamlRuntime/RuntimeMarkup/XamlParser.h>

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

int main(int argc, char** argv) {
    using namespace mobileclock::tests::_details;
    try {
        Check(argc == 2, "Source directory argument required");
        const std::string project = argv[1];
        const auto ast = xaml::runtime::XamlParser{}.Parse(
            "<?xml version='1.0'?><Page xmlns='urn:mobileclock:xaml'>\n<TextBlock text='a &amp; b &#x1F600; > c'/></Page>", "parser.xaml");
        Check(ast.children[0].location.line == 2 && ast.children[0].location.column == 1, "Source location");
        Check(ast.children[0].attributes[0].value.find("a & b") == 0, "XML entities");
        Actions actions;
        mobileclock::ui::ApplicationSession session(actions);
        session.Initialize({1080, 1920});
        std::string diagnostics;
        for (const std::string name : {"MainPage", "SettingsPage", "StatisticsPage"}) {
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
        auto* previous = &session.Root();
        const std::string start = "<Page xmlns='urn:mobileclock:xaml'>";
        for (const auto& broken : {start + "<Grid></Page>",
            start + "<TextBlock text='{Binding UpcommingAlarms}'/></Page>",
            start + "<TextBlock bad='1'/></Page>",
            start + "<Border><TextBlock/><TextBlock/></Border></Page>",
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
        Find(session.Root(), "addAlarmButton")->ExecuteCommand();
        Check(items->Children().size() == count + 1, "Runtime template must observe collection insertion");
        auto* oldContent = list->Content();
        auto brokenTemplate = Read(templatePath);
        const auto timeBinding = brokenTemplate.find("{Binding Time}");
        brokenTemplate.replace(timeBinding, std::string("{Binding Time}").size(), "{Binding MissingTime}");
        Check(!session.ReloadMarkup("MainPage", brokenTemplate, templatePath, diagnostics), "Bad item binding accepted");
        Check(list->Content() == oldContent, "Failed template reload replaced content");
        Find(session.Root(), "addAlarmButton")->ExecuteCommand();
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
            Find(session.Root(), "addAlarmButton")->ExecuteCommand();
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
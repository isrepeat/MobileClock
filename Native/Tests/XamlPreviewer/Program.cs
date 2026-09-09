using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

internal static class Program {
    private static Type sessionType = null!;
    private static Type rendererType = null!;
    private static string resources = "";

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool SetDllDirectory(string path);

    [STAThread]
    private static int Main(string[] args) {
        string root = Path.GetFullPath(args[0]);
        string output = Path.Combine(root, "Native/UtilityHelpersLib/!VS_TMP/Build/Debug/x64/XamlPreviewer.WPF");
        Require(SetDllDirectory(output), "Cannot set native dependency directory");
        var assembly = Assembly.LoadFrom(Path.Combine(output, "XamlPreviewer.dll"));
        sessionType = assembly.GetType("XamlPreviewer.PreviewSession", true)!;
        rendererType = assembly.GetType("XamlPreviewer.PreviewRenderer", true)!;
        resources = Path.Combine(root, "Native/Resources");
        CheckPreviewDirectives();
        CheckScenarioInteractions(assembly);
        CheckConfigurableLists(assembly);
        Assembly.LoadFrom(Path.Combine(output, "ICSharpCode.AvalonEdit.dll"));
        CheckRefreshDuringSlowNavigation(assembly);
        CheckSourceLocations();

        using var outgoing = CreateSession(false);
        Require(Alpha(outgoing, 2) > 250 && Alpha(outgoing, 61) > 250,
            "Initial preview must be visible without a page transition");
        Require(!(bool)Call(outgoing, "Update")!, "Initial preview must not animate Show");
        for (int refresh = 0; refresh < 3; ++refresh) {
            using var rebuilt = CreateSession(false);
            Require(Alpha(rebuilt, 32) > 250 && !(bool)Call(rebuilt, "Update")!,
                "Rebuilding the preview must not restart the page transition");
        }
        using var incoming = CreateSession(true);
        Call(outgoing, "Transition", "main", "settings", false, false);
        Call(incoming, "Transition", "main", "settings", false, true);
        Require(Alpha(incoming, 32) == 0, "Incoming page should start outside the viewport");
        Thread.Sleep(130);
        Call(outgoing, "Update");
        Call(incoming, "Update");
        Require(Alpha(incoming, 2) == 0 && Alpha(incoming, 61) > 200,
            "Forward Show must enter from the right using registered native animation");
        Require(Alpha(outgoing, 61) == 0, "Outgoing Hide must move the actual native page");
        outgoing.Dispose();
        Thread.Sleep(1000);
        Call(incoming, "Update");
        Require(Alpha(incoming, 32) > 250, "Disposing outgoing surface broke incoming EGL display");

        Call(incoming, "Transition", "settings", "main", true, false);
        Thread.Sleep(1100);
        Call(incoming, "Update");
        Call(incoming, "Transition", "settings", "main", true, true);
        Thread.Sleep(130);
        Call(incoming, "Update");
        Require(Alpha(incoming, 2) > 200 && Alpha(incoming, 61) == 0,
            "Backward Show must enter from the left");
        Call(incoming, "SetAnimationSpeed", 4.0);
        Thread.Sleep(300);
        Call(incoming, "Update");
        Require(!(bool)Call(incoming, "Update")!, "Completed animation must stop");

        // ANGLE returns premultiplied color with an independent alpha channel.
        using var translucent = CreateSession(false, """<Page xmlns="urn:mobileclock:xaml" background="#FFFFFF" opacity="0.5"/>""");
        var pixels = Pixels(translucent);
        Require(pixels[0] >= 126 && pixels[0] <= 129 && pixels[3] >= 126 && pixels[3] <= 129,
            "Expected premultiplied RGB and alpha 0.5, not alpha squared");

        using var commandBinding = CreateSession(false, """
            <Page xmlns="urn:mobileclock:xaml">
                <Button command="{Binding CreateAlarmCommand}"/>
            </Page>
            """);

        using var conditional = CreateSession(true, """
            <Page xmlns="urn:mobileclock:xaml" background="#FFFFFF">
                <Page.Storyboards>
                    <Storyboard trigger="Show">
                        <Animation name="animationSettingsReveal" duration="1000"/>
                    </Storyboard>
                </Page.Storyboards>
            </Page>
            """);
        Call(conditional, "Transition", "main", "settings", false, true);
        Require(Alpha(conditional, 32) == 0, "Destination parameters were not passed to settings reveal");
        Call(conditional, "SetAnimationSpeed", 4.0);
        Thread.Sleep(300);
        Call(conditional, "Update");
        Require(Alpha(conditional, 32) > 250, "Registered settings reveal did not finish");

        // Repeated creation/destruction must leave surviving sessions usable.
        using var fast = CreateSession(true);
        Call(fast, "SetAnimationSpeed", 8.0);
        Call(fast, "Transition", "main", "settings", false, true);
        Thread.Sleep(180);
        Call(fast, "Update");
        Require(!(bool)Call(fast, "Update")!, "Custom speed 8x was clamped to the old maximum");
        for (int index = 0; index < 3; ++index) {
            using var next = CreateSession(true);
            Call(next, "Transition", "main", "settings", false, true);
            Call(next, "SetAnimationSpeed", 4.0);
            Thread.Sleep(300);
            Call(next, "Update");
            Require(Alpha(next, 32) > 250, "Repeated navigation failed");
        }
        Console.WriteLine("Native preview transitions, direction, lifetime, speed and alpha passed.");
        return 0;
    }

    private static IDisposable CreateSession(bool hidden, string? markup = null) {
        markup ??= """
            <Page xmlns="urn:mobileclock:xaml" background="#FFFFFF">
                <Page.Storyboards>
                    <Storyboard trigger="Show">
                        <Animation name="animationPageTransition" duration="1000" distance="1" easing="Linear"/>
                        <FloatAnimation property="opacity" from="0" to="1" duration="80" easing="Linear"/>
                    </Storyboard>
                    <Storyboard trigger="Hide">
                        <Animation name="animationPageTransition" duration="1000" distance="1" easing="Linear"/>
                        <FloatAnimation property="opacity" from="Current" to="0" duration="900" easing="Linear"/>
                    </Storyboard>
                </Page.Storyboards>
            </Page>
            """;
        using var data = JsonDocument.Parse("{}");
        var nativeRoot = rendererType.GetMethod("CreateRoot")!.Invoke(null, [markup, data.RootElement]);
        return (IDisposable)Activator.CreateInstance(sessionType, [nativeRoot, resources, 64, 16, hidden])!;
    }

    private static void CheckPreviewDirectives() {
        var getScenarioPath = rendererType.GetMethod("GetPreviewScenarioPath")!;
        const string markup = """
            <?mobileclock-preview width="400" height="300"?>
            <?mobileclock-preview-scenario path="Scenarios/MainPage.json"?>
            <Page xmlns="urn:mobileclock:xaml"/>
            """;
        Require((string?)getScenarioPath.Invoke(null, [markup]) == "Scenarios/MainPage.json",
            "Page-specific scenario directive was not parsed");
        Require(getScenarioPath.Invoke(null, ["<Page xmlns=\"urn:mobileclock:xaml\"/>"]) is null,
            "Missing page-specific scenario directive must use global scenarios");

        bool rejected = false;
        try {
            getScenarioPath.Invoke(null, ["<?mobileclock-preview-scenario?><Page xmlns=\"urn:mobileclock:xaml\"/>"]);
        }
        catch (TargetInvocationException error) when (error.InnerException is InvalidDataException) {
            rejected = true;
        }
        Require(rejected, "Page-specific scenario directive without path was accepted");
    }

    private static void CheckScenarioInteractions(Assembly assembly) {
        var type = assembly.GetType("XamlPreviewer.ScenarioInteraction", true)!;
        var handleTap = type.GetMethod("HandleTap")!;
        var getTap = type.GetMethod("GetTap")!;
        var scenario = JsonNode.Parse("""
            {
              "IsEnabled": false,
              "Status": { "Text": "Idle" },
              "$interactions": {
                "enableButton": { "tap": { "type": "set", "path": "IsEnabled", "value": true } },
                "toggleButton": { "tap": { "type": "toggle", "path": "IsEnabled" } },
                "statusButton": { "tap": { "type": "set", "path": "Status.Text", "value": "Ready" } },
                "settingsButton": { "tap": { "type": "navigate", "target": "SettingsPage" } }
              }
            }
            """)!.AsObject();
        Require((bool)handleTap.Invoke(null, [scenario, "enableButton"])!, "Scenario set interaction was not handled");
        Require(scenario["IsEnabled"]!.GetValue<bool>(), "Scenario set interaction did not update state");
        Require((bool)handleTap.Invoke(null, [scenario, "toggleButton"])!, "Scenario toggle interaction was not handled");
        Require(!scenario["IsEnabled"]!.GetValue<bool>(), "Scenario toggle interaction did not update state");
        Require((bool)handleTap.Invoke(null, [scenario, "statusButton"])!, "Nested scenario set interaction was not handled");
        Require(scenario["Status"]!["Text"]!.GetValue<string>() == "Ready", "Nested scenario state did not update");
        var navigation = (JsonObject)getTap.Invoke(null, [scenario, "settingsButton"])!;
        Require(navigation["type"]!.GetValue<string>() == "navigate"
            && navigation["target"]!.GetValue<string>() == "SettingsPage",
            "Scenario navigation interaction was not read");
        Require(!(bool)handleTap.Invoke(null, [scenario, "missingButton"])!, "Missing scenario interaction was handled");
    }

    private static void CheckConfigurableLists(Assembly assembly) {
        var type = assembly.GetType("XamlPreviewer.PreviewerSettings", true)!;
        var parse = type.GetMethod("Parse")!;
        object ParseLists(string text) {
            var node = System.Text.Json.Nodes.JsonNode.Parse(text)!.AsObject();
            node["XamlDirectory"] = resources;
            node["ScenariosPath"] = "unused.json";
            node["ResourcesDirectory"] = resources;
            return parse.Invoke(null, [node.ToJsonString(), "unused.json"])!;
        }
        var defaults = ParseLists("{}");
        Require(((double[])type.GetProperty("AnimationPlaybackRates")!.GetValue(defaults)!)
            .SequenceEqual(new[] { 0.1, 0.25, 0.5, 1.0, 2.0, 4.0 }), "Legacy settings lost default speeds");
        Require(((Array)type.GetProperty("PreviewResolutions")!.GetValue(defaults)!).Length == 4,
            "Legacy settings lost default resolutions");
        const string json = """
            {"AnimationPlaybackRates":[0.05,8,0.05],"AnimationPlaybackRate":8,
             "PreviewResolutions":[{"Name":"Custom","Width":900,"Height":1800}]}
            """;
        var custom = ParseLists(json);
        Require(((double[])type.GetProperty("AnimationPlaybackRates")!.GetValue(custom)!)
            .SequenceEqual(new[] { 0.05, 8.0 }), "Custom speeds or their order were lost");
        Require((double)type.GetProperty("AnimationPlaybackRate")!.GetValue(custom)! == 8,
            "Custom selected speed was clamped");
        string saved = (string)type.GetMethod("ToJson")!.Invoke(custom, null)!;
        using var data = JsonDocument.Parse(saved);
        Require(data.RootElement.GetProperty("PreviewResolutions")[0].GetProperty("Width").GetInt32() == 900,
            "Custom resolution did not round-trip");
        foreach (string invalid in new[] {
            """{"AnimationPlaybackRates":[]} """,
            """{"AnimationPlaybackRates":[0]}""",
            """{"PreviewResolutions":null}""",
            """{"PreviewResolutions":[{"Name":"Bad","Width":0,"Height":10}]}"""
        }) {
            bool rejected = false;
            try { ParseLists(invalid); }
            catch (TargetInvocationException error) when (error.InnerException is InvalidDataException) { rejected = true; }
            Require(rejected, "Invalid configurable list was accepted");
        }
    }

    private static void CheckRefreshDuringSlowNavigation(Assembly assembly) {
        string directory = Path.Combine(Path.GetTempPath(), "MobileClock-preview-refresh-" + Guid.NewGuid());
        Directory.CreateDirectory(directory);
        const string markup = """
            <Page xmlns="urn:mobileclock:xaml" background="#FFFFFF">
                <Page.Storyboards>
                    <Storyboard trigger="Show">
                        <Animation name="animationPageTransition" duration="240" distance="1" easing="Linear"/>
                    </Storyboard>
                    <Storyboard trigger="Hide">
                        <Animation name="animationPageTransition" duration="240" distance="1" easing="Linear"/>
                    </Storyboard>
                </Page.Storyboards>
            </Page>
            """;
        string page = Path.Combine(directory, "SettingsPage.xaml");
        string scenarios = Path.Combine(directory, "scenarios.json");
        string settingsPath = Path.Combine(directory, "settings.json");
        File.WriteAllText(page, markup);
        File.WriteAllText(scenarios, "{}");
        string json = JsonSerializer.Serialize(new {
            XamlDirectory = directory, ScenariosPath = scenarios, ResourcesDirectory = resources,
            PreviewWidth = 64, PreviewHeight = 16, AnimationPlaybackRate = 0.1
        });
        var settingsType = assembly.GetType("XamlPreviewer.PreviewerSettings", true)!;
        var settings = settingsType.GetMethod("Parse")!.Invoke(null, [json, settingsPath]);
        json = (string)settingsType.GetMethod("ToJson")!.Invoke(settings, null)!;
        File.WriteAllText(settingsPath, json);
        var windowType = assembly.GetType("XamlPreviewer.MainWindow", true)!;
        var window = (System.Windows.Window)Activator.CreateInstance(windowType)!;
        const BindingFlags flags = BindingFlags.Instance | BindingFlags.NonPublic;
        object Get(string name) => windowType.GetField(name, flags)!.GetValue(window)!;
        void Set(string name, object? value) => windowType.GetField(name, flags)!.SetValue(window, value);
        void Invoke(string name, params object?[] args) => windowType.GetMethod(name, flags)!.Invoke(window, args);
        try {
            Set("settings", settings);
            Set("updatingEditors", true);
            foreach (var entry in new[] { ("ScenarioEditor", "{}"), ("SettingsEditor", json) }) {
                var jsonEditor = Get(entry.Item1);
                jsonEditor.GetType().GetProperty("Text")!.SetValue(jsonEditor, entry.Item2);
            }
            Set("updatingEditors", false);
            var picker = (ComboBox)Get("PagePicker");
            picker.ItemsSource = new[] { "SettingsPage.xaml" };
            picker.SelectedItem = "SettingsPage.xaml";
            var renderTimer = (DispatcherTimer)Get("renderTimer");
            renderTimer.Stop();
            using var outgoing = CreateSession(false, markup);
            using var incoming = CreateSession(true, markup);
            Call(outgoing, "SetAnimationSpeed", 0.1);
            Call(incoming, "SetAnimationSpeed", 0.1);
            Set("previewSession", incoming);
            var editor = Get("MarkupEditor");
            var editorType = editor.GetType();
            var modeType = windowType.GetNestedType("EditorMode", BindingFlags.NonPublic)!;
            Set("editorMode", Enum.Parse(modeType, "Xaml"));
            Invoke("PreviewElementSelected", incoming, (3, 5));
            int selectedOffset = (int)editorType.GetProperty("CaretOffset")!.GetValue(editor)!;
            Require(selectedOffset > 0, "Alt selection did not move the XAML caret");
            Set("editorMode", Enum.Parse(modeType, "Scenarios"));
            Invoke("PreviewElementSelected", incoming, (1, 1));
            Require((int)editorType.GetProperty("CaretOffset")!.GetValue(editor)! == selectedOffset,
                "Element selection must be ignored outside the XAML editor");
            Set("editorMode", Enum.Parse(modeType, "Xaml"));
            Invoke("StartPageTransition", outgoing, incoming, ("main", "settings", false));
            for (int refresh = 0; refresh < 3; ++refresh) {
                Thread.Sleep(200);
                Invoke("ExternalRefreshTimerTick", null, EventArgs.Empty);
                Require(!renderTimer.IsEnabled, "Unchanged watcher event scheduled a preview rebuild");
                Invoke("AnimationTimerTick", null, EventArgs.Empty);
                Require(ReferenceEquals(Get("outgoingSession"), outgoing), "Slow navigation ended prematurely");
            }
            Require(Alpha(incoming, 2) == 0 && Alpha(incoming, 61) > 250,
                "0.1x transition must still be in progress after watcher refreshes");
            Thread.Sleep(2000);
            Invoke("AnimationTimerTick", null, EventArgs.Empty);
            Invoke("AnimationTimerTick", null, EventArgs.Empty);
            Require(Alpha(incoming, 2) > 250, "Slow transition did not reach the full page");
            File.WriteAllText(page, markup + "\n<!-- external edit -->");
            Invoke("ExternalRefreshTimerTick", null, EventArgs.Empty);
            Require(renderTimer.IsEnabled, "Real external XAML changes must still refresh the preview");

            using var closingOutgoing = CreateSession(false, markup);
            using var closingIncoming = CreateSession(true, markup);
            Call(closingOutgoing, "SetAnimationSpeed", 0.1);
            Call(closingIncoming, "SetAnimationSpeed", 0.1);
            Set("previewSession", closingIncoming);
            Invoke("StartPageTransition", closingOutgoing, closingIncoming, ("main", "settings", false));
            Require((bool)Call(closingIncoming, "Update")!, "Close test must run during a transition");
            Invoke("WindowClosing", null, new System.ComponentModel.CancelEventArgs());
            typeof(System.Windows.Window).GetMethod("OnDeactivated", flags)!.Invoke(window, [EventArgs.Empty]);
            Invoke("UpdateElementInspection");
            Invoke("QueueExternalRefresh");
            Invoke("ExternalRefreshTimerTick", null, EventArgs.Empty);
            Invoke("RenderTimerTick", null, EventArgs.Empty);
            Invoke("AnimationTimerTick", null, EventArgs.Empty);
            Invoke("ScheduleRender");
            Require(Get("previewSession") is null && Get("outgoingSession") is null,
                "Closing must clear both session references before later window events");
            foreach (string timer in new[] { "renderTimer", "externalRefreshTimer", "animationTimer",
                "previewZoomTimer", "smoothScrollTimer" }) {
                Require(!((DispatcherTimer)Get(timer)).IsEnabled, "Closing left a timer active: " + timer);
            }
            var disposed = sessionType.GetField("isDisposed", flags)!;
            Require((bool)disposed.GetValue(closingOutgoing)! && (bool)disposed.GetValue(closingIncoming)!,
                "Closing during navigation must dispose both native sessions");
        } finally {
            window.Close();
            Require(Path.GetFullPath(directory).StartsWith(
                Path.Combine(Path.GetFullPath(Path.GetTempPath()), "MobileClock-preview-refresh-"),
                StringComparison.OrdinalIgnoreCase), "Unexpected test cleanup path");
            Directory.Delete(directory, true);
        }
    }

    private static object? Call(IDisposable session, string name, params object[] args) {
        return sessionType.GetMethod(name)!.Invoke(session, args);
    }

    private static void CheckSourceLocations() {
        const string markup = """
            <Page xmlns="urn:mobileclock:xaml">
                <ListView itemsSource="{Binding Items}">
                    <ListView.ItemTemplate>
                        <DataTemplate>
                            <Border width="64" height="8" background="#FFFFFF"/>
                        </DataTemplate>
                    </ListView.ItemTemplate>
                </ListView>
            </Page>
            """;
        using var data = JsonDocument.Parse("""{"Items":[{},{}]}""");
        var locations = new Dictionary<IntPtr, (int Line, int Column)>();
        var nativeRoot = rendererType.GetMethod("CreateRootWithLocations")!.Invoke(null,
            [markup, data.RootElement, locations]);
        using var session = (IDisposable)Activator.CreateInstance(sessionType,
            [nativeRoot, resources, 64, 16, false])!;
        Require(locations.Count == 4 && locations.Values.Count(value => value.Line == 5) == 2,
            "Template instances without id must map to the original template line");
        Call(session, "SetSourceLocations", locations);
        var surface = (Grid)sessionType.GetProperty("Surface")!.GetValue(session)!;
        surface.Measure(new System.Windows.Size(64, 16));
        surface.Arrange(new System.Windows.Rect(0, 0, 64, 16));
        (int Line, int Column) selected = default;
        EventHandler<(int Line, int Column)> handler = (_, location) => selected = location;
        sessionType.GetEvent("ElementSelected")!.AddEventHandler(session, handler);
        Call(session, "SetElementInspectionEnabled", true);
        sessionType.GetMethod("InspectElement", BindingFlags.Instance | BindingFlags.NonPublic)!
            .Invoke(session, [new System.Windows.Point(1, 1)]);
        Require(selected.Line == 5, $"Visual inspection must select a non-interactive template element, got {selected}");
    }

    private static byte[] Pixels(IDisposable session) {
        var surface = (Grid)sessionType.GetProperty("Surface")!.GetValue(session)!;
        var bitmap = (BitmapSource)((Image)surface.Children[0]).Source;
        Require(bitmap.Format == PixelFormats.Pbgra32, "Preview must use premultiplied pixel format");
        var pixels = new byte[64 * 16 * 4];
        bitmap.CopyPixels(pixels, 64 * 4, 0);
        return pixels;
    }

    private static byte Alpha(IDisposable session, int x) {
        return Pixels(session)[(8 * 64 + x) * 4 + 3];
    }

    private static void Require(bool condition, string message) {
        if (!condition) {
            throw new InvalidOperationException(message);
        }
    }
}
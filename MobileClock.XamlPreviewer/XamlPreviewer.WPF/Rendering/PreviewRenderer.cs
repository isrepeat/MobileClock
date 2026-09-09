using System.Globalization;
using System.Runtime.InteropServices;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Media;
using System.Xml.Linq;
using System.Xml;

namespace XamlPreviewer;

internal static class PreviewRenderer {
    private sealed record Style(string TargetType, IReadOnlyDictionary<string, string> Setters);

    public static (int Width, int Height)? GetPreviewResolution(string markup) {
        var document = XDocument.Parse(markup, LoadOptions.None);
        var instruction = PreviewRenderer.FindProcessingInstruction(document, "mobileclock-preview");
        if (instruction is null) {
            return null;
        }
        var attributes = PreviewRenderer.ParseInstructionAttributes(instruction);
        if (!int.TryParse(attributes.Attribute("width")?.Value, out var width)
            || !int.TryParse(attributes.Attribute("height")?.Value, out var height)
            || width <= 0 || height <= 0) {
            throw new InvalidDataException("mobileclock-preview требует положительные width и height.");
        }
        return (width, height);
    }

    public static string? GetPreviewScenarioPath(string markup) {
        var document = XDocument.Parse(markup, LoadOptions.None);
        var instruction = PreviewRenderer.FindProcessingInstruction(document, "mobileclock-preview-scenario");
        if (instruction is null) {
            return null;
        }
        var path = PreviewRenderer.ParseInstructionAttributes(instruction).Attribute("path")?.Value;
        if (string.IsNullOrWhiteSpace(path)) {
            throw new InvalidDataException("mobileclock-preview-scenario требует непустой path.");
        }
        return path;
    }

    public static IntPtr CreateRoot(string markup, JsonElement data) {
        return CreateRootWithLocations(markup, data, new Dictionary<IntPtr, (int Line, int Column)>(), null);
    }

    public static IntPtr CreateRootWithLocations(
        string markup,
        JsonElement data,
        Dictionary<IntPtr, (int Line, int Column)> locations,
        string? xamlDirectory) {
        var document = XDocument.Parse(markup, LoadOptions.SetLineInfo);
        MarkupValidator.Validate(document);
        var rootNode = document.Root
            ?? throw new InvalidDataException("Разметка не содержит корневого элемента.");
        var root = PreviewRenderer.Build(
            rootNode,
            data,
            locations,
            PreviewRenderer.ExtractStyles(rootNode),
            xamlDirectory);
        try {
            PreviewRenderer.ApplyInitialVisualStates(root, data);
            return root;
        }
        catch {
            NativeRuntime.xr_destroy_element(root);
            throw;
        }
    }

    public static SolidColorBrush ParseBrush(string value) {
        return new SolidColorBrush((Color)ColorConverter.ConvertFromString(value));
    }

    private static XProcessingInstruction? FindProcessingInstruction(XDocument document, string target) {
        return document.Nodes().OfType<XProcessingInstruction>()
            .FirstOrDefault(node => node.Target == target);
    }

    private static XElement ParseInstructionAttributes(XProcessingInstruction instruction) {
        try {
            return XElement.Parse($"<preview {instruction.Data} />");
        }
        catch (System.Xml.XmlException exception) {
            throw new InvalidDataException($"Некорректная директива {instruction.Target}.", exception);
        }
    }

    private static IntPtr Build(
        XElement node,
        JsonElement data,
        Dictionary<IntPtr, (int Line, int Column)> locations,
        IReadOnlyDictionary<string, Style> styles,
        string? xamlDirectory) {
        if (node.Name.NamespaceName == "using:mobileclock.ui.controls") {
            return PreviewRenderer.BuildUserControl(node, data, locations, xamlDirectory);
        }
        var element = NativeRuntime.xr_create_element(node.Name.LocalName);
        if (element == IntPtr.Zero) {
            throw new InvalidOperationException(NativeRuntime.GetLastError());
        }

        try {
            var source = (IXmlLineInfo)node;
            locations[element] = (source.LineNumber, Math.Max(1, source.LinePosition - 1));
            var styleReference = PreviewRenderer.Attribute(node, "style");
            if (styleReference is not null) {
                var key = PreviewRenderer.StaticResourceKey(styleReference);
                if (!styles.TryGetValue(key, out var style)) {
                    throw new InvalidDataException($"Ресурс стиля не найден: {key}.");
                }
                if (style.TargetType != node.Name.LocalName) {
                    throw new InvalidDataException("TargetType стиля не соответствует элементу.");
                }
                foreach (var setter in style.Setters) {
                    PreviewRenderer.ApplyAttribute(element, setter.Key, setter.Value, data);
                }
            }
            foreach (var attribute in node.Attributes()) {
                if (attribute.IsNamespaceDeclaration || attribute.Name.LocalName == "style") {
                    continue;
                }
                PreviewRenderer.ApplyAttribute(element, attribute.Name.LocalName, attribute.Value, data);
            }

            if (node.Elements().Any(child => child.Name.LocalName == $"{node.Name.LocalName}.Animation")) {
                throw new InvalidDataException("Use <Animation> inside an event <Storyboard>.");
            }
            PreviewRenderer.ApplyDefinitions(element, node);
            PreviewRenderer.ApplyStoryboards(element, node);
            if (node.Name.LocalName == "ListView") {
                PreviewRenderer.BuildListViewItems(element, node, data, locations, styles, xamlDirectory);
            }
            else {
                foreach (var childNode in node.Elements().Where(PreviewRenderer.IsVisualElement)) {
                    PreviewRenderer.AddChild(element, PreviewRenderer.Build(childNode, data, locations, styles, xamlDirectory));
                }
            }
            PreviewRenderer.ApplyVisualStateGroups(element, node);

            return element;
        }
        catch {
            NativeRuntime.xr_destroy_element(element);
            throw;
        }
    }

    private static IntPtr BuildUserControlRoot(
        XElement userControl,
        JsonElement data,
        Dictionary<IntPtr, (int Line, int Column)> locations,
        string? xamlDirectory) {
        var content = userControl.Elements().Where(PreviewRenderer.IsVisualElement).ToArray();
        if (content.Length != 1) {
            throw new InvalidDataException("UserControl требует ровно один визуальный корневой элемент.");
        }
        return PreviewRenderer.Build(
            content[0],
            data,
            locations,
            PreviewRenderer.ExtractStyles(userControl),
            xamlDirectory);
    }

    private static IntPtr BuildUserControl(
        XElement invocation,
        JsonElement data,
        Dictionary<IntPtr, (int Line, int Column)> locations,
        string? xamlDirectory) {
        if (string.IsNullOrEmpty(xamlDirectory)) {
            throw new InvalidDataException("Для предпросмотра UserControl требуется каталог XAML.");
        }
        var path = PreviewRenderer.ResolveUserControlPath(xamlDirectory, invocation.Name.LocalName);
        if (!File.Exists(path)) {
            throw new InvalidDataException($"UserControl не найден: {path}.");
        }
        var document = XDocument.Load(path, LoadOptions.SetLineInfo);
        var userControl = document.Root;
        if (userControl?.Name.LocalName != "UserControl") {
            throw new InvalidDataException($"{path} должен иметь корень <UserControl>.");
        }
        var controlDefinition = new XElement(userControl);
        var itemsSource = PreviewRenderer.Attribute(invocation, "itemsSource");
        if (itemsSource is not null) {
            foreach (var listView in controlDefinition.Descendants()
                .Where(element => element.Name.LocalName == "ListView")) {
                var source = listView.Attributes().FirstOrDefault(attribute => attribute.Name.LocalName == "itemsSource");
                if (source?.Value == "{Binding ItemsSource}") {
                    source.Value = itemsSource;
                }
            }
        }
        var root = PreviewRenderer.BuildUserControlRoot(controlDefinition, data, locations, xamlDirectory);
        try {
            var className = userControl.Attribute(XName.Get("Class", "http://schemas.microsoft.com/winfx/2006/xaml"))?.Value;
            foreach (var attribute in invocation.Attributes()) {
                if (!attribute.IsNamespaceDeclaration && attribute.Name.LocalName != "itemsSource") {
                    PreviewRenderer.ApplyAttribute(root, attribute.Name.LocalName, attribute.Value, data);
                }
            }
            if (!string.IsNullOrEmpty(className)) {
            }
            return root;
        }
        catch {
            NativeRuntime.xr_destroy_element(root);
            throw;
        }
    }

    private static string ResolveUserControlPath(string xamlDirectory, string controlName) {
        var pageControlsPath = Path.Combine(xamlDirectory, "Controls", controlName + ".xaml");
        if (File.Exists(pageControlsPath)) {
            return pageControlsPath;
        }
        var directory = new DirectoryInfo(xamlDirectory);
        while (directory is not null) {
            var sharedControlPath = Path.Combine(
                directory.FullName,
                "MobileClock.UI",
                "Controls",
                controlName + ".xaml");
            if (File.Exists(sharedControlPath)) {
                return sharedControlPath;
            }
            directory = directory.Parent;
        }
        return Path.GetFullPath(Path.Combine(
            xamlDirectory,
            "..",
            "..",
            "MobileClock.UI",
            "Controls",
            controlName + ".xaml"));
    }

    private static IReadOnlyDictionary<string, Style> ExtractStyles(XElement page) {
        var styles = new Dictionary<string, Style>(StringComparer.Ordinal);
        var resources = page.Elements().FirstOrDefault(element => element.Name.LocalName
            == $"{page.Name.LocalName}.Resources");
        if (resources is null) {
            return styles;
        }
        var entries = resources.Elements().SingleOrDefault(element => element.Name.LocalName == "ResourceDictionary")?
            .Elements() ?? resources.Elements();
        foreach (var element in entries) {
            if (element.Name.LocalName != "Style") {
                throw new InvalidDataException("Page.Resources поддерживает только Style.");
            }
            var key = element.Attribute(XName.Get("Key", "http://schemas.microsoft.com/winfx/2006/xaml"))?.Value;
            var targetType = PreviewRenderer.Attribute(element, "TargetType");
            if (string.IsNullOrEmpty(key) || string.IsNullOrEmpty(targetType)) {
                throw new InvalidDataException("Style требует x:Key и TargetType.");
            }
            var setters = element.Elements().ToDictionary(
                setter => PreviewRenderer.PropertyName(PreviewRenderer.Attribute(setter, "Property")),
                setter => PreviewRenderer.Attribute(setter, "Value") ?? throw new InvalidDataException("Setter требует Value."));
            styles.Add(key, new Style(targetType, setters));
        }
        return styles;
    }

    private static string StaticResourceKey(string value) {
        const string prefix = "{StaticResource ";
        if (!value.StartsWith(prefix, StringComparison.Ordinal) || !value.EndsWith('}')) {
            throw new InvalidDataException("style должен использовать {StaticResource Key}.");
        }
        return value[prefix.Length..^1].Trim();
    }

    private static string PropertyName(string? value) {
        if (string.IsNullOrEmpty(value)) {
            throw new InvalidDataException("Setter требует Property.");
        }
        return char.ToLowerInvariant(value[0]) + value[1..];
    }

    private static void BuildListViewItems(
        IntPtr listView,
        XElement node,
        JsonElement data,
        Dictionary<IntPtr, (int Line, int Column)> locations,
        IReadOnlyDictionary<string, Style> styles,
        string? xamlDirectory) {
        var source = PreviewRenderer.ResolveElement(PreviewRenderer.Attribute(node, "itemsSource"), data);
        var template = node.Elements()
            .FirstOrDefault(element => element.Name.LocalName == "ListView.ItemTemplate")?
            .Elements()
            .SingleOrDefault(element => element.Name.LocalName == "DataTemplate")?
            .Elements()
            .SingleOrDefault();
        if (source is not JsonElement items || items.ValueKind != JsonValueKind.Array || template is null) {
            return;
        }

        foreach (var item in items.EnumerateArray()) {
            PreviewRenderer.AddChild(listView, PreviewRenderer.Build(template, item, locations, styles, xamlDirectory));
        }
    }

    private static void ApplyDefinitions(IntPtr element, XElement node) {
        var columns = node.Elements().FirstOrDefault(child => child.Name.LocalName == "columnDefinitions");
        if (columns is not null) {
            PreviewRenderer.SetAttribute(element, "columns", string.Join(",", columns.Elements().Select(
                definition => PreviewRenderer.Attribute(definition, "width") ?? "*")));
        }

        var rows = node.Elements().FirstOrDefault(child => child.Name.LocalName == "rowDefinitions");
        if (rows is not null) {
            PreviewRenderer.SetAttribute(element, "rows", string.Join(",", rows.Elements().Select(
                definition => PreviewRenderer.Attribute(definition, "height") ?? "*")));
        }
    }

    private static void ApplyStoryboards(IntPtr element, XElement node) {
        foreach (var collection in node.Elements().Where(child => child.Name.LocalName
            == $"{node.Name.LocalName}.Storyboards")) {
            foreach (var storyboard in collection.Elements()) {
                if (storyboard.Name.LocalName != "Storyboard") {
                    throw new InvalidDataException("Only Storyboard is allowed in Storyboards.");
                }
                var trigger = PreviewRenderer.ParseTrigger(PreviewRenderer.Attribute(storyboard, "trigger"));
                if (!storyboard.HasElements) {
                    NativeRuntime.Ensure(NativeRuntime.xr_add_storyboard_animation(
                        element, trigger, null, [], [], 0) != 0);
                }
                foreach (var track in storyboard.Elements()) {
                    if (track.HasElements) {
                        throw new InvalidDataException("Animation tracks do not support children.");
                    }
                    if (track.Name.LocalName == "Animation") {
                        var name = Attribute(track, "name");
                        if (string.IsNullOrEmpty(name)) {
                            throw new InvalidDataException("<Animation> requires name.");
                        }
                        var options = track.Attributes().Where(attribute => !attribute.IsNamespaceDeclaration
                            && attribute.Name.LocalName != "name").ToArray();
                        var keys = new IntPtr[options.Length];
                        var values = new IntPtr[options.Length];
                        try {
                            for (var index = 0; index < options.Length; ++index) {
                                keys[index] = Marshal.StringToCoTaskMemUTF8(options[index].Name.LocalName);
                                values[index] = Marshal.StringToCoTaskMemUTF8(options[index].Value);
                            }
                            NativeRuntime.Ensure(NativeRuntime.xr_add_storyboard_animation(
                                element, trigger, name, keys, values, options.Length) != 0);
                        }
                        finally {
                            foreach (var key in keys) {
                                Marshal.FreeCoTaskMem(key);
                            }
                            foreach (var value in values) {
                                Marshal.FreeCoTaskMem(value);
                            }
                        }
                        continue;
                    }
                    if (track.Name.LocalName != "FloatAnimation") {
                        throw new InvalidDataException("Use Animation or FloatAnimation inside Storyboard.");
                    }
                    var property = PreviewRenderer.ParseProperty(PreviewRenderer.Attribute(track, "property"));
                    var from = PreviewRenderer.ParseAnimationValue(PreviewRenderer.Attribute(track, "from"), "Current");
                    var to = PreviewRenderer.ParseAnimationValue(PreviewRenderer.Attribute(track, "to"), "ToggleState");
                    if (!int.TryParse(PreviewRenderer.Attribute(track, "duration"), out var duration)
                        || duration < 0) {
                        throw new InvalidDataException("Animation track requires non-negative duration.");
                    }

                    var easing = PreviewRenderer.Attribute(track, "easing") switch {
                        null or "CubicOut" => 1,
                        "Linear" => 0,
                        _ => throw new InvalidDataException("Animation easing must be Linear or CubicOut."),
                    };
                    NativeRuntime.Ensure(NativeRuntime.xr_add_storyboard_track(
                        element, trigger, property, from, to, duration, easing) != 0);
                }
            }
        }
    }

    private static int ParseTrigger(string? value) {
        return value switch {
            "PointerDown" => 0,
            "PointerUp" => 1,
            "Toggled" => 2,
            "Show" => 3,
            "Hide" => 4,
            "ParentShow" => 5,
            "ParentHide" => 6,
            _ => throw new InvalidDataException("Storyboard trigger must be PointerDown, PointerUp, Toggled, Show, Hide, ParentShow or ParentHide."),
        };
    }

    private static int ParseProperty(string? value) {
        return value switch {
            "opacity" => 0,
            "renderOffsetX" => 1,
            "renderOffsetY" => 2,
            "height" => 3,
            "toggleProgress" => 4,
            "pressProgress" => 5,
            _ => throw new InvalidDataException("Unsupported FloatAnimation property."),
        };
    }

    private static float ParseAnimationValue(string? value, string stateValue) {
        return value == stateValue
            ? float.NaN
            : float.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out var result)
                ? result
                : throw new InvalidDataException("Animation value must be a number or supported state value.");
    }

    private static void AddChild(IntPtr parent, IntPtr child) {
        if (NativeRuntime.xr_add_child(parent, child) == 0) {
            NativeRuntime.xr_destroy_element(child);
            throw new InvalidOperationException(NativeRuntime.GetLastError());
        }
    }

    private static void SetAttribute(IntPtr element, string name, string value) {
        if (NativeRuntime.xr_set_attribute(element, name, value) == 0) {
            throw new InvalidOperationException($"{name}: {NativeRuntime.GetLastError()}");
        }
    }

    private static void ApplyVisualStateGroups(IntPtr element, XElement node) {
        var collection = node.Elements().SingleOrDefault(child => child.Name.LocalName == "VisualStateManager.VisualStateGroups");
        if (collection is null) {
            return;
        }
        foreach (var group in collection.Elements()) {
            var groupName = PreviewRenderer.Attribute(group, "name");
            if (group.Name.LocalName != "VisualStateGroup" || string.IsNullOrWhiteSpace(groupName)) {
                throw new InvalidDataException("VisualStateManager.VisualStateGroups требует именованные VisualStateGroup.");
            }
            foreach (var state in group.Elements()) {
                var stateName = PreviewRenderer.Attribute(state, "name");
                var storyboard = state.Elements().SingleOrDefault();
                if (state.Name.LocalName != "VisualState" || string.IsNullOrWhiteSpace(stateName)
                    || storyboard?.Name.LocalName != "Storyboard") {
                    throw new InvalidDataException("VisualState требует name и один Storyboard.");
                }
                foreach (var track in storyboard.Elements()) {
                    var targetName = PreviewRenderer.Attribute(track, "targetName");
                    if (track.Name.LocalName != "FloatAnimation" || string.IsNullOrWhiteSpace(targetName)) {
                        throw new InvalidDataException("VisualState поддерживает FloatAnimation с targetName.");
                    }
                    var property = PreviewRenderer.ParseProperty(PreviewRenderer.Attribute(track, "property"));
                    var from = PreviewRenderer.ParseAnimationValue(PreviewRenderer.Attribute(track, "from"), "Current");
                    var to = PreviewRenderer.ParseAnimationValue(PreviewRenderer.Attribute(track, "to"), "ToggleState");
                    if (!int.TryParse(PreviewRenderer.Attribute(track, "duration"), out var duration) || duration < 0) {
                        throw new InvalidDataException("Visual state animation requires non-negative duration.");
                    }
                    var easing = PreviewRenderer.Attribute(track, "easing") switch {
                        null or "CubicOut" => 1,
                        "Linear" => 0,
                        _ => throw new InvalidDataException("Animation easing must be Linear or CubicOut."),
                    };
                    NativeRuntime.Ensure(NativeRuntime.xr_add_visual_state_track(element, groupName, stateName,
                        targetName, property, from, to, duration, easing) != 0);
                }
            }
        }
    }

    private static void ApplyInitialVisualStates(IntPtr root, JsonElement data) {
        if (!data.TryGetProperty("$visualStates", out var states) || states.ValueKind != JsonValueKind.Array) {
            return;
        }
        foreach (var item in states.EnumerateArray()) {
            if (!item.TryGetProperty("host", out var hostValue)
                || !item.TryGetProperty("group", out var groupValue)
                || !item.TryGetProperty("state", out var stateValue)
                || hostValue.GetString() is not string hostId
                || groupValue.GetString() is not string groupName
                || stateValue.GetString() is not string stateName) {
                throw new InvalidDataException("$visualStates требует host, group и state.");
            }
            var host = NativeRuntime.xr_find_element(root, hostId);
            if (host == IntPtr.Zero) {
                throw new InvalidDataException($"Visual state host не найден: {hostId}.");
            }
            NativeRuntime.Ensure(NativeRuntime.xr_go_to_visual_state(host, groupName, stateName, 0) != 0);
        }
    }

    private static void ApplyAttribute(IntPtr element, string name, string value, JsonElement data) {
        // XamlRuntime регистрирует command по исходной binding-строке; её нельзя
        // подменять результатом поиска в сценарии, как остальные свойства UI.
        var resolved = name == "command" ? value : PreviewRenderer.Resolve(value, data);
        if (name == "visibility" && bool.TryParse(resolved, out var isVisible)) {
            resolved = isVisible ? "Visible" : "Collapsed";
        }
        PreviewRenderer.SetAttribute(
            element,
            name,
            resolved);
    }

    private static string Resolve(string value, JsonElement data) {
        var resolved = PreviewRenderer.ResolveElement(value, data);
        return resolved switch {
            null => string.Empty,
            bool boolean => boolean ? "True" : "False",
            JsonElement element => element.ToString(),
            _ => Convert.ToString(resolved, CultureInfo.InvariantCulture) ?? string.Empty
        };
    }

    private static object? ResolveElement(string? value, JsonElement data) {
        if (value is null || !value.StartsWith("{Binding ", StringComparison.Ordinal)
            || !value.EndsWith('}')) {
            return value;
        }

        var path = value[9..^1].Split(',', 2)[0].Trim();
        var current = data;
        foreach (var segment in path.Split('.')) {
            if (current.ValueKind != JsonValueKind.Object || !current.TryGetProperty(segment, out current)) {
                return $"{{{path}?}}";
            }
        }

        return current.ValueKind switch {
            JsonValueKind.String => current.GetString(),
            JsonValueKind.True => true,
            JsonValueKind.False => false,
            JsonValueKind.Number when current.TryGetInt64(out var integer) => integer,
            JsonValueKind.Number => current.GetDouble(),
            JsonValueKind.Null => null,
            _ => current
        };
    }

    private static bool IsVisualElement(XElement element) {
        return element.Name.LocalName != "columnDefinitions"
            && element.Name.LocalName != "rowDefinitions"
            && element.Name.LocalName != "Page.Resources"
            && !element.Name.LocalName.Contains('.');
    }

    private static string? Attribute(XElement element, string name) {
        return element.Attributes().FirstOrDefault(attribute => string.Equals(
            attribute.Name.LocalName,
            name,
            StringComparison.OrdinalIgnoreCase))?.Value;
    }
}
using System.IO;
using System.Text.Encodings.Web;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace XamlPreviewer;

internal sealed class DevicePreset {
    public required string Name { get; init; }
    public required int Width { get; init; }
    public required int Height { get; init; }

    public override string ToString() {
        return $"{this.Name} · {this.Width}×{this.Height}";
    }
}

internal sealed class PreviewerSettings {
    private const string DefaultResourcesDirectory = @"C:\WORK\Android\Projects\MobileClock\MobileClock.Application\Resources";
    private const string DefaultXamlDirectory = @"C:\WORK\Android\Projects\MobileClock\MobileClock.Application\UI";
    private static readonly JsonSerializerOptions JsonOptions = new() {
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
        WriteIndented = true
    };

    public required string XamlDirectory { get; set; }
    public required string ResourcesDirectory { get; init; }
    public string? LastMarkupPath { get; set; }
    public Dictionary<string, int[]> CollapsedMarkupFoldingOffsets { get; set; } = [];
    public double WindowWidth { get; set; }
    public double WindowHeight { get; set; }
    public bool IsMaximized { get; set; }
    public double EditorPaneWidth { get; set; }
    public double EditorScale { get; set; } = 1.0;
    public int MouseWheelLines { get; set; } = 6;
    public double MouseWheelAnimationDurationMilliseconds { get; set; } = 400.0;
    public string MouseWheelSmoothingMode { get; set; } = "Exponential";
    public int PreviewWidth { get; set; } = 720;
    public int PreviewHeight { get; set; } = 1600;
    public double PreviewScale { get; set; }
    public double PreviewHorizontalOffset { get; set; }
    public double PreviewVerticalOffset { get; set; }
    public bool IsPreviewLandscape { get; set; }
    public double AnimationPlaybackRate { get; set; } = 1.0;
    public double[] AnimationPlaybackRates { get; set; } = [0.1, 0.25, 0.5, 1.0, 2.0, 4.0];
    public string ElementInspectionHighlightColor { get; set; } = "#E05252";
    public double ElementInspectionHighlightThickness { get; set; } = 3.0;
    public string ElementInspectionHighlightLineStyle { get; set; } = "solid";
    public DevicePreset[] PreviewResolutions { get; set; } = [
        new() { Name = "Redmi 15C", Width = 720, Height = 1600 },
        new() { Name = "HD+", Width = 720, Height = 1280 },
        new() { Name = "FHD+", Width = 1080, Height = 2400 },
        new() { Name = "QHD+", Width = 1440, Height = 3200 },
    ];

    [JsonIgnore]
    public string FilePath { get; private set; } = string.Empty;

    public static PreviewerSettings LoadDebug() {
        var settingsPath = Path.Combine(AppContext.BaseDirectory, "previewer.settings.json");
        PreviewerSettings settings;
        if (File.Exists(settingsPath)) {
            settings = JsonSerializer.Deserialize<PreviewerSettings>(File.ReadAllText(settingsPath))
                ?? CreateDefaults(settingsPath);
            settings.FilePath = settingsPath;
        } else {
            settings = CreateDefaults(settingsPath);
            settings.Save();
        }
        settings.CollapsedMarkupFoldingOffsets ??= [];
        settings.ValidateAnimationSpeeds();
        settings.ValidateResolutions();
        settings.ValidateElementInspectionHighlight();
        return settings;
    }

    public static PreviewerSettings Parse(string json, string filePath) {
        var settings = JsonSerializer.Deserialize<PreviewerSettings>(json)
            ?? throw new InvalidDataException("Настройки не содержат объект.");
        settings.FilePath = filePath;
        settings.CollapsedMarkupFoldingOffsets ??= [];
        settings.ValidateAnimationSpeeds();
        settings.ValidateResolutions();
        settings.ValidateElementInspectionHighlight();
        return settings;
    }

    private void ValidateResolutions() {
        if (this.PreviewResolutions is null || this.PreviewResolutions.Length == 0
            || this.PreviewResolutions.Any(preset => preset is null || string.IsNullOrWhiteSpace(preset.Name)
                || preset.Width <= 0 || preset.Height <= 0)) {
            throw new InvalidDataException("PreviewResolutions должен содержать названия и положительные размеры экранов.");
        }
    }

    private void ValidateAnimationSpeeds() {
        if (this.AnimationPlaybackRates is null || this.AnimationPlaybackRates.Length == 0
            || this.AnimationPlaybackRates.Any(rate => !float.IsFinite((float)rate) || (float)rate <= 0)) {
            throw new InvalidDataException("AnimationPlaybackRates должен содержать положительные конечные скорости.");
        }
        this.AnimationPlaybackRates = this.AnimationPlaybackRates.Distinct().ToArray();
        if (!this.AnimationPlaybackRates.Contains(this.AnimationPlaybackRate)) {
            this.AnimationPlaybackRate = this.AnimationPlaybackRates[0];
        }
    }

    private void ValidateElementInspectionHighlight() {
        if (string.IsNullOrWhiteSpace(this.ElementInspectionHighlightColor)) {
            throw new InvalidDataException("ElementInspectionHighlightColor должен содержать цвет.");
        }
        try {
            _ = PreviewBrushes.Parse(this.ElementInspectionHighlightColor);
        }
        catch (Exception exception) {
            throw new InvalidDataException("ElementInspectionHighlightColor содержит некорректный цвет.", exception);
        }
        if (!double.IsFinite(this.ElementInspectionHighlightThickness)
            || this.ElementInspectionHighlightThickness <= 0.0) {
            throw new InvalidDataException("ElementInspectionHighlightThickness должен быть положительным конечным числом.");
        }
        if (this.ElementInspectionHighlightLineStyle is not "solid" and not "dashed") {
            throw new InvalidDataException("ElementInspectionHighlightLineStyle должен быть solid или dashed.");
        }
    }

    public void Save() {
        File.WriteAllText(this.FilePath, this.ToJson());
    }

    public string ToJson() {
        return JsonSerializer.Serialize(this, JsonOptions).TrimEnd();
    }

    private static PreviewerSettings CreateDefaults(string settingsPath) {
        return new PreviewerSettings {
            FilePath = settingsPath,
            XamlDirectory = DefaultXamlDirectory,
            ResourcesDirectory = DefaultResourcesDirectory,
        };
    }

}
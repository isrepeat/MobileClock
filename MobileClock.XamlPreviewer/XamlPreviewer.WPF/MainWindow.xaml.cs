using ICSharpCode.AvalonEdit;
using ICSharpCode.AvalonEdit.Highlighting;
using ICSharpCode.AvalonEdit.Search;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;
using PreviewRenderer = XamlPreviewer.PreviewBrushes;

namespace XamlPreviewer;

/// <summary>
/// Координирует UI превьювера: режимы AvalonEdit, сохранённое состояние сессии,
/// наблюдение за внешними файлами и жизненный цикл нативной сессии рендеринга.
/// WPF редактирует файлы и показывает кадры; дерево, ввод и отрисовка принадлежат native ApplicationSession.
/// </summary>
public partial class MainWindow : Window {
    private const string NativeBridgeLibraryName = "XamlPreviewer.NativeBridge.dll";
    private const double SearchPanelOverlayHeight = 84.0;
    private readonly DispatcherTimer renderTimer;
    private readonly DispatcherTimer animationTimer;
    private readonly PreviewFileWatchController fileWatchController;
    private readonly EditorScrollController editorScrollController;
    private readonly PreviewStatusPresenter statusPresenter;
    private readonly MarkupEditorController markupEditorController;
    private readonly SearchPanel markupSearchPanel;
    private readonly XamlCompletionController xamlCompletionController;
    private readonly FolderPickerController folderPickerController;
    private readonly PreviewViewportController previewViewportController;
    private readonly Grid previewLayer = new();
    private bool updatingPreviewControls;
    private bool settingsPersistenceReady;
    private MobileClockSession? nativeApplicationSession;
    private bool isClosing;
    private PreviewerSettings settings = null!;
    private string? markupPath;
    private bool isMarkupDirty;
    private bool isSettingsDirty;
    private bool isScenarioDirty;
    private bool updatingEditors;
    private bool suppressFoldingStatePersistence;
    private EditorMode editorMode;
    private string? scenarioPath;
    private string? scenarioFileText;

    private enum EditorMode {
        Xaml,
        Scenario,
        Settings,
    }

    private sealed class AnimationSpeed {
        public required string Name { get; init; }
        public required double Rate { get; init; }

        public override string ToString() {
            return this.Name;
        }
    }

    public MainWindow() {
        InitializeComponent();
        this.statusPresenter = new PreviewStatusPresenter(this.StatusText);
        this.DeviceSurface.Child = this.previewLayer;
        WindowTheme.EnableDarkTitleBar(this);
        this.markupSearchPanel = MainWindow.ConfigureEditor(this.MarkupEditor, MarkupSyntaxHighlighter.Create());
        MainWindow.ConfigureEditor(this.SettingsEditor, MarkupSyntaxHighlighter.CreateJson());
        MainWindow.ConfigureEditor(this.ScenarioEditor, MarkupSyntaxHighlighter.CreateJson());
        this.markupSearchPanel.IsVisibleChanged += this.MarkupSearchPanelLayoutChanged;
        this.markupEditorController = new MarkupEditorController(this.MarkupEditor);
        this.markupEditorController.FoldingStateChanged += this.MarkupEditorFoldingStateChanged;
        this.xamlCompletionController = new XamlCompletionController(this.MarkupEditor);
        this.folderPickerController = new FolderPickerController(
            this.FolderPickerPanel,
            this.FolderPickerPathText,
            this.FolderPickerErrorText,
            this.FolderPickerEntries,
            this.SelectFolderButton,
            this.FolderPickerBackButton,
            this.FolderPickerForwardButton,
            this.ShowFolderPickerPreview,
            this.ClearFolderPickerPreview,
            this.ReportFolderPickerStatus);
        this.previewViewportController = new PreviewViewportController(
            this.PreviewViewport,
            this.GetPreviewScale,
            this.SetPreviewScale,
            () => { this.SyncSettingsEditor(); this.PersistSettings(); });
        this.renderTimer = new DispatcherTimer {
            Interval = TimeSpan.FromMilliseconds(250)
        };
        this.renderTimer.Tick += this.RenderTimerTick;
        this.animationTimer = new DispatcherTimer {
            Interval = TimeSpan.FromMilliseconds(16)
        };
        this.animationTimer.Tick += this.AnimationTimerTick;
        this.fileWatchController = new PreviewFileWatchController(this.Dispatcher, this.ExternalRefresh);
        this.editorScrollController = new EditorScrollController(
            () => this.settings,
            steps => this.SetEditorScale(this.GetEditorScale() + 0.1 * steps));
        this.Loaded += this.WindowLoaded;
        this.Closing += this.WindowClosing;
        this.SizeChanged += this.WindowSizeChanged;
        this.StateChanged += this.WindowStateChanged;
        this.PreviewKeyDown += this.WindowPreviewKeyDown;
        this.PreviewKeyUp += this.WindowPreviewKeyUp;
    }

    private void WindowLoaded(object sender, RoutedEventArgs eventArgs) {
        this.UpdateNativeBridgeTitle();
        NativeRuntime.xr_configure_logging(Path.Combine(AppContext.BaseDirectory, "xaml-previewer.log"));
        this.LoadSettings();
        this.ConfigureMouseWheelScrolling();
        this.ApplyEditorScale();
        this.InitializePreviewControls();
        this.RestoreWindowState();
        this.ConfigureWatchers();
        this.RefreshPageNames();
        var lastMarkupPath = this.settings.LastMarkupPath;
        if (lastMarkupPath is not null && File.Exists(lastMarkupPath)) {
            var lastPageName = Path.GetRelativePath(this.settings.XamlDirectory, lastMarkupPath);
            if (this.PagePicker.Items.Contains(lastPageName)) {
                this.PagePicker.SelectedItem = lastPageName;
            } else {
                this.LoadMarkup(lastMarkupPath);
            }
        } else {
            var defaultMarkupPath = Path.Combine(this.settings.XamlDirectory, "Pages", "MainPage.xaml");
            if (File.Exists(defaultMarkupPath)) {
                this.LoadMarkup(defaultMarkupPath);
            }
        }

        this.updatingEditors = true;
        this.SettingsEditor.Text = this.settings.ToJson();
        this.updatingEditors = false;
        this.isMarkupDirty = false;
        this.isSettingsDirty = false;
        this.UpdateEditorMode();
        this.settingsPersistenceReady = true;
        this.PersistSettings();
        this.UpdateDocumentState();
        this.ScheduleRender();
        if (this.settings.PreviewScale <= 0.0) {
            this.Dispatcher.BeginInvoke(new Action(this.FitPreview));
        }
    }

    private void UpdateNativeBridgeTitle() {
        var loadedLibraryPath = Path.Combine(AppContext.BaseDirectory, NativeBridgeLibraryName);
        var loadedLibrary = new FileInfo(loadedLibraryPath);
        if (!loadedLibrary.Exists) {
            this.Title = "MobileClock XAML Previewer (DLL не найдена)";
            WindowTheme.SetTitleBarWarning(this, true);
            return;
        }

        var expectedLibraryPath = MainWindow.GetExpectedNativeBridgePath();
        var isCurrent = expectedLibraryPath is not null
            && File.Exists(expectedLibraryPath)
            && MainWindow.FilesAreEqual(loadedLibraryPath, expectedLibraryPath);
        this.Title = $"MobileClock XAML Previewer (DLL: {loadedLibrary.LastWriteTime:yyyy-MM-dd HH:mm:ss}{(isCurrent ? string.Empty : " — СТАРАЯ")})";
        WindowTheme.SetTitleBarWarning(this, !isCurrent);
    }

    private static string? GetExpectedNativeBridgePath() {
        var outputDirectory = new DirectoryInfo(AppContext.BaseDirectory);
        var configurationDirectory = outputDirectory.Parent?.Parent;
        if (configurationDirectory?.Parent?.Name != "Build") {
            return null;
        }

        return Path.Combine(
            configurationDirectory.FullName,
            "x64",
            "XamlPreviewer.NativeBridge",
            NativeBridgeLibraryName);
    }

    private static bool FilesAreEqual(string firstPath, string secondPath) {
        var first = new FileInfo(firstPath);
        var second = new FileInfo(secondPath);
        return first.Length == second.Length
            && CryptographicOperations.FixedTimeEquals(
                SHA256.HashData(File.ReadAllBytes(firstPath)),
                SHA256.HashData(File.ReadAllBytes(secondPath)));
    }

    private void OpenButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.folderPickerController.Open(this.settings.XamlDirectory);
        this.MarkupEditor.Visibility = Visibility.Collapsed;
        this.SettingsPanel.Visibility = Visibility.Collapsed;
        this.OpenButton.IsEnabled = false;
        this.statusPresenter.Information("Выберите папку, содержащую XAML-файлы.");
    }

    private void FolderPickerUpButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.folderPickerController.Up();
    }

    private void FolderPickerBackButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.folderPickerController.Back();
    }

    private void FolderPickerForwardButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.folderPickerController.Forward();
    }

    private void FolderPickerPathTextKeyDown(object sender, KeyEventArgs eventArgs) {
        if (eventArgs.Key != Key.Enter) {
            return;
        }
        this.folderPickerController.SubmitPath();
        eventArgs.Handled = true;
    }

    private void FolderPickerEntriesPreviewKeyDown(object sender, KeyEventArgs eventArgs) {
        this.folderPickerController.HandleListKey(eventArgs);
    }

    private void FolderPickerEntriesPreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        this.folderPickerController.HandleListBackgroundMouseDown(eventArgs.OriginalSource as DependencyObject);
    }

    private void FolderPickerEntriesMouseDoubleClick(object sender, MouseButtonEventArgs eventArgs) {
        this.folderPickerController.HandleListDoubleClick();
    }

    private void FolderPickerEntriesSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        this.folderPickerController.HandleSelectionChanged();
    }

    private void CancelFolderPickerButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.HideFolderPicker();
    }

    private void SelectFolderButtonClick(object sender, RoutedEventArgs eventArgs) {
        if (!this.folderPickerController.TrySelectDirectory(out var selectedDirectory)) {
            return;
        }
        this.settings.XamlDirectory = selectedDirectory;
        this.ConfigureWatchers();
        this.RefreshPageNames();
        this.HideFolderPicker();
        this.PersistSettings();
        this.statusPresenter.Success($"Выбрана папка XAML: {this.settings.XamlDirectory}");
    }

    private void ShowFolderPickerPreview(string path) {
        this.ShowNativeApplicationPreview();
    }

    private void ClearFolderPickerPreview() {
        this.animationTimer.Stop();
        this.nativeApplicationSession?.Dispose();
        this.nativeApplicationSession = null;
        this.previewLayer.Children.Clear();
    }

    private void ShowPreviewError(Exception exception) {
        this.ClearFolderPickerPreview();
        this.previewLayer.Children.Add(new Border {
            Background = PreviewBrushes.Parse("#1F1717"),
            BorderBrush = PreviewBrushes.Parse("#A75B5B"),
            BorderThickness = new Thickness(1),
            Child = new TextBlock {
                Margin = new Thickness(32),
                Foreground = PreviewBrushes.Parse("#FFB4AB"),
                FontSize = 18,
                Text = $"Ошибка предпросмотра\n\n{exception.Message}",
                TextWrapping = TextWrapping.Wrap,
                VerticalAlignment = VerticalAlignment.Center,
                HorizontalAlignment = HorizontalAlignment.Center,
                TextAlignment = TextAlignment.Center,
            },
        });
        this.statusPresenter.Error(exception.Message);
    }

    private void HideFolderPicker() {
        this.folderPickerController.Close();
        this.UpdateEditorMode();
        this.ScheduleRender();
    }

    private void ReportFolderPickerStatus(string message, bool isSuccess) {
        if (isSuccess) {
            this.statusPresenter.Success(message);
        } else {
            this.statusPresenter.Information(message);
        }
    }

    private void SaveButtonClick(object sender, RoutedEventArgs eventArgs) {
        if (this.editorMode == EditorMode.Settings) {
            this.settings = PreviewerSettings.Parse(this.SettingsEditor.Text, this.settings.FilePath);
            this.ConfigureMouseWheelScrolling();
            this.ApplyEditorScale();
            this.ApplySettingsToPreviewControls();
            this.SaveSettings();
            this.RefreshPageNames();
            this.updatingEditors = true;
            this.SettingsEditor.Text = this.settings.ToJson();
            this.updatingEditors = false;
            this.isSettingsDirty = false;
            this.UpdateDocumentState();
            this.statusPresenter.Success($"Настройки сохранены: {this.settings.FilePath}");
            return;
        }
        if (this.editorMode == EditorMode.Scenario) {
            if (this.scenarioPath is null) {
                return;
            }
            try {
                using var document = JsonDocument.Parse(this.ScenarioEditor.Text);
                if (document.RootElement.ValueKind != JsonValueKind.Object) {
                    throw new JsonException("Корневой элемент должен быть объектом.");
                }
                File.WriteAllText(this.scenarioPath, this.ScenarioEditor.Text.TrimEnd());
                this.isScenarioDirty = false;
                this.RefreshScenarioNames();
                if (this.scenarioPath is not null) {
                    this.editorMode = EditorMode.Scenario;
                    this.ScenarioButton.IsChecked = true;
                    this.UpdateEditorMode();
                }
                this.ShowNativeApplicationPreview();
                this.statusPresenter.Success($"Сценарии сохранены: {this.scenarioPath}");
            }
            catch (JsonException exception) {
                this.statusPresenter.Error($"Сценарии не сохранены: {exception.Message}");
            }
            this.UpdateDocumentState();
            return;
        }
        if (this.markupPath is null) {
            return;
        }

        File.WriteAllText(this.markupPath, this.markupEditorController.Text.TrimEnd());
        this.isMarkupDirty = false;
        this.RefreshScenarioNames();
        this.UpdateDocumentState();
        this.statusPresenter.Success($"Сохранено: {this.markupPath}");
    }

    private void DevicePresetPickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        if (this.updatingPreviewControls || this.DevicePresetPicker.SelectedItem is not DevicePreset preset) {
            return;
        }

        this.settings.PreviewWidth = preset.Width;
        this.settings.PreviewHeight = preset.Height;
        this.ApplyPreviewLayout();
        this.SyncSettingsEditor();
        this.PersistSettings();
        this.ScheduleRender();
    }

    private void ResetSessionButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.animationTimer.Stop();
        this.nativeApplicationSession?.Dispose();
        this.nativeApplicationSession = null;
        this.previewLayer.Children.Clear();
        this.ShowNativeApplicationPreview();
    }

    private void AnimationSpeedPickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        if (this.updatingPreviewControls || this.AnimationSpeedPicker.SelectedItem is not AnimationSpeed speed) {
            return;
        }

        this.settings.AnimationPlaybackRate = speed.Rate;
        this.nativeApplicationSession?.SetAnimationPlaybackRate(speed.Rate);
        this.SyncSettingsEditor();
        this.PersistSettings();
    }

    private void PreviewOrientationToggleClick(object sender, RoutedEventArgs eventArgs) {
        if (this.updatingPreviewControls) {
            return;
        }

        this.settings.IsPreviewLandscape = this.PreviewOrientationToggle.IsChecked == true;
        this.UpdatePreviewOrientationToggle();
        this.ApplyPreviewLayout();
        this.SyncSettingsEditor();
        this.PersistSettings();
        this.ScheduleRender();
    }

    private void ZoomOutButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.SetPreviewScale(this.GetPreviewScale() - 0.1);
    }

    private void ZoomInButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.SetPreviewScale(this.GetPreviewScale() + 0.1);
    }

    private void PreviewViewportPreviewMouseWheel(object sender, MouseWheelEventArgs eventArgs) {
        this.previewViewportController.HandleMouseWheel(eventArgs);
    }

    private void PreviewViewportPreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        this.previewViewportController.HandleMouseDown(eventArgs);
    }

    private void PreviewViewportPreviewMouseMove(object sender, MouseEventArgs eventArgs) {
        this.previewViewportController.HandleMouseMove(eventArgs);
    }

    private void PreviewViewportPreviewMouseLeftButtonUp(object sender, MouseButtonEventArgs eventArgs) {
        this.previewViewportController.HandleMouseUp(eventArgs);
    }

    private void PreviewViewportLostMouseCapture(object sender, MouseEventArgs eventArgs) {
        this.previewViewportController.HandleLostMouseCapture();
    }

    private void WindowPreviewKeyDown(object sender, KeyEventArgs eventArgs) {
    }

    private void WindowPreviewKeyUp(object sender, KeyEventArgs eventArgs) {
        this.previewViewportController.HandleKeyUp(eventArgs);
    }

    private void ExpandAllFoldingsButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.markupEditorController.ExpandAll();
    }

    private void MarkupEditorFoldingStateChanged(object? sender, EventArgs eventArgs) {
        this.UpdateExpandAllFoldingsButtonLayout();
        this.ExpandAllFoldingsButton.Visibility = this.markupEditorController.HasFoldedSections
            ? Visibility.Visible
            : Visibility.Collapsed;
        if (!this.suppressFoldingStatePersistence) {
            this.StoreCollapsedMarkupFoldings();
            this.PersistSettings();
        }
    }

    private void MarkupSearchPanelLayoutChanged(
        object sender,
        DependencyPropertyChangedEventArgs eventArgs) {
        this.UpdateExpandAllFoldingsButtonLayout();
    }

    private void UpdateExpandAllFoldingsButtonLayout() {
        var searchPanelHeight = this.markupSearchPanel.IsVisible
            ? MainWindow.SearchPanelOverlayHeight
            : 0.0;
        this.ExpandAllFoldingsButton.Margin = new Thickness(0.0, 16.0 + searchPanelHeight, 38.0, 0.0);
    }

    private void FitPreviewButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.FitPreview();
    }

    private void EditorZoomOutButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.SetEditorScale(this.GetEditorScale() - 0.1);
    }

    private void EditorZoomInButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.SetEditorScale(this.GetEditorScale() + 0.1);
    }

    private void SettingsButtonClick(object sender, RoutedEventArgs eventArgs) {
        if (this.SettingsButton.IsChecked == true) {
            this.ScenarioButton.IsChecked = false;
            this.editorMode = EditorMode.Settings;
            if (!this.isSettingsDirty) {
                this.SyncSettingsEditor();
            }
        } else {
            this.editorMode = EditorMode.Xaml;
        }
        this.UpdateEditorMode();
    }

    private void ScenarioButtonClick(object sender, RoutedEventArgs eventArgs) {
        if (this.ScenarioButton.IsChecked == true && this.scenarioPath is not null) {
            this.SettingsButton.IsChecked = false;
            this.editorMode = EditorMode.Scenario;
            if (!this.isScenarioDirty) {
                this.updatingEditors = true;
                this.ScenarioEditor.Text = File.ReadAllText(this.scenarioPath);
                this.updatingEditors = false;
            }
        } else {
            this.editorMode = EditorMode.Xaml;
        }
        this.UpdateEditorMode();
    }

    private void PagePickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        if (this.PagePicker.SelectedItem is string pageName) {
            this.LoadMarkup(Path.Combine(this.settings.XamlDirectory, pageName));
        }

        this.ShowNativeApplicationPreview();
    }

    private void ScenarioPickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        this.ShowNativeApplicationPreview();
    }

    private void EditorTextChanged(object sender, EventArgs eventArgs) {
        if (ReferenceEquals(sender, this.MarkupEditor)) {
            if (this.markupEditorController.HandleTextChanged()) {
                this.isMarkupDirty = true;
                this.UpdateDocumentState();
            }

            return;
        }

        if (!this.updatingEditors) {
            if (ReferenceEquals(sender, this.SettingsEditor)) {
                this.isSettingsDirty = true;
            }
            if (ReferenceEquals(sender, this.ScenarioEditor)) {
                this.isScenarioDirty = true;
            }

            this.UpdateDocumentState();
        }
    }

    private void EditorPreviewKeyDown(object sender, KeyEventArgs eventArgs) {
        if (ReferenceEquals(sender, this.SettingsEditor)
            && MainWindow.IsPasteGesture(eventArgs)
            && Clipboard.ContainsText()) {
            var pastedText = Clipboard.GetText();
            var jsonText = MainWindow.EscapeWindowsPathForJson(pastedText);
            if (!string.Equals(pastedText, jsonText, StringComparison.Ordinal)) {
                this.SettingsEditor.SelectedText = jsonText;
                eventArgs.Handled = true;
                return;
            }
        }

        if (eventArgs.Key == Key.S && Keyboard.Modifiers == ModifierKeys.Control) {
            this.SaveButtonClick(this, eventArgs);
            eventArgs.Handled = true;
            return;
        }

        if (!ReferenceEquals(sender, this.MarkupEditor)) {
            return;
        }

        if (this.xamlCompletionController.HandlePreviewKeyDown(eventArgs)) {
            return;
        }
        this.markupEditorController.HandlePreviewKeyDown(eventArgs);
    }

    private static bool IsPasteGesture(KeyEventArgs eventArgs) {
        return eventArgs.Key == Key.V && Keyboard.Modifiers == ModifierKeys.Control
            || eventArgs.Key == Key.Insert && Keyboard.Modifiers == ModifierKeys.Shift;
    }

    private static string EscapeWindowsPathForJson(string value) {
        var candidate = value.Trim();
        if (candidate.Length >= 2 && candidate[0] == '"' && candidate[^1] == '"') {
            candidate = candidate[1..^1];
        }
        if (!MainWindow.IsWindowsPath(candidate)) {
            return value;
        }

        var result = new StringBuilder(value.Length * 2);
        for (var index = 0; index < value.Length; index++) {
            var character = value[index];
            if (character != '\\') {
                result.Append(character);
                continue;
            }

            result.Append("\\\\");
            if (index + 1 < value.Length && value[index + 1] == '\\') {
                index++;
            }
        }
        return result.ToString();
    }

    private static bool IsWindowsPath(string value) {
        return value.Length >= 3
            && char.IsAsciiLetter(value[0])
            && value[1] == ':'
            && value[2] == '\\'
            || value.StartsWith("\\\\", StringComparison.Ordinal);
    }

    private void RenderTimerTick(object? sender, EventArgs eventArgs) {
        this.renderTimer.Stop();
        if (this.isClosing) {
            return;
        }
        this.ShowNativeApplicationPreview();
    }

    private void AnimationTimerTick(object? sender, EventArgs eventArgs) {
        if (this.isClosing) {
            return;
        }
        try {
            this.nativeApplicationSession?.UpdateAndRender();
        }
        catch (Exception exception) {
            this.ShowPreviewError(exception);
        }
    }

    private void LoadMarkup(string path) {
        this.StoreCollapsedMarkupFoldings();
        this.markupPath = Path.GetFullPath(path);
        this.ConfigureWatchers();
        this.FilePathText.Text = this.markupPath;
        this.suppressFoldingStatePersistence = true;
        this.updatingEditors = true;
        try {
            this.markupEditorController.SetText(File.ReadAllText(this.markupPath));
            this.markupEditorController.SetFoldedOffsets(this.GetCollapsedMarkupFoldings());
        }
        finally {
            this.updatingEditors = false;
            this.suppressFoldingStatePersistence = false;
        }
        this.isMarkupDirty = false;
        this.RefreshScenarioNames();
        this.ConfigureWatchers();
        this.UpdateDocumentState();
        // PersistSettings записывает previewer.settings.json. Его изменение
        // асинхронно придёт обратно через settingsWatcher, поэтому refresh ниже
        // не должен самовольно выбирать MainPage вместо текущей страницы.
        this.PersistSettings();
        this.ScheduleRender();
    }

    private void RefreshPageNames() {
        // RefreshPageNames вызывается и из settingsWatcher после PersistSettings.
        // Сохраняем выбор, чтобы такой внутренний refresh не отменял навигацию
        // MainPage -> SettingsPage через несколько сотен миллисекунд после tap.
        var previous = this.PagePicker.SelectedItem as string;
        var pages = Directory.Exists(this.settings.XamlDirectory)
            ? Directory.GetFiles(this.settings.XamlDirectory, "*.xaml", SearchOption.AllDirectories)
                .Select(path => Path.GetRelativePath(this.settings.XamlDirectory, path))
                .Where(path => !path.StartsWith("Pages\\backup\\", StringComparison.OrdinalIgnoreCase))
                .Order()
                .ToArray()
            : [];
        if (this.PagePicker.Items.Cast<string>().SequenceEqual(pages)) {
            return;
        }
        this.PagePicker.ItemsSource = pages;
        this.PagePicker.SelectedItem = pages.Contains(previous)
            ? previous
            : pages.Contains("Pages/MainPage.xaml")
                ? "Pages/MainPage.xaml"
                : pages.FirstOrDefault();
    }

    private void LoadSettings() {
        this.settings = PreviewerSettings.LoadDebug();
    }

    private void ConfigureWatchers() {
        this.fileWatchController.Configure(
            this.markupPath,
            this.scenarioPath,
            this.settings.FilePath,
            this.settings.XamlDirectory);
    }

    private void ExternalRefresh() {
        if (this.isClosing) {
            return;
        }
        try {
            bool previewChanged = false;
            this.RefreshPageNames();
            if (this.markupPath is not null && File.Exists(this.markupPath)) {
                var markup = File.ReadAllText(this.markupPath);
                if (!this.isMarkupDirty
                    && !string.Equals(markup, this.markupEditorController.Text, StringComparison.Ordinal)) {
                    this.updatingEditors = true;
                    this.markupEditorController.SetText(markup);
                    this.updatingEditors = false;
                    this.isMarkupDirty = false;
                    this.RefreshScenarioNames();
                    this.ConfigureWatchers();
                    this.UpdateDocumentState();
                    previewChanged = true;
                }
            }
            if (this.scenarioPath is not null
                && (!File.Exists(this.scenarioPath)
                    || !string.Equals(File.ReadAllText(this.scenarioPath), this.scenarioFileText, StringComparison.Ordinal))) {
                this.RefreshScenarioNames();
                this.ConfigureWatchers();
                previewChanged = true;
            }
            if (File.Exists(this.settings.FilePath)) {
                var settingsJson = File.ReadAllText(this.settings.FilePath);
                if (!this.isSettingsDirty
                    && !string.Equals(settingsJson, this.SettingsEditor.Text, StringComparison.Ordinal)) {
                    this.settings = PreviewerSettings.Parse(settingsJson, this.settings.FilePath);
                    this.updatingEditors = true;
                    this.SettingsEditor.Text = settingsJson;
                    this.updatingEditors = false;
                    this.isSettingsDirty = false;
                    this.UpdateDocumentState();
                    this.ConfigureWatchers();
                    // Не сбрасывает PagePicker на MainPage: RefreshPageNames
                    // восстанавливает страницу, выбранную обработчиком navigation.
                    this.RefreshPageNames();
                    this.ConfigureMouseWheelScrolling();
                    this.ApplyEditorScale();
                    this.ApplySettingsToPreviewControls();
                    previewChanged = true;
                }
            }
            if (previewChanged) {
                this.ScheduleRender();
            }
        }
        catch (Exception exception) {
            this.statusPresenter.Error(exception.Message);
        }
    }

    private void SaveSettings() {
        this.StoreCollapsedMarkupFoldings();
        this.settings.LastMarkupPath = this.markupPath;
        if (this.WindowState == WindowState.Normal) {
            this.settings.WindowWidth = this.Width;
            this.settings.WindowHeight = this.Height;
        }
        this.settings.IsMaximized = this.WindowState == WindowState.Maximized;
        this.settings.EditorPaneWidth = this.EditorColumn.ActualWidth;
        this.settings.Save();
        // Синхронизируем отображаемый JSON после собственного сохранения.
        // Тогда settingsWatcher не принимает нашу же запись за внешнее
        // изменение и не запускает повторный рендер native-сессии.
        if (!this.isSettingsDirty) {
            this.updatingEditors = true;
            this.SettingsEditor.Text = this.settings.ToJson();
            this.updatingEditors = false;
        }
    }

    private void PersistSettings() {
        if (this.settingsPersistenceReady) {
            this.SaveSettings();
        }
    }

    private int[] GetCollapsedMarkupFoldings() {
        if (this.markupPath is null
            || !this.settings.CollapsedMarkupFoldingOffsets.TryGetValue(this.markupPath, out var offsets)) {
            return [];
        }
        return offsets;
    }

    private void StoreCollapsedMarkupFoldings() {
        if (this.markupPath is null) {
            return;
        }
        var offsets = this.markupEditorController.GetFoldedOffsets();
        if (offsets.Length == 0) {
            this.settings.CollapsedMarkupFoldingOffsets.Remove(this.markupPath);
            return;
        }
        this.settings.CollapsedMarkupFoldingOffsets[this.markupPath] = offsets;
    }

    private void RestoreWindowState() {
        if (this.settings.WindowWidth >= this.MinWidth) {
            this.Width = this.settings.WindowWidth;
        }
        if (this.settings.WindowHeight >= this.MinHeight) {
            this.Height = this.settings.WindowHeight;
        }
        if (this.settings.IsMaximized) {
            this.WindowState = WindowState.Maximized;
        } else {
            // Положение из прошлой сессии не восстанавливаем: окно должно
            // открываться в центре рабочей области текущего экрана.
            var workArea = SystemParameters.WorkArea;
            this.Left = workArea.Left + (workArea.Width - this.Width) / 2.0;
            this.Top = workArea.Top + (workArea.Height - this.Height) / 2.0;
        }
        if (this.settings.EditorPaneWidth > 0.0) {
            this.EditorColumn.Width = new GridLength(this.settings.EditorPaneWidth, GridUnitType.Pixel);
        }
    }

    private void WindowClosing(object? sender, System.ComponentModel.CancelEventArgs eventArgs) {
        if (this.isClosing) {
            return;
        }
        this.isClosing = true;
        this.settingsPersistenceReady = false;
        this.renderTimer.Stop();
        this.animationTimer.Stop();
        this.fileWatchController.Dispose();
        this.editorScrollController.Dispose();
        this.markupEditorController.Dispose();
        this.nativeApplicationSession?.Dispose();
        this.nativeApplicationSession = null;
        this.previewLayer.Children.Clear();
    }

    private void WindowSizeChanged(object sender, SizeChangedEventArgs eventArgs) {
        this.PersistSettings();
    }

    private void WindowStateChanged(object? sender, EventArgs eventArgs) {
        this.PersistSettings();
    }

    private void EditorSplitterDragCompleted(object sender, DragCompletedEventArgs eventArgs) {
        this.PersistSettings();
    }

    private void UpdateEditorMode() {
        this.MarkupEditor.Visibility = this.editorMode == EditorMode.Xaml ? Visibility.Visible : Visibility.Collapsed;
        this.ScenarioPanel.Visibility = this.editorMode == EditorMode.Scenario ? Visibility.Visible : Visibility.Collapsed;
        this.SettingsPanel.Visibility = this.editorMode == EditorMode.Settings ? Visibility.Visible : Visibility.Collapsed;
        this.OpenButton.IsEnabled = this.editorMode == EditorMode.Xaml;
        this.UpdateScenarioToggle();
        this.UpdateDocumentState();
    }

    private void UpdateDocumentState() {
        this.SettingsButton.Content = this.isSettingsDirty ? "Настройки *" : "Настройки";
        this.ScenarioButton.ToolTip = this.isScenarioDirty ? "Сценарии изменены" : null;
        this.SaveButton.IsEnabled = this.editorMode switch {
            EditorMode.Xaml => this.isMarkupDirty,
            EditorMode.Scenario => this.isScenarioDirty,
            EditorMode.Settings => this.isSettingsDirty,
            _ => false,
        };
    }

    private void UpdateScenarioToggle() {
        var isScenarioMode = this.editorMode == EditorMode.Scenario;
        this.XamlModeText.Foreground = PreviewBrushes.Parse(isScenarioMode ? "#E6E6E6" : "#D5BD7D");
        this.ScenarioModeText.Foreground = PreviewBrushes.Parse(isScenarioMode ? "#D5BD7D" : "#E6E6E6");
    }

    private void InitializePreviewControls() {
        this.ApplySettingsToPreviewControls();
        this.ApplyPreviewLayout();
    }

    private void ApplySettingsToPreviewControls() {
        this.updatingPreviewControls = true;
        try {
            this.DevicePresetPicker.ItemsSource = this.settings.PreviewResolutions;
            this.DevicePresetPicker.SelectedItem = this.settings.PreviewResolutions.FirstOrDefault(
                preset => preset.Width == this.settings.PreviewWidth
                    && preset.Height == this.settings.PreviewHeight);
            this.PreviewOrientationToggle.IsChecked = this.settings.IsPreviewLandscape;
            var speeds = this.settings.AnimationPlaybackRates.Select(rate => new AnimationSpeed {
                Name = rate.ToString("G", System.Globalization.CultureInfo.InvariantCulture) + "×",
                Rate = rate,
            }).ToArray();
            this.AnimationSpeedPicker.ItemsSource = speeds;
            this.AnimationSpeedPicker.SelectedItem = speeds.FirstOrDefault(
                speed => speed.Rate == this.GetAnimationPlaybackRate());
        }
        finally {
            this.updatingPreviewControls = false;
        }
        this.UpdatePreviewOrientationToggle();
        this.ApplyPreviewLayout();
    }

    private void UpdatePreviewOrientationToggle() {
        this.PortraitOrientationText.Foreground = PreviewBrushes.Parse(
            this.settings.IsPreviewLandscape ? "#E6E6E6" : "#D5BD7D");
        this.LandscapeOrientationText.Foreground = PreviewBrushes.Parse(
            this.settings.IsPreviewLandscape ? "#D5BD7D" : "#E6E6E6");
    }

    private void ApplyPreviewLayout() {
        var previewSize = this.GetPreviewSize();
        var scale = this.GetPreviewScale();
        this.DeviceSurface.Width = previewSize.Width;
        this.DeviceSurface.Height = previewSize.Height;
        this.PreviewViewbox.Width = previewSize.Width * scale;
        this.PreviewViewbox.Height = previewSize.Height * scale;
        this.ZoomText.Text = $"{scale:P0}";
    }

    private double GetAnimationPlaybackRate() {
        return this.settings.AnimationPlaybackRate;
    }

    private void FitPreview() {
        var previewSize = this.GetPreviewSize();
        if (this.PreviewViewport.ViewportWidth <= 0.0 || this.PreviewViewport.ViewportHeight <= 0.0) {
            return;
        }

        var scale = Math.Min(
            this.PreviewViewport.ViewportWidth / previewSize.Width,
            this.PreviewViewport.ViewportHeight / previewSize.Height);
        this.SetPreviewScale(scale);
    }

    private (int Width, int Height) GetPreviewSize() {
        var width = Math.Max(1, this.settings.PreviewWidth);
        var height = Math.Max(1, this.settings.PreviewHeight);
        return this.settings.IsPreviewLandscape
            ? (height, width)
            : (width, height);
    }

    private double GetPreviewScale() {
        return Math.Clamp(
            this.settings.PreviewScale > 0.0 ? this.settings.PreviewScale : 0.5,
            0.1,
            3.0);
    }

    private void SetPreviewScale(double scale) {
        this.settings.PreviewScale = Math.Clamp(scale, 0.1, 3.0);
        this.ApplyPreviewLayout();
        this.SyncSettingsEditor();
        this.PersistSettings();
    }

    private string GetNativeApplicationPageName() {
        var pagePath = this.PagePicker.SelectedItem as string ?? this.markupPath ?? "MainPage.xaml";
        return Path.GetFileNameWithoutExtension(pagePath);
    }

    private void RefreshScenarioNames() {
        var previous = this.ScenarioPicker.SelectedItem as string;
        var wasScenarioMode = this.editorMode == EditorMode.Scenario;
        this.scenarioPath = null;
        this.scenarioFileText = null;
        this.ScenarioPicker.ItemsSource = null;
        this.ScenarioPicker.SelectedItem = null;
        this.ScenarioPicker.Visibility = Visibility.Collapsed;
        this.ScenarioLabel.Visibility = Visibility.Collapsed;
        if (this.markupPath is null) {
            this.ClearScenarioMode();
            return;
        }
        const string pattern = "<\\?mobileclock-preview-scenario\\s+path=\\\"(?<path>[^\\\"]+)\\\"\\s*\\?>";
        var match = Regex.Match(File.ReadAllText(this.markupPath), pattern, RegexOptions.CultureInvariant);
        if (!match.Success) {
            this.ClearScenarioMode();
            return;
        }
        var candidate = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(this.markupPath)!, match.Groups["path"].Value));
        if (!File.Exists(candidate)) {
            this.ClearScenarioMode();
            this.statusPresenter.Information($"Файл сценариев не найден: {candidate}");
            return;
        }
        try {
            var scenarioFileText = File.ReadAllText(candidate);
            using var document = JsonDocument.Parse(scenarioFileText);
            if (document.RootElement.ValueKind != JsonValueKind.Object) {
                throw new JsonException("Корневой элемент должен быть объектом.");
            }
            var names = document.RootElement.EnumerateObject().Select(property => property.Name).ToArray();
            if (names.Length == 0) {
                this.ClearScenarioMode();
                return;
            }
            this.scenarioPath = candidate;
            this.scenarioFileText = scenarioFileText;
            this.ScenarioPicker.ItemsSource = names;
            this.ScenarioPicker.SelectedItem = names.Contains(previous)
                ? previous
                : names[0];
            this.ScenarioPicker.Visibility = Visibility.Visible;
            this.ScenarioLabel.Visibility = Visibility.Visible;
            this.ScenarioButton.Visibility = Visibility.Visible;
            this.ScenarioButton.IsChecked = wasScenarioMode;
        }
        catch (Exception exception) when (exception is IOException || exception is JsonException) {
            this.ClearScenarioMode();
            this.statusPresenter.Information($"Сценарии не загружены: {exception.Message}");
        }
    }

    private void ClearScenarioMode() {
        this.ScenarioButton.Visibility = Visibility.Collapsed;
        this.ScenarioButton.IsChecked = false;
        if (this.editorMode == EditorMode.Scenario) {
            this.editorMode = EditorMode.Xaml;
            this.UpdateEditorMode();
        }
    }

    private string? GetSelectedScenarioJson() {
        if (this.scenarioPath is null || this.ScenarioPicker.SelectedItem is not string name) {
            return null;
        }
        using var document = JsonDocument.Parse(File.ReadAllText(this.scenarioPath));
        return document.RootElement.TryGetProperty(name, out var scenario) ? scenario.GetRawText() : null;
    }

    private void ShowNativeApplicationPreview() {
        if (this.isClosing) {
            return;
        }
        try {
            var previewSize = this.GetPreviewSize();
            if (this.nativeApplicationSession is null
                || this.nativeApplicationSession.Width != previewSize.Width
                || this.nativeApplicationSession.Height != previewSize.Height) {
                this.animationTimer.Stop();
                this.nativeApplicationSession?.Dispose();
                this.nativeApplicationSession = new MobileClockSession(
                    this.settings.ResourcesDirectory,
                    previewSize.Width,
                    previewSize.Height);
                this.nativeApplicationSession.SetAnimationPlaybackRate(this.GetAnimationPlaybackRate());
                this.previewLayer.Children.Clear();
                this.previewLayer.Children.Add(this.nativeApplicationSession.Surface);
            }
            this.nativeApplicationSession.LoadPage(this.GetNativeApplicationPageName());
            var scenario = this.GetSelectedScenarioJson();
            if (scenario is not null) {
                this.nativeApplicationSession.ApplyPreviewScenario(this.GetNativeApplicationPageName(), scenario);
            }
            this.nativeApplicationSession.UpdateAndRender();
            this.animationTimer.Start();
            this.statusPresenter.Success($"Native app: {this.GetNativeApplicationPageName()}");
        }
        catch (Exception exception) {
            this.ShowPreviewError(exception);
        }
    }

    private void PreviewViewportScrollChanged(object sender, ScrollChangedEventArgs eventArgs) {
        if (eventArgs.HorizontalChange == 0.0 && eventArgs.VerticalChange == 0.0) {
            return;
        }
        this.settings.PreviewHorizontalOffset = this.PreviewViewport.HorizontalOffset;
        this.settings.PreviewVerticalOffset = this.PreviewViewport.VerticalOffset;
        this.PersistSettings();
    }

    private void RestorePreviewPosition() {
        this.PreviewViewport.ScrollToHorizontalOffset(this.settings.PreviewHorizontalOffset);
        this.PreviewViewport.ScrollToVerticalOffset(this.settings.PreviewVerticalOffset);
    }

    private void ApplyEditorScale() {
        const double defaultFontSize = 14.0;
        var scale = this.GetEditorScale();
        foreach (var editor in new[] {
            this.MarkupEditor,
            this.ScenarioEditor,
            this.SettingsEditor,
        }) {
            editor.FontSize = defaultFontSize * scale;
        }
        this.markupEditorController.UpdateFoldingMarkerSize();
        this.EditorZoomText.Text = $"{scale:P0}";
    }

    private double GetEditorScale() {
        return Math.Clamp(
            this.settings.EditorScale > 0.0 ? this.settings.EditorScale : 1.0,
            0.5,
            3.0);
    }

    private void SetEditorScale(double scale) {
        this.settings.EditorScale = Math.Clamp(scale, 0.5, 3.0);
        this.ApplyEditorScale();
        this.SyncSettingsEditor();
        this.PersistSettings();
    }

    private void SyncSettingsEditor() {
        if (this.editorMode != EditorMode.Settings) {
            return;
        }

        this.updatingEditors = true;
        this.SettingsEditor.Text = this.settings.ToJson();
        this.updatingEditors = false;
        this.isSettingsDirty = false;
        this.UpdateDocumentState();
    }

    private static SearchPanel ConfigureEditor(TextEditor editor, IHighlightingDefinition highlighting) {
        editor.SyntaxHighlighting = highlighting;
        editor.Options.EnableHyperlinks = false;
        editor.Options.EnableEmailHyperlinks = false;
        editor.TextArea.Caret.CaretBrush = PreviewBrushes.Parse("#F0D78C");
        editor.TextArea.SelectionBrush = PreviewBrushes.Parse("#5A4D26");
        editor.TextArea.SelectionForeground = PreviewBrushes.Parse("#FFFFFF");
        var searchPanel = SearchPanel.Install(editor);
        searchPanel.Background = PreviewBrushes.Parse("#252525");
        searchPanel.BorderBrush = PreviewBrushes.Parse("#4A4A4A");
        searchPanel.Foreground = PreviewBrushes.Parse("#E6E6E6");
        searchPanel.MarkerBrush = PreviewBrushes.Parse("#665A4D26");
        searchPanel.MarkerPen = new Pen(PreviewBrushes.Parse("#D5BD7D"), 1.0);
        var messageField = typeof(SearchPanel).GetField("messageView", BindingFlags.Instance | BindingFlags.NonPublic);
        if (messageField?.GetValue(searchPanel) is ToolTip message) {
            message.Visibility = Visibility.Collapsed;
        }
        return searchPanel;
    }

    private void ConfigureMouseWheelScrolling() {
        this.editorScrollController.Configure(
            this.MarkupEditor,
            this.SettingsEditor);
    }

    private void ScheduleRender() {
        if (this.isClosing) {
            return;
        }
        this.renderTimer.Stop();
        this.renderTimer.Start();
    }
}
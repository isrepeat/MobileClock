using ICSharpCode.AvalonEdit;
using ICSharpCode.AvalonEdit.Highlighting;
using ICSharpCode.AvalonEdit.Search;
using System.Text.Encodings.Web;
using System.IO;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Threading;

namespace XamlPreviewer;

#pragma warning disable CS0162

/// <summary>
/// Координирует UI превьювера: режимы AvalonEdit, сохранённое состояние сессии,
/// наблюдение за внешними файлами и жизненный цикл нативной сессии рендеринга.
/// Разбор разметки и отрисовка остаются в специализированных классах PreviewRenderer и PreviewSession.
/// </summary>
public partial class MainWindow : Window {
    private const string NativeBridgeLibraryName = "XamlPreviewer.NativeBridge.dll";
    private const double SearchPanelOverlayHeight = 84.0;
    private static readonly JsonSerializerOptions ScenarioJsonOptions = new() {
        Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
        WriteIndented = true
    };
    private readonly DispatcherTimer renderTimer;
    private readonly DispatcherTimer animationTimer;
    private readonly DispatcherTimer previewZoomTimer;
    private readonly PreviewFileWatchController fileWatchController;
    private readonly EditorScrollController editorScrollController;
    private readonly PreviewStatusPresenter statusPresenter;
    private readonly MarkupEditorController markupEditorController;
    private readonly SearchPanel markupSearchPanel;
    private readonly XamlCompletionController xamlCompletionController;
    private readonly FolderPickerController folderPickerController;
    private readonly PreviewViewportController previewViewportController;
    private readonly PreviewGestureController previewGestureController;
    private readonly PreviewCursorSet previewCursors;
    private readonly Grid previewLayer = new();
    private bool updatingPreviewControls;
    private bool settingsPersistenceReady;
    private PreviewSession? previewSession;
    private bool isClosing;
    private PreviewSession? outgoingSession;
    private PreviewerSettings settings = null!;
    private string? markupPath;
    private string scenariosPath = string.Empty;
    private bool usesPageSpecificScenarios;
    private (string From, string To, bool Backward)? pendingPageTransition;
    private (int Width, int Height)? markupPreviewResolution;
    private bool shouldRestorePreviewPosition = true;
    private bool isMarkupDirty;
    private bool isScenariosDirty;
    private bool isSettingsDirty;
    private bool updatingEditors;
    private bool suppressFoldingStatePersistence;
    private EditorMode editorMode;
    private EditorMode previousEditorMode = EditorMode.Xaml;
    private bool isPreviewPanning;
    private Point previewPanStart;
    private double previewPanHorizontalOffset;
    private double previewPanVerticalOffset;
    private DateTime previewZoomStartedAt;
    private double previewZoomStartScale;
    private double previewZoomTargetScale;
    private double previewZoomAnchorX;
    private double previewZoomAnchorY;
    private Point previewZoomViewportPoint;

    private sealed class ScenarioSource {
        public required string Path { get; init; }
        public required bool IsPageSpecific { get; init; }
    }

    private enum EditorMode {
        Xaml,
        Scenarios,
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
        MainWindow.ConfigureEditor(this.ScenarioEditor, MarkupSyntaxHighlighter.CreateJson());
        MainWindow.ConfigureEditor(this.SettingsEditor, MarkupSyntaxHighlighter.CreateJson());
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
        this.previewGestureController = new PreviewGestureController(
            this.GetPreviewScenarioRoot,
            this.GetSelectedScenario,
            this.SavePreviewScenarioChanges);
        this.previewCursors = new PreviewCursorSet();
        this.renderTimer = new DispatcherTimer {
            Interval = TimeSpan.FromMilliseconds(250)
        };
        this.renderTimer.Tick += this.RenderTimerTick;
        this.animationTimer = new DispatcherTimer {
            Interval = TimeSpan.FromMilliseconds(16)
        };
        this.animationTimer.Tick += this.AnimationTimerTick;
        this.previewZoomTimer = new DispatcherTimer {
            Interval = TimeSpan.FromMilliseconds(16)
        };
        this.previewZoomTimer.Tick += this.PreviewZoomTimerTick;
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
        this.Deactivated += (_, _) => this.previewSession?.SetElementInspectionEnabled(false);
        this.Activated += (_, _) => this.UpdateElementInspection();
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
        if (this.markupPath is null) {
            this.LoadScenarioForCurrentMarkup();
        }
        this.isMarkupDirty = false;
        this.isScenariosDirty = false;
        this.isSettingsDirty = false;
        this.RefreshScenarioNames();
        if (this.settings.LastScenarioName is not null) {
            this.ScenarioPicker.SelectedItem = this.settings.LastScenarioName;
        }
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
        this.ScenarioPanel.Visibility = Visibility.Collapsed;
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
        try {
            var markup = File.ReadAllText(path);
            var source = this.GetScenarioSource(markup, path);
            using var document = JsonDocument.Parse(File.Exists(source.Path)
                ? File.ReadAllText(source.Path)
                : "{}");
            var pageName = Path.GetFileName(path);
            var scenarios = MainWindow.GetScenarios(document.RootElement, source.IsPageSpecific, pageName);
            var scenarioName = this.ScenarioPicker.SelectedItem as string;
            var data = scenarioName is not null && scenarios.TryGetProperty(scenarioName, out var selected)
                ? selected
                : scenarios;
            var root = PreviewRenderer.CreateRoot(markup, data);
            this.animationTimer.Stop();
            this.CompletePageTransition();
            this.previewSession?.Dispose();
            this.previewSession = null;
            this.previewLayer.Children.Clear();
            var previewSize = this.GetPreviewSize();
            PreviewSession session;
            try {
                session = new PreviewSession(
                    root,
                    this.settings.ResourcesDirectory,
                    previewSize.Width,
                    previewSize.Height,
                    this.previewCursors);
            }
            catch {
                NativeRuntime.xr_destroy_element(root);
                throw;
            }
            session.SetAnimationSpeed(this.GetAnimationPlaybackRate());
            session.AnimationStarted += this.PreviewSessionAnimationStarted;
            this.animationTimer.Start();
            session.Tapped += this.PreviewSessionTapped;
            session.Panned += this.PreviewSessionPanned;
            this.previewSession = session;
            this.previewLayer.Children.Add(session.Surface);
            this.statusPresenter.Success($"Предпросмотр: {path}");
        }
        catch (Exception exception) {
            this.ShowPreviewError(exception);
        }
    }

    private void ClearFolderPickerPreview() {
        this.animationTimer.Stop();
        this.pendingPageTransition = null;
        this.CompletePageTransition();
        this.previewSession?.Dispose();
        this.previewSession = null;
        this.previewLayer.Children.Clear();
    }

    private void ShowPreviewError(Exception exception) {
        this.ClearFolderPickerPreview();
        this.previewLayer.Children.Add(new Border {
            Background = PreviewRenderer.ParseBrush("#1F1717"),
            BorderBrush = PreviewRenderer.ParseBrush("#A75B5B"),
            BorderThickness = new Thickness(1),
            Child = new TextBlock {
                Margin = new Thickness(32),
                Foreground = PreviewRenderer.ParseBrush("#FFB4AB"),
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
        if (this.editorMode == EditorMode.Scenarios) {
            this.SaveScenarios("Сценарии сохранены");
            return;
        }
        if (this.editorMode == EditorMode.Settings) {
            this.settings = PreviewerSettings.Parse(this.SettingsEditor.Text, this.settings.FilePath);
            this.ConfigureMouseWheelScrolling();
            this.ApplyEditorScale();
            this.ApplySettingsToPreviewControls();
            this.SaveSettings();
            this.RefreshPageNames();
            this.LoadScenarioForCurrentMarkup();
            this.updatingEditors = true;
            this.SettingsEditor.Text = this.settings.ToJson();
            this.updatingEditors = false;
            this.isSettingsDirty = false;
            this.UpdateDocumentState();
            this.statusPresenter.Success($"Настройки сохранены: {this.settings.FilePath}");
            return;
        }
        if (this.markupPath is null) {
            return;
        }

        File.WriteAllText(this.markupPath, this.markupEditorController.Text.TrimEnd());
        this.isMarkupDirty = false;
        this.UpdateDocumentState();
        this.statusPresenter.Success($"Сохранено: {this.markupPath}");
    }

    private void ScenarioPickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        this.PersistSettings();
        this.ScheduleRender();
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

    private void AnimationSpeedPickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        if (this.updatingPreviewControls || this.AnimationSpeedPicker.SelectedItem is not AnimationSpeed speed) {
            return;
        }

        this.settings.AnimationPlaybackRate = speed.Rate;
        this.previewSession?.SetAnimationSpeed(speed.Rate);
        this.outgoingSession?.SetAnimationSpeed(speed.Rate);
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
        return;
        if (!Keyboard.IsKeyDown(Key.LeftCtrl) && !Keyboard.IsKeyDown(Key.RightCtrl)) {
            return;
        }
        var point = eventArgs.GetPosition(this.PreviewViewport);
        this.previewZoomStartScale = this.GetPreviewScale();
        var wheelSteps = Math.Max(1, Math.Abs(eventArgs.Delta) / Mouse.MouseWheelDeltaForOneLine);
        var zoomFactor = Math.Pow(1.25, wheelSteps);
        this.previewZoomTargetScale = Math.Clamp(
            this.previewZoomStartScale * (eventArgs.Delta > 0 ? zoomFactor : 1.0 / zoomFactor),
            0.1,
            3.0);
        this.previewZoomAnchorX = (this.PreviewViewport.HorizontalOffset + point.X) / this.previewZoomStartScale;
        this.previewZoomAnchorY = (this.PreviewViewport.VerticalOffset + point.Y) / this.previewZoomStartScale;
        this.previewZoomViewportPoint = point;
        this.previewZoomStartedAt = DateTime.UtcNow;
        this.previewZoomTimer.Start();
        eventArgs.Handled = true;
    }

    private void PreviewZoomTimerTick(object? sender, EventArgs eventArgs) {
        const double durationMilliseconds = 120.0;
        var progress = Math.Clamp((DateTime.UtcNow - this.previewZoomStartedAt).TotalMilliseconds / durationMilliseconds, 0.0, 1.0);
        var easedProgress = progress;
        // var inverseProgress = 1.0 - progress;
        // var easedProgress = 1.0 - inverseProgress * inverseProgress * inverseProgress;
        var scale = this.previewZoomStartScale
            + (this.previewZoomTargetScale - this.previewZoomStartScale) * easedProgress;
        this.settings.PreviewScale = scale;
        this.ApplyPreviewLayout();
        this.PreviewViewport.ScrollToHorizontalOffset(
            this.previewZoomAnchorX * scale - this.previewZoomViewportPoint.X);
        this.PreviewViewport.ScrollToVerticalOffset(
            this.previewZoomAnchorY * scale - this.previewZoomViewportPoint.Y);
        if (progress < 1.0) {
            return;
        }
        this.previewZoomTimer.Stop();
        this.SyncSettingsEditor();
        this.PersistSettings();
    }

    private void PreviewViewportPreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        this.previewViewportController.HandleMouseDown(eventArgs);
        return;
        if ((Keyboard.Modifiers & ModifierKeys.Control) == 0) {
            return;
        }
        this.isPreviewPanning = true;
        this.previewPanStart = eventArgs.GetPosition(this.PreviewViewport);
        this.previewPanHorizontalOffset = this.PreviewViewport.HorizontalOffset;
        this.previewPanVerticalOffset = this.PreviewViewport.VerticalOffset;
        this.PreviewViewport.Cursor = Cursors.Cross;
        this.PreviewViewport.CaptureMouse();
        eventArgs.Handled = true;
    }

    private void PreviewViewportPreviewMouseMove(object sender, MouseEventArgs eventArgs) {
        this.previewViewportController.HandleMouseMove(eventArgs);
        return;
        if (!this.isPreviewPanning) {
            return;
        }
        var point = eventArgs.GetPosition(this.PreviewViewport);
        this.PreviewViewport.ScrollToHorizontalOffset(this.previewPanHorizontalOffset - point.X + this.previewPanStart.X);
        this.PreviewViewport.ScrollToVerticalOffset(this.previewPanVerticalOffset - point.Y + this.previewPanStart.Y);
        eventArgs.Handled = true;
    }

    private void PreviewViewportPreviewMouseLeftButtonUp(object sender, MouseButtonEventArgs eventArgs) {
        this.previewViewportController.HandleMouseUp(eventArgs);
        return;
        if (!this.isPreviewPanning) {
            return;
        }
        this.StopPreviewPanning();
        eventArgs.Handled = true;
    }

    private void PreviewViewportLostMouseCapture(object sender, MouseEventArgs eventArgs) {
        this.previewViewportController.HandleLostMouseCapture();
        return;
        this.StopPreviewPanning();
    }

    private void WindowPreviewKeyDown(object sender, KeyEventArgs eventArgs) {
        var key = eventArgs.Key == Key.System ? eventArgs.SystemKey : eventArgs.Key;
        if ((key is Key.LeftAlt or Key.RightAlt) && this.ElementInspectionButton.IsChecked != true) {
            this.UpdateElementInspection();
        }
    }

    private void WindowPreviewKeyUp(object sender, KeyEventArgs eventArgs) {
        var key = eventArgs.Key == Key.System ? eventArgs.SystemKey : eventArgs.Key;
        if ((key is Key.LeftAlt or Key.RightAlt) && this.ElementInspectionButton.IsChecked != true) {
            this.UpdateElementInspection();
        }
        this.previewViewportController.HandleKeyUp(eventArgs);
        return;
        if (eventArgs.Key == Key.LeftCtrl || eventArgs.Key == Key.RightCtrl) {
            this.StopPreviewPanning();
        }
    }

    private void StopPreviewPanning() {
        if (!this.isPreviewPanning) {
            return;
        }
        this.isPreviewPanning = false;
        this.PreviewViewport.Cursor = null;
        this.PreviewViewport.ReleaseMouseCapture();
    }

    private void UpdateElementInspection() {
        if (this.isClosing) {
            return;
        }
        this.previewSession?.SetElementInspectionEnabled(
            this.IsActive && this.editorMode == EditorMode.Xaml
            && (this.ElementInspectionButton.IsChecked == true
                || Keyboard.IsKeyDown(Key.LeftAlt)
                || Keyboard.IsKeyDown(Key.RightAlt)));
    }

    private void ElementInspectionButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.UpdateElementInspection();
    }

    private void PreviewElementSelected(object? sender, (int Line, int Column) location) {
        if (this.editorMode != EditorMode.Xaml || !ReferenceEquals(sender, this.previewSession)) {
            return;
        }
        var document = this.MarkupEditor.Document;
        if (location.Line < 1 || location.Line > document.LineCount) {
            return;
        }
        var line = document.GetLineByNumber(location.Line);
        this.MarkupEditor.CaretOffset = line.Offset + Math.Min(location.Column - 1, line.Length);
        this.MarkupEditor.Select(this.MarkupEditor.CaretOffset, 0);
        this.MarkupEditor.ScrollTo(location.Line, location.Column);
        this.MarkupEditor.Focus();
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

    private void EditorModeToggleClick(object sender, RoutedEventArgs eventArgs) {
        this.SettingsButton.IsChecked = false;
        this.editorMode = this.EditorModeToggle.IsChecked == true
            ? EditorMode.Scenarios
            : EditorMode.Xaml;
        this.UpdateEditorMode();
    }

    private void SettingsButtonClick(object sender, RoutedEventArgs eventArgs) {
        if (this.SettingsButton.IsChecked == true) {
            this.previousEditorMode = this.editorMode == EditorMode.Settings
                ? this.previousEditorMode
                : this.editorMode;
            this.EditorModeToggle.IsChecked = false;
            this.editorMode = EditorMode.Settings;
            if (!this.isSettingsDirty) {
                this.SyncSettingsEditor();
            }
        } else {
            this.editorMode = this.previousEditorMode;
            this.EditorModeToggle.IsChecked = this.editorMode == EditorMode.Scenarios;
        }
        this.UpdateEditorMode();
    }

    private void PagePickerSelectionChanged(object sender, SelectionChangedEventArgs eventArgs) {
        if (this.PagePicker.SelectedItem is string pageName) {
            this.LoadMarkup(Path.Combine(this.settings.XamlDirectory, pageName));
        }

        this.RefreshScenarioNames();
        // Редактирование XAML намеренно debounce'ится, но navigation уже имеет
        // готовую цель. Иначе пользователь ждёт 250 мс до начала transition.
        if (this.pendingPageTransition is not null) {
            this.RenderTimerTick(this, EventArgs.Empty);
        }
    }

    private void EditorTextChanged(object sender, EventArgs eventArgs) {
        if (ReferenceEquals(sender, this.MarkupEditor)) {
            if (this.markupEditorController.HandleTextChanged()) {
                this.isMarkupDirty = true;
                try {
                    var source = this.GetScenarioSource(
                        this.markupEditorController.Text,
                        this.markupPath ?? Path.Combine(this.settings.XamlDirectory, "Preview.xaml"));
                    if (!string.Equals(source.Path, this.scenariosPath, StringComparison.OrdinalIgnoreCase)
                        || source.IsPageSpecific != this.usesPageSpecificScenarios) {
                        this.LoadScenarioForCurrentMarkup();
                    }
                }
                catch (InvalidDataException) {
                }
                catch (System.Xml.XmlException) {
                }
                this.UpdateDocumentState();
                this.ScheduleRender();
            }

            return;
        }

        if (!this.updatingEditors) {
            if (ReferenceEquals(sender, this.ScenarioEditor)) {
                this.isScenariosDirty = true;
                this.RefreshScenarioNames();
            } else if (ReferenceEquals(sender, this.SettingsEditor)) {
                this.isSettingsDirty = true;
            }

            this.UpdateDocumentState();
            this.ScheduleRender();
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
        try {
            this.markupPreviewResolution = PreviewRenderer.GetPreviewResolution(this.markupEditorController.Text);
            this.ApplyPreviewLayout();
            using var document = JsonDocument.Parse(this.ScenarioEditor.Text);
            var scenarioName = this.ScenarioPicker.SelectedItem as string;
            var scenarios = this.GetScenarios(document.RootElement);
            var data = scenarioName is not null && scenarios.TryGetProperty(scenarioName, out var selected)
                ? selected
                : scenarios;
            string sourceMarkup = this.markupEditorController.Text;
            var locations = new Dictionary<IntPtr, (int Line, int Column)>();
            var root = PreviewRenderer.CreateRootWithLocations(
                sourceMarkup,
                data,
                locations,
                this.settings.XamlDirectory);
            var previewSize = this.GetPreviewSize();
            this.animationTimer.Stop();
            var previousSession = this.previewSession;
            var transition = this.pendingPageTransition;
            this.pendingPageTransition = null;
            this.CompletePageTransition();
            PreviewSession session;
            try {
                session = new PreviewSession(
                    root,
                    this.settings.ResourcesDirectory,
                    previewSize.Width,
                    previewSize.Height,
                    this.previewCursors,
                    previousSession is not null && transition is not null);
            }
            catch {
                NativeRuntime.xr_destroy_element(root);
                throw;
            }
            this.previewLayer.Children.Clear();
            session.SetAnimationSpeed(this.GetAnimationPlaybackRate());
            session.AnimationStarted += this.PreviewSessionAnimationStarted;
            this.animationTimer.Start();
            session.Tapped += this.PreviewSessionTapped;
            session.Panned += this.PreviewSessionPanned;
            this.previewSession = session;
            if (previousSession is not null && transition is not null) {
                this.StartPageTransition(previousSession, session, transition.Value);
            } else {
                previousSession?.Dispose();
                this.previewLayer.Children.Add(session.Surface);
            }
            session.SetSourceLocations(locations);
            session.ElementSelected += (sender, location) => {
                if (this.markupEditorController.Text == sourceMarkup) {
                    this.PreviewElementSelected(sender, location);
                }
            };
            this.UpdateElementInspection();
            this.statusPresenter.Success($"Предпросмотр обновлён · {DateTime.Now:HH:mm:ss}");
            if (this.shouldRestorePreviewPosition) {
                this.shouldRestorePreviewPosition = false;
                this.Dispatcher.BeginInvoke(new Action(this.RestorePreviewPosition));
            }
        }
        catch (Exception exception) {
            this.ShowPreviewError(exception);
        }
    }

    private void AnimationTimerTick(object? sender, EventArgs eventArgs) {
        if (this.isClosing) {
            return;
        }
        var currentSession = this.previewSession;
        var previousOutgoingSession = this.outgoingSession;
        bool currentAnimating = currentSession?.Update() ?? false;
        // Update can synchronously replace the session through the Panned event.
        // Its return value then describes the old session, not the new animation.
        if (!ReferenceEquals(currentSession, this.previewSession)
            || !ReferenceEquals(previousOutgoingSession, this.outgoingSession)) {
            NativeRuntime.xr_log_info("Animation tick: session replaced during Update; keeping timer active for the new session.");
            return;
        }
        bool outgoingAnimating = previousOutgoingSession?.Update() ?? false;
        if (!currentAnimating && !outgoingAnimating) {
            this.CompletePageTransition();
            this.animationTimer.Stop();
        }
    }

    private void PreviewSessionAnimationStarted(object? sender, EventArgs eventArgs) {
        if (this.isClosing) {
            return;
        }
        this.animationTimer.Start();
    }

    private void PreviewSessionTapped(object? sender, string elementId) {
        if (this.outgoingSession is not null || string.IsNullOrEmpty(elementId)) {
            return;
        }
        try {
            if (this.previewGestureController.HandleTap(elementId)) {
                return;
            }
            this.ApplyScenarioTap(elementId);
        }
        catch (Exception exception) {
            this.ShowPreviewError(exception);
        }
    }

    private void PreviewSessionPanned(object? sender, (IntPtr Element, string ElementId, int ItemIndex) pan) {
        if (this.outgoingSession is not null || sender is not PreviewSession session) {
            return;
        }
        try {
            if (this.previewGestureController.HandlePan(pan.ElementId, pan.ItemIndex)) {
                session.RemoveItem(pan.Element);
            }
        }
        catch (Exception exception) {
            this.ShowPreviewError(exception);
        }
    }

    private JsonObject GetPreviewScenarioRoot() {
        return JsonNode.Parse(this.ScenarioEditor.Text) as JsonObject
            ?? throw new InvalidDataException("Сценарии должны содержать JSON-объект.");
    }

    private void SavePreviewScenarioChanges(JsonObject root, bool renderPreview) {
        this.updatingEditors = true;
        try {
            this.ScenarioEditor.Text = root.ToJsonString(ScenarioJsonOptions);
        }
        finally {
            this.updatingEditors = false;
        }
        this.isScenariosDirty = true;
        this.UpdateDocumentState();
        this.SaveScenarios("Сценарии автоматически сохранены после изменения будильников");
        if (renderPreview) {
            this.RenderTimerTick(this, EventArgs.Empty);
        }
    }

    private void ApplyScenarioTap(string elementId) {
        var root = JsonNode.Parse(this.ScenarioEditor.Text) as JsonObject
            ?? throw new InvalidDataException("Сценарии должны содержать JSON-объект.");
        var scenario = this.GetSelectedScenario(root);
        var tap = ScenarioInteraction.GetTap(scenario, elementId);
        if (tap is null) {
            return;
        }
        var type = tap["type"]?.GetValue<string>();
        if (type == "navigate") {
            this.NavigateScenarioTap(tap);
            return;
        }
        ScenarioInteraction.HandleTap(scenario, elementId);
        var visualStateHost = tap["visualStateHost"]?.GetValue<string>();
        var visualStateGroup = tap["visualStateGroup"]?.GetValue<string>();
        var visualStatePath = tap["path"]?.GetValue<string>();
        string? visualState = null;
        if (visualStateHost is not null
            && visualStateGroup is not null
            && visualStatePath is not null
            && scenario[visualStatePath]?.GetValue<bool>() is bool visualStateValue) {
            visualState = tap[visualStateValue ? "visualStateTrue" : "visualStateFalse"]?.GetValue<string>();
            if (visualState is not null) {
                this.SetScenarioVisualState(scenario, visualStateHost, visualStateGroup, visualState);
            }
        }
        this.updatingEditors = true;
        try {
            this.ScenarioEditor.Text = root.ToJsonString(ScenarioJsonOptions);
        }
        finally {
            this.updatingEditors = false;
        }
        this.isScenariosDirty = true;
        this.UpdateDocumentState();
        this.SaveScenarios("Сценарии автоматически сохранены после интерактивного действия");
        if (visualStateHost is not null
            && visualStateGroup is not null
            && visualState is not null
            && this.previewSession?.GoToVisualState(visualStateHost, visualStateGroup, visualState) == true) {
            return;
        }
        if (tap["previewElement"]?.GetValue<string>() is string previewElementId
            && tap["path"]?.GetValue<string>() is string path
            && scenario[path]?.GetValue<bool>() is bool state) {
            if (tap["previewAttribute"]?.GetValue<string>() is string previewAttribute
                && tap[state ? "previewTrueValue" : "previewFalseValue"]?.GetValue<string>() is string previewValue
                && this.previewSession?.SetElementAttribute(previewElementId, previewAttribute, previewValue) == true) {
                return;
            }
            if (this.previewSession?.SetElementVisibility(previewElementId, state) == true) {
                return;
            }
        }
        this.RenderTimerTick(this, EventArgs.Empty);
    }

    private void SetScenarioVisualState(
        JsonObject scenario,
        string host,
        string group,
        string state) {
        if (scenario["$visualStates"] is not JsonArray states) {
            states = new JsonArray();
            scenario["$visualStates"] = states;
        }
        foreach (var item in states) {
            if (item is JsonObject visualState
                && visualState["host"]?.GetValue<string>() == host
                && visualState["group"]?.GetValue<string>() == group) {
                visualState["state"] = state;
                return;
            }
        }
        states.Add(new JsonObject {
            ["host"] = host,
            ["group"] = group,
            ["state"] = state
        });
    }

    private void SaveScenarios(string status) {
        File.WriteAllText(this.scenariosPath, this.ScenarioEditor.Text.TrimEnd());
        this.isScenariosDirty = false;
        this.UpdateDocumentState();
        this.statusPresenter.Success($"{status}: {this.scenariosPath}");
    }

    private void NavigateScenarioTap(JsonObject tap) {
        if (this.PagePicker.SelectedItem is not string pageName
            || tap["target"]?.GetValue<string>() is not string target) {
            throw new InvalidDataException("Обработчик navigate требует target.");
        }
        bool backward = string.Equals(tap["direction"]?.GetValue<string>(), "backward",
            StringComparison.OrdinalIgnoreCase);
        var targetPage = target + ".xaml";
        if (targetPage != pageName && this.PagePicker.Items.Contains(targetPage)) {
            this.pendingPageTransition = (PageId(pageName), PageId(targetPage), backward);
            this.PagePicker.SelectedItem = targetPage;
        }
    }

    private JsonObject GetSelectedScenario(JsonObject root) {
        JsonNode? scenarios = this.usesPageSpecificScenarios
            ? root
            : this.PagePicker.SelectedItem is string pageName
                ? root[Path.GetFileNameWithoutExtension(pageName)] ?? root
                : root;
        if (this.ScenarioPicker.SelectedItem is string scenarioName) {
            scenarios = scenarios?[scenarioName] ?? scenarios;
        }
        return scenarios as JsonObject
            ?? throw new InvalidDataException("Выбранный сценарий должен быть JSON-объектом.");
    }

    private static string PageId(string fileName) {
        var name = Path.GetFileNameWithoutExtension(fileName);
        if (name.EndsWith("Page", StringComparison.Ordinal)) {
            name = name[..^4];
        }
        return name.Length == 0 ? name : char.ToLowerInvariant(name[0]) + name[1..];
    }

    private void StartPageTransition(
        PreviewSession previousSession,
        PreviewSession nextSession,
        (string From, string To, bool Backward) transition) {
        this.outgoingSession = previousSession;
        previousSession.Surface.IsHitTestVisible = false;
        nextSession.Surface.IsHitTestVisible = false;
        this.previewLayer.Children.Add(previousSession.Surface);
        this.previewLayer.Children.Add(nextSession.Surface);
        previousSession.Transition(transition.From, transition.To, transition.Backward, false);
        nextSession.Transition(transition.From, transition.To, transition.Backward, true);
        this.animationTimer.Start();
    }

    private void CompletePageTransition() {
        if (this.outgoingSession is not null) {
            this.previewLayer.Children.Remove(this.outgoingSession.Surface);
            this.outgoingSession.Dispose();
            this.outgoingSession = null;
        }
        if (this.previewSession is not null) {
            this.previewSession.Surface.IsHitTestVisible = true;
        }
    }

    private ScenarioSource GetScenarioSource(string markup, string markupFilePath) {
        var scenarioReference = PreviewRenderer.GetPreviewScenarioPath(markup);
        if (scenarioReference is null) {
            return new ScenarioSource {
                Path = this.settings.ScenariosPath,
                IsPageSpecific = false,
            };
        }
        if (Path.IsPathRooted(scenarioReference)) {
            throw new InvalidDataException("Путь mobileclock-preview-scenario должен быть относительным.");
        }
        var markupDirectory = Path.GetDirectoryName(markupFilePath)
            ?? throw new InvalidDataException("Не удалось определить каталог XAML.");
        return new ScenarioSource {
            Path = Path.GetFullPath(Path.Combine(markupDirectory, scenarioReference)),
            IsPageSpecific = true,
        };
    }

    private void LoadScenarioForCurrentMarkup() {
        var markupFilePath = this.markupPath ?? Path.Combine(this.settings.XamlDirectory, "Preview.xaml");
        var source = this.GetScenarioSource(this.markupEditorController.Text, markupFilePath);
        this.scenariosPath = source.Path;
        this.usesPageSpecificScenarios = source.IsPageSpecific;
        this.ConfigureWatchers();
        this.updatingEditors = true;
        try {
            this.ScenarioEditor.Text = File.Exists(this.scenariosPath)
                ? File.ReadAllText(this.scenariosPath)
                : "{}";
        }
        finally {
            this.updatingEditors = false;
        }
        this.isScenariosDirty = false;
        this.RefreshScenarioNames();
    }

    private JsonElement GetScenarios(JsonElement document) {
        return MainWindow.GetScenarios(
            document,
            this.usesPageSpecificScenarios,
            this.PagePicker.SelectedItem as string);
    }

    private static JsonElement GetScenarios(
        JsonElement document,
        bool isPageSpecific,
        string? pageName) {
        if (isPageSpecific) {
            return document;
        }
        return document.ValueKind == JsonValueKind.Object
            && pageName is not null
            && document.TryGetProperty(Path.GetFileNameWithoutExtension(pageName), out var selectedPage)
            ? selectedPage
            : document;
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
        this.LoadScenarioForCurrentMarkup();
        this.UpdateDocumentState();
        // PersistSettings записывает previewer.settings.json. Его изменение
        // асинхронно придёт обратно через settingsWatcher, поэтому refresh ниже
        // не должен самовольно выбирать MainPage вместо текущей страницы.
        this.PersistSettings();
        this.ScheduleRender();
    }

    private void RefreshScenarioNames() {
        var previous = this.ScenarioPicker.SelectedItem as string;
        try {
            using var document = JsonDocument.Parse(this.ScenarioEditor.Text);
            var scenarios = this.GetScenarios(document.RootElement);
            if (scenarios.ValueKind != JsonValueKind.Object) {
                return;
            }

            var names = scenarios.EnumerateObject().Select(property => property.Name).ToArray();
            if (this.ScenarioPicker.Items.Cast<string>().SequenceEqual(names)) {
                return;
            }
            this.ScenarioPicker.ItemsSource = names;
            this.ScenarioPicker.SelectedItem = names.Contains(previous) ? previous : names.FirstOrDefault();
        }
        catch (JsonException) {
        }
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
        var scenariosPath = string.IsNullOrEmpty(this.scenariosPath)
            ? this.settings.ScenariosPath
            : this.scenariosPath;
        this.fileWatchController.Configure(
            this.markupPath,
            scenariosPath,
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
                    this.UpdateDocumentState();
                    previewChanged = true;
                }
            }
            if (File.Exists(this.scenariosPath)) {
                var scenarios = File.ReadAllText(this.scenariosPath);
                if (!this.isScenariosDirty
                    && !string.Equals(scenarios, this.ScenarioEditor.Text, StringComparison.Ordinal)) {
                    this.updatingEditors = true;
                    this.ScenarioEditor.Text = scenarios;
                    this.updatingEditors = false;
                    this.isScenariosDirty = false;
                    this.UpdateDocumentState();
                    this.RefreshScenarioNames();
                    previewChanged = true;
                }
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
                    this.LoadScenarioForCurrentMarkup();
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
        this.settings.LastScenarioName = this.ScenarioPicker.SelectedItem as string;
        if (this.WindowState == WindowState.Normal) {
            this.settings.WindowWidth = this.Width;
            this.settings.WindowHeight = this.Height;
        }
        this.settings.IsMaximized = this.WindowState == WindowState.Maximized;
        this.settings.EditorPaneWidth = this.EditorColumn.ActualWidth;
        this.settings.Save();
        // Синхронизируем отображаемый JSON после собственного сохранения.
        // Тогда settingsWatcher не принимает нашу же запись за внешнее
        // изменение и не запускает лишний тяжёлый render PreviewSession.
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
        this.previewZoomTimer.Stop();
        this.fileWatchController.Dispose();
        this.editorScrollController.Dispose();
        this.pendingPageTransition = null;
        this.markupEditorController.Dispose();
        var session = this.previewSession;
        this.previewSession = null;
        this.CompletePageTransition();
        session?.Dispose();
        this.previewCursors.Dispose();
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
        this.UpdateElementInspection();
        this.MarkupEditor.Visibility = this.editorMode == EditorMode.Xaml ? Visibility.Visible : Visibility.Collapsed;
        this.ScenarioPanel.Visibility = this.editorMode == EditorMode.Scenarios ? Visibility.Visible : Visibility.Collapsed;
        this.SettingsPanel.Visibility = this.editorMode == EditorMode.Settings ? Visibility.Visible : Visibility.Collapsed;
        this.OpenButton.IsEnabled = this.editorMode == EditorMode.Xaml;
        this.XamlModeText.Foreground = PreviewRenderer.ParseBrush(
            this.editorMode == EditorMode.Xaml ? "#D5BD7D" : "#E6E6E6");
        this.ScenariosModeText.Foreground = PreviewRenderer.ParseBrush(
            this.editorMode == EditorMode.Scenarios ? "#D5BD7D" : "#E6E6E6");
        this.UpdateDocumentState();
    }

    private void UpdateDocumentState() {
        this.XamlModeText.Text = this.isMarkupDirty ? "XAML *" : "XAML";
        this.ScenariosModeText.Text = this.isScenariosDirty ? "Сценарии *" : "Сценарии";
        this.SettingsButton.Content = this.isSettingsDirty ? "Настройки *" : "Настройки";
        this.SaveButton.IsEnabled = this.editorMode switch {
            EditorMode.Xaml => this.isMarkupDirty,
            EditorMode.Scenarios => this.isScenariosDirty,
            EditorMode.Settings => this.isSettingsDirty,
            _ => false,
        };
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
        this.previewSession?.SetAnimationSpeed(this.GetAnimationPlaybackRate());
        this.outgoingSession?.SetAnimationSpeed(this.GetAnimationPlaybackRate());
    }

    private void UpdatePreviewOrientationToggle() {
        this.PortraitOrientationText.Foreground = PreviewRenderer.ParseBrush(
            this.settings.IsPreviewLandscape ? "#E6E6E6" : "#D5BD7D");
        this.LandscapeOrientationText.Foreground = PreviewRenderer.ParseBrush(
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
        if (this.markupPreviewResolution is { } resolution) {
            return resolution;
        }
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
        editor.TextArea.Caret.CaretBrush = PreviewRenderer.ParseBrush("#F0D78C");
        editor.TextArea.SelectionBrush = PreviewRenderer.ParseBrush("#5A4D26");
        editor.TextArea.SelectionForeground = PreviewRenderer.ParseBrush("#FFFFFF");
        var searchPanel = SearchPanel.Install(editor);
        searchPanel.Background = PreviewRenderer.ParseBrush("#252525");
        searchPanel.BorderBrush = PreviewRenderer.ParseBrush("#4A4A4A");
        searchPanel.Foreground = PreviewRenderer.ParseBrush("#E6E6E6");
        searchPanel.MarkerBrush = PreviewRenderer.ParseBrush("#665A4D26");
        searchPanel.MarkerPen = new Pen(PreviewRenderer.ParseBrush("#D5BD7D"), 1.0);
        var messageField = typeof(SearchPanel).GetField("messageView", BindingFlags.Instance | BindingFlags.NonPublic);
        if (messageField?.GetValue(searchPanel) is ToolTip message) {
            message.Visibility = Visibility.Collapsed;
        }
        return searchPanel;
    }

    private void ConfigureMouseWheelScrolling() {
        this.editorScrollController.Configure(
            this.MarkupEditor,
            this.ScenarioEditor,
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
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media.Animation;

namespace XamlPreviewer;

internal sealed class WindowLayoutController {
    private const double MinimumEditorPaneRatio = 0.2;
    private const double MaximumEditorPaneRatio = 0.8;
    private const double MinimumNavigationGraphPaneWidth = 280.0;
    private const double MaximumNavigationGraphPaneWidth = 900.0;
    private readonly ColumnDefinition editorColumn;
    private readonly ColumnDefinition workspaceColumn;
    private readonly ColumnDefinition editorNavigationSplitterColumn;
    private readonly ColumnDefinition navigationGraphColumn;
    private readonly ColumnDefinition navigationPreviewSplitterColumn;
    private readonly ColumnDefinition previewColumn;
    private readonly FrameworkElement editorNavigationSplitter;
    private readonly FrameworkElement navigationGraphPanel;
    private readonly FrameworkElement navigationPreviewSplitter;
    private readonly GridLength splitterWidth;
    private int navigationGraphAnimationGeneration;

    public WindowLayoutController(
        ColumnDefinition editorColumn,
        ColumnDefinition workspaceColumn,
        ColumnDefinition editorNavigationSplitterColumn,
        ColumnDefinition navigationGraphColumn,
        ColumnDefinition navigationPreviewSplitterColumn,
        ColumnDefinition previewColumn,
        FrameworkElement editorNavigationSplitter,
        FrameworkElement navigationGraphPanel,
        FrameworkElement navigationPreviewSplitter,
        GridLength splitterWidth) {
        this.editorColumn = editorColumn;
        this.workspaceColumn = workspaceColumn;
        this.editorNavigationSplitterColumn = editorNavigationSplitterColumn;
        this.navigationGraphColumn = navigationGraphColumn;
        this.navigationPreviewSplitterColumn = navigationPreviewSplitterColumn;
        this.previewColumn = previewColumn;
        this.editorNavigationSplitter = editorNavigationSplitter;
        this.navigationGraphPanel = navigationGraphPanel;
        this.navigationPreviewSplitter = navigationPreviewSplitter;
        this.splitterWidth = splitterWidth;
    }

    public double GetEditorPaneRatio(PreviewerSettings settings) {
        var panesWidth = this.editorColumn.ActualWidth + this.workspaceColumn.ActualWidth;
        return panesWidth <= 0.0 ? settings.EditorPaneRatio : Math.Clamp(this.editorColumn.ActualWidth / panesWidth, MinimumEditorPaneRatio, MaximumEditorPaneRatio);
    }

    public void ApplyEditorPreviewSplit(PreviewerSettings settings) {
        var ratio = Math.Clamp(settings.EditorPaneRatio, MinimumEditorPaneRatio, MaximumEditorPaneRatio);
        settings.EditorPaneRatio = ratio;
        this.editorColumn.Width = new GridLength(ratio, GridUnitType.Star);
        this.workspaceColumn.Width = new GridLength(1.0 - ratio, GridUnitType.Star);
    }

    public double GetNavigationGraphPaneWidth() {
        return Math.Clamp(this.navigationGraphColumn.ActualWidth, MinimumNavigationGraphPaneWidth, MaximumNavigationGraphPaneWidth);
    }

    public void UpdateNavigationGraph(PreviewerSettings settings, bool animate) {
        var graphWidth = Math.Clamp(settings.NavigationGraphPaneWidth, MinimumNavigationGraphPaneWidth, MaximumNavigationGraphPaneWidth);
        var targetWidth = settings.IsNavigationGraphVisible ? graphWidth : 0.0;
        if (!animate) {
            this.navigationGraphColumn.BeginAnimation(ColumnDefinition.WidthProperty, null);
            this.ApplyNavigationGraphLayout(settings.IsNavigationGraphVisible ? graphWidth : 0.0, settings.IsNavigationGraphVisible);
            return;
        }
        var generation = ++this.navigationGraphAnimationGeneration;
        var sourceWidth = this.navigationGraphPanel.Visibility == Visibility.Visible ? this.navigationGraphColumn.ActualWidth : 0.0;
        this.ApplyNavigationGraphLayout(sourceWidth, true);
        var animation = new GridLengthAnimation {
            From = new GridLength(sourceWidth),
            To = new GridLength(targetWidth),
            Duration = TimeSpan.FromMilliseconds(220.0),
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut },
        };
        animation.Completed += (_, _) => {
            if (generation == this.navigationGraphAnimationGeneration) {
                this.navigationGraphColumn.BeginAnimation(ColumnDefinition.WidthProperty, null);
                this.ApplyNavigationGraphLayout(settings.IsNavigationGraphVisible ? graphWidth : 0.0, settings.IsNavigationGraphVisible);
            }
        };
        this.navigationGraphColumn.BeginAnimation(ColumnDefinition.WidthProperty, animation);
    }

    private void ApplyNavigationGraphLayout(double graphWidth, bool isVisible) {
        this.editorNavigationSplitter.Visibility = Visibility.Visible;
        this.navigationGraphPanel.Visibility = isVisible ? Visibility.Visible : Visibility.Collapsed;
        this.navigationPreviewSplitter.Visibility = isVisible ? Visibility.Visible : Visibility.Collapsed;
        this.editorNavigationSplitterColumn.Width = this.splitterWidth;
        this.navigationGraphColumn.Width = new GridLength(graphWidth);
        this.navigationPreviewSplitterColumn.Width = isVisible ? this.splitterWidth : new GridLength(0.0);
        this.previewColumn.Width = new GridLength(1.0, GridUnitType.Star);
    }
}
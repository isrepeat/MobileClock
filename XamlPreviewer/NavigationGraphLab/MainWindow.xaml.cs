using Microsoft.Win32;
using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using ShapePath = System.Windows.Shapes.Path;
using Polygon = System.Windows.Shapes.Polygon;

namespace NavigationGraphLab;

public partial class MainWindow : Window {
    private const double NodeWidth = 176.0;
    private const double NodeHeight = 74.0;
    private const double LayerHeight = 154.0;
    private const double GraphHorizontalPadding = 48.0;
    private readonly Dictionary<string, IReadOnlyList<string>> lastPathByTarget = new(StringComparer.Ordinal);
    private readonly List<string> navigationHistory = [];
    private readonly Dictionary<string, RouteSlot> routeSlots = new(StringComparer.Ordinal);
    private NavigationGraphDocument? document;
    private IReadOnlyList<IReadOnlyList<NavigationTransition>> pathCandidates = [];
    private IReadOnlyList<NavigationTransition> selectedPath = [];
    private string? selectedTargetId;

    public MainWindow() {
        this.InitializeComponent();
        WindowTheme.EnableDarkTitleBar(this);
        this.LoadGraph(Path.Combine(AppContext.BaseDirectory, "navigation-graph.json"));
    }

    private void OpenGraphButtonClick(object sender, RoutedEventArgs eventArgs) {
        var dialog = new OpenFileDialog {
            Filter = "Navigation graph JSON|*.json|All files|*.*",
            InitialDirectory = this.document is null ? AppContext.BaseDirectory : Path.GetDirectoryName(this.document.Path),
        };
        if (dialog.ShowDialog(this) == true) {
            this.LoadGraph(dialog.FileName);
        }
    }

    private void ResetButtonClick(object sender, RoutedEventArgs eventArgs) {
        this.ClearSelection();
        this.lastPathByTarget.Clear();
        this.Render();
    }

    private void GraphCanvasMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        if (!ReferenceEquals(eventArgs.OriginalSource, this.GraphCanvas)) {
            return;
        }
        this.ClearSelection();
        this.Render();
    }

    private void ClearSelection() {
        this.selectedTargetId = null;
        this.selectedPath = [];
        this.pathCandidates = [];
    }

    private void LoadGraph(string path) {
        try {
            var content = File.ReadAllText(path);
            var graph = JsonSerializer.Deserialize<NavigationGraphDocument>(content, new JsonSerializerOptions {
                PropertyNameCaseInsensitive = true,
            }) ?? throw new InvalidDataException("JSON does not contain a graph.");
            graph.Path = path;
            graph.Validate();
            this.document = graph;
            this.navigationHistory.Clear();
            this.navigationHistory.Add(graph.CurrentPageId);
            this.selectedTargetId = null;
            this.selectedPath = [];
            this.pathCandidates = [];
            this.lastPathByTarget.Clear();
            this.GraphPathText.Text = path;
            this.Render();
        }
        catch (Exception exception) when (exception is IOException || exception is JsonException || exception is InvalidDataException) {
            this.document = null;
            this.GraphCanvas.Children.Clear();
            this.StatusText.Text = $"Не удалось открыть граф: {exception.Message}";
        }
    }

    private void Render() {
        this.GraphCanvas.Children.Clear();
        if (this.document is null) {
            return;
        }
        var graphWidth = this.CalculateGraphWidth();
        var positions = this.CalculatePositions(graphWidth);
        this.GraphCanvas.Width = graphWidth;
        this.GraphCanvas.Height = Math.Max(480.0, positions.Values.Max(point => point.Y) + NodeHeight + 56.0);
        var transitionGroups = this.GetNavigationTransitions()
            .GroupBy(transition => PagePair.Create(transition.SourcePageId, transition.TargetPageId))
            .ToArray();
        this.AssignRouteSlots(transitionGroups, positions);
        foreach (var group in transitionGroups) {
            this.DrawTransitionGroup(group.Key, group.ToArray(), positions);
        }
        foreach (var page in this.document.Pages) {
            this.DrawNode(page, positions[page.Id]);
        }
        this.RenderBranchPanel();
        this.UpdateStatus();
    }

    private double CalculateGraphWidth() {
        var largestLayer = this.GetPageDepths()
            .GroupBy(pair => pair.Value)
            .Max(layer => layer.Count());
        return Math.Max(760.0, largestLayer * NodeWidth + (largestLayer + 1) * GraphHorizontalPadding);
    }

    private Dictionary<string, Point> CalculatePositions(double width) {
        var document = this.document!;
        var depths = this.GetPageDepths();
        var positions = new Dictionary<string, Point>(StringComparer.Ordinal);
        foreach (var layer in document.Pages.GroupBy(page => depths[page.Id]).OrderBy(group => group.Key)) {
            var pages = layer.OrderBy(page => page.Title).ToArray();
            var gap = (width - pages.Length * NodeWidth) / (pages.Length + 1);
            for (var index = 0; index < pages.Length; ++index) {
                positions.Add(pages[index].Id, new Point(gap + index * (NodeWidth + gap), 34.0 + layer.Key * LayerHeight));
            }
        }
        return positions;
    }

    private Dictionary<string, int> GetPageDepths() {
        var document = this.document!;
        var depths = new Dictionary<string, int>(StringComparer.Ordinal) {
            [document.LayoutRootPageId] = 0,
        };
        for (var depth = 0; depth < document.Pages.Count; ++depth) {
            foreach (var transition in document.Transitions.Where(transition => !transition.IsBackNavigation && depths.TryGetValue(transition.SourcePageId, out var sourceDepth) && sourceDepth == depth)) {
                depths.TryAdd(transition.TargetPageId, depth + 1);
            }
        }
        foreach (var page in document.Pages.Where(page => !depths.ContainsKey(page.Id))) {
            depths.Add(page.Id, depths.Values.DefaultIfEmpty().Max() + 1);
        }
        return depths;
    }

    private void DrawTransitionGroup(PagePair pair, IReadOnlyList<NavigationTransition> transitions, IReadOnlyDictionary<string, Point> positions) {
        var (firstPageId, secondPageId) = this.GetOrderedEndpoints(pair, positions);
        var forwardTransitions = transitions
            .Where(transition => transition.SourcePageId == firstPageId && transition.TargetPageId == secondPageId)
            .ToArray();
        var backwardTransitions = transitions
            .Where(transition => transition.SourcePageId == secondPageId && transition.TargetPageId == firstPageId)
            .ToArray();
        var visibleTransitions = this.GetVisibleTransitions(firstPageId, secondPageId, transitions);
        var selectedBackwardTransition = backwardTransitions.FirstOrDefault(transition => this.selectedPath.Any(item => item.Id == transition.Id));
        for (var index = 0; index < visibleTransitions.Count; ++index) {
            var transition = visibleTransitions[index];
            var connection = this.CreateConnectionGeometry(
                positions[firstPageId],
                positions[secondPageId],
                this.routeSlots[transition.Id]);
            var isSelectedVisibleTransition = this.selectedPath.Any(item => item.Id == transition.Id);
            var isSelectedBackwardTransition = forwardTransitions.Length > 0 && selectedBackwardTransition is not null && index == 0;
            var isSelected = isSelectedVisibleTransition || isSelectedBackwardTransition;
            var isPotential = this.IsPotentialTransition(transition);
            var brush = isSelected ? Brushes.Gold : isPotential ? Brushes.SlateGray : Brushes.DimGray;
            var opacity = this.selectedTargetId is null || isPotential || isSelectedBackwardTransition ? 1.0 : 0.22;
            this.GraphCanvas.Children.Add(new ShapePath {
                Data = connection.Data,
                Stroke = brush,
                StrokeThickness = isSelected ? 4.0 : isPotential ? 2.0 : 1.5,
                Opacity = opacity,
                IsHitTestVisible = false,
            });
            if (isSelectedVisibleTransition) {
                var isForward = transition.SourcePageId == firstPageId;
                var arrowTip = isForward ? connection.SecondPoint : connection.FirstPoint;
                var arrowTail = isForward ? connection.SecondPreviousPoint : connection.FirstNextPoint;
                this.GraphCanvas.Children.Add(this.CreateRouteArrow(arrowTip, arrowTail, brush, opacity));
                this.GraphCanvas.Children.Add(this.CreateRouteStepBadge(arrowTip, arrowTail, this.GetPathStep(transition.Id)));
            }
            if (isSelectedBackwardTransition) {
                this.GraphCanvas.Children.Add(this.CreateRouteArrow(connection.FirstPoint, connection.FirstNextPoint, brush, opacity));
                this.GraphCanvas.Children.Add(this.CreateRouteStepBadge(
                    connection.FirstPoint,
                    connection.FirstNextPoint,
                    this.GetPathStep(selectedBackwardTransition.Id)));
            }
        }
    }

    private void AssignRouteSlots(
        IReadOnlyList<IGrouping<PagePair, NavigationTransition>> transitionGroups,
        IReadOnlyDictionary<string, Point> positions) {
        this.routeSlots.Clear();
        var routes = transitionGroups
            .SelectMany(group => {
                var (firstPageId, secondPageId) = this.GetOrderedEndpoints(group.Key, positions);
                return this.GetVisibleTransitions(firstPageId, secondPageId, group.ToArray())
                    .Select(transition => new RoutedTransition(
                        transition,
                        this.GetRouteLaneKey(positions[firstPageId], positions[secondPageId], group.Key)));
            })
            .ToArray();
        foreach (var lane in routes.GroupBy(route => route.LaneKey)) {
            var transitions = lane
                .OrderBy(route => route.Transition.Id, StringComparer.Ordinal)
                .ToArray();
            for (var index = 0; index < transitions.Length; ++index) {
                this.routeSlots.Add(transitions[index].Transition.Id, new RouteSlot(index, transitions.Length));
            }
        }
    }

    private IReadOnlyList<NavigationTransition> GetVisibleTransitions(
        string firstPageId,
        string secondPageId,
        IReadOnlyList<NavigationTransition> transitions) {
        var forwardTransitions = transitions
            .Where(transition => transition.SourcePageId == firstPageId && transition.TargetPageId == secondPageId)
            .ToArray();
        if (forwardTransitions.Length > 0) {
            return forwardTransitions;
        }
        return [transitions.Single(transition => transition.SourcePageId == secondPageId && transition.TargetPageId == firstPageId)];
    }

    private string GetRouteLaneKey(Point first, Point second, PagePair pair) {
        if (Math.Abs(second.Y - first.Y) > 0.01) {
            return $"vertical:{first.Y}:{second.Y}";
        }
        var distance = Math.Abs(second.X - first.X);
        return distance <= NodeWidth + GraphHorizontalPadding * 1.5
            ? $"direct:{pair.FirstPageId}:{pair.SecondPageId}"
            : $"bypass:{first.Y}";
    }

    private ConnectionGeometry CreateConnectionGeometry(Point first, Point second, RouteSlot routeSlot) {
        var firstCenter = new Point(first.X + NodeWidth / 2.0, first.Y + NodeHeight / 2.0);
        var secondCenter = new Point(second.X + NodeWidth / 2.0, second.Y + NodeHeight / 2.0);
        var portOffset = (routeSlot.Index + 1.0) / (routeSlot.Count + 1.0);
        if (Math.Abs(secondCenter.Y - firstCenter.Y) > 0.01) {
            var firstPoint = new Point(first.X + NodeWidth * portOffset, first.Y + NodeHeight);
            var secondPoint = new Point(second.X + NodeWidth * portOffset, second.Y);
            var laneY = firstPoint.Y + (secondPoint.Y - firstPoint.Y) * portOffset;
            return this.CreateOrthogonalConnection([
                firstPoint,
                new Point(firstPoint.X, laneY),
                new Point(secondPoint.X, laneY),
                secondPoint,
            ]);
        }
        var horizontalDistance = Math.Abs(secondCenter.X - firstCenter.X);
        if (horizontalDistance <= NodeWidth + GraphHorizontalPadding * 1.5) {
            return this.CreateOrthogonalConnection([
                new Point(first.X + NodeWidth, first.Y + NodeHeight * portOffset),
                new Point(second.X, second.Y + NodeHeight * portOffset),
            ]);
        }
        var firstBottom = new Point(first.X + NodeWidth * portOffset, first.Y + NodeHeight);
        var secondBottom = new Point(second.X + NodeWidth * portOffset, second.Y + NodeHeight);
        var bypassLaneY = Math.Max(firstBottom.Y, secondBottom.Y) + 20.0 + routeSlot.Index * 12.0;
        return this.CreateOrthogonalConnection([
            firstBottom,
            new Point(firstBottom.X, bypassLaneY),
            new Point(secondBottom.X, bypassLaneY),
            secondBottom,
        ]);
    }

    private ConnectionGeometry CreateOrthogonalConnection(IReadOnlyList<Point> points) {
        var routePoints = points
            .Where((point, index) => index == 0 || point != points[index - 1])
            .ToArray();
        var segments = routePoints
            .Skip(1)
            .Select(point => (PathSegment)new LineSegment(point, true))
            .ToArray();
        return new ConnectionGeometry(
            new PathGeometry([
                new PathFigure(routePoints[0], segments, false),
            ]),
            routePoints[0],
            routePoints[1],
            routePoints[^2],
            routePoints[^1]);
    }

    private void DrawNode(NavigationPage page, Point position) {
        var isCurrent = page.Id == this.document!.CurrentPageId;
        var isTarget = page.Id == this.selectedTargetId;
        var node = new Button {
            Width = NodeWidth,
            Height = NodeHeight,
            Content = page.Title,
            FontSize = 16.0,
            FontWeight = FontWeights.SemiBold,
            Background = new SolidColorBrush(isTarget ? Color.FromRgb(61, 72, 45) : Color.FromRgb(54, 54, 54)),
            BorderBrush = isCurrent ? Brushes.Gold : isTarget ? Brushes.YellowGreen : Brushes.Gray,
            BorderThickness = new Thickness(isCurrent || isTarget ? 3.0 : 1.0),
            Foreground = Brushes.White,
            ToolTip = "Один клик выбирает типичный путь; двойной запускает его.",
        };
        node.PreviewMouseLeftButtonDown += (_, eventArgs) => {
            if (eventArgs.ClickCount == 2 && page.Id == this.selectedTargetId && this.selectedPath.Count > 0) {
                this.CompleteNavigation();
                this.ClearSelection();
                this.Render();
                eventArgs.Handled = true;
                return;
            }
            this.SelectTarget(page.Id);
            eventArgs.Handled = true;
        };
        Canvas.SetLeft(node, position.X);
        Canvas.SetTop(node, position.Y);
        this.GraphCanvas.Children.Add(node);
    }

    private void SelectTarget(string targetPageId) {
        var paths = this.FindPaths(this.document!.CurrentPageId, targetPageId);
        if (paths.Count == 0) {
            this.StatusText.Text = "Из активной страницы к выбранной цели нет маршрута.";
            return;
        }
        this.selectedTargetId = targetPageId;
        this.pathCandidates = paths;
        var previous = this.lastPathByTarget.GetValueOrDefault(targetPageId);
        this.selectedPath = previous is null
            ? paths[0]
            : paths.FirstOrDefault(path => path.Select(transition => transition.Id).SequenceEqual(previous)) ?? paths[0];
        this.Render();
    }

    private void UpdateStatus() {
        if (this.selectedTargetId is null) {
            this.StatusText.Text = $"Активная страница: {this.PageTitle(this.document!.CurrentPageId)}. Выберите страницу, чтобы увидеть пути к ней.";
            return;
        }
        var target = this.document!.Pages.Single(page => page.Id == this.selectedTargetId);
        this.StatusText.Text = $"Выбран путь «{this.PathTitle(this.selectedPath)}». Нажмите другую подпись, чтобы сменить ветку; двойной клик по {target.Title} подтвердит переход.";
    }

    private void RenderBranchPanel() {
        this.BranchPanel.Children.Clear();
        if (this.selectedTargetId is null) {
            this.BranchPanelScrollViewer.Visibility = Visibility.Collapsed;
            return;
        }
        this.BranchPanelScrollViewer.Visibility = Visibility.Visible;
        for (var index = 0; index < this.pathCandidates.Count; ++index) {
            var path = this.pathCandidates[index];
            var isSelected = path.Select(transition => transition.Id).SequenceEqual(this.selectedPath.Select(transition => transition.Id));
            var actions = new StackPanel();
            actions.Children.Add(new TextBlock {
                Text = $"Путь {index + 1}",
                FontWeight = FontWeights.SemiBold,
                Foreground = Brushes.White,
            });
            for (var step = 0; step < path.Count; ++step) {
                var transition = path[step];
                actions.Children.Add(new TextBlock {
                    Margin = new Thickness(0.0, 5.0, 0.0, 0.0),
                    Text = $"{step + 1}. {this.PageTitle(transition.SourcePageId)}: {transition.Title}",
                    TextWrapping = TextWrapping.Wrap,
                    Foreground = new SolidColorBrush(Color.FromRgb(218, 218, 218)),
                });
            }
            var card = new Border {
                Margin = new Thickness(0.0, 0.0, 0.0, 8.0),
                Padding = new Thickness(10.0),
                Background = new SolidColorBrush(isSelected ? Color.FromRgb(78, 65, 28) : Color.FromRgb(48, 48, 48)),
                BorderBrush = new SolidColorBrush(isSelected ? Colors.Gold : Color.FromRgb(92, 92, 92)),
                BorderThickness = new Thickness(isSelected ? 2.0 : 1.0),
                Child = actions,
                Cursor = Cursors.Hand,
                ToolTip = "Выбрать этот сценарий маршрута",
            };
            card.MouseLeftButtonDown += (_, eventArgs) => {
                this.SelectPath(path);
                eventArgs.Handled = true;
            };
            this.BranchPanel.Children.Add(card);
        }
    }

    private IReadOnlyList<IReadOnlyList<NavigationTransition>> FindPaths(string sourcePageId, string targetPageId) {
        if (sourcePageId == targetPageId) {
            return [];
        }
        var paths = new List<IReadOnlyList<NavigationTransition>>();
        var path = new List<NavigationTransition>();
        var visitedPages = new HashSet<string>(StringComparer.Ordinal) { sourcePageId };
        void Visit(string pageId) {
            if (pageId == targetPageId) {
                paths.Add(path.ToArray());
                return;
            }
            foreach (var transition in this.GetNavigationTransitions().Where(item => item.SourcePageId == pageId)) {
                if (!visitedPages.Add(transition.TargetPageId)) {
                    continue;
                }
                path.Add(transition);
                Visit(transition.TargetPageId);
                path.RemoveAt(path.Count - 1);
                visitedPages.Remove(transition.TargetPageId);
            }
        }
        Visit(sourcePageId);
        return paths
            .OrderBy(path => path.Count)
            .ThenByDescending(path => path.Count(transition => transition.IsDefault))
            .ThenBy(path => string.Join('/', path.Select(transition => transition.Id)), StringComparer.Ordinal)
            .ToArray();
    }

    private bool IsPotentialTransition(NavigationTransition transition) {
        return this.pathCandidates.Any(path => path.Any(item => item.Id == transition.Id));
    }

    private IReadOnlyList<NavigationTransition> GetNavigationTransitions() {
        var transitions = new List<NavigationTransition>();
        foreach (var transition in this.document!.Transitions) {
            if (!transition.IsBackNavigation) {
                transitions.Add(transition);
                continue;
            }
            for (var historyIndex = 1; historyIndex < this.navigationHistory.Count; ++historyIndex) {
                if (transition.SourcePageId != this.navigationHistory[historyIndex]) {
                    continue;
                }
                transitions.Add(new NavigationTransition {
                    Id = $"{transition.Id}@{historyIndex}",
                    SourcePageId = transition.SourcePageId,
                    TargetPageId = this.navigationHistory[historyIndex - 1],
                    TargetKind = transition.TargetKind,
                    Title = transition.Title,
                    IsDefault = transition.IsDefault,
                });
            }
        }
        return transitions;
    }

    private void CompleteNavigation() {
        foreach (var transition in this.selectedPath) {
            if (transition.IsBackNavigation) {
                this.navigationHistory.RemoveAt(this.navigationHistory.Count - 1);
            } else {
                this.navigationHistory.Add(transition.TargetPageId);
            }
        }
        this.document!.CurrentPageId = this.navigationHistory[^1];
    }

    private string PathTitle(IReadOnlyList<NavigationTransition> path) {
        return string.Join(" → ", path.Select(transition => transition.Title));
    }

    private void SelectPath(IReadOnlyList<NavigationTransition> path) {
        if (this.selectedTargetId is null) {
            return;
        }
        this.selectedPath = path;
        this.lastPathByTarget[this.selectedTargetId] = path.Select(transition => transition.Id).ToArray();
        this.Render();
    }

    private string PageTitle(string pageId) {
        return this.document!.Pages.Single(page => page.Id == pageId).Title;
    }

    private (string FirstPageId, string SecondPageId) GetOrderedEndpoints(
        PagePair pair,
        IReadOnlyDictionary<string, Point> positions) {
        var first = positions[pair.FirstPageId];
        var second = positions[pair.SecondPageId];
        if (first.Y < second.Y || (Math.Abs(first.Y - second.Y) < 0.01 && first.X <= second.X)) {
            return (pair.FirstPageId, pair.SecondPageId);
        }
        return (pair.SecondPageId, pair.FirstPageId);
    }

    private Polygon CreateRouteArrow(Point tip, Point tail, Brush fill, double opacity) {
        var direction = tip - tail;
        direction.Normalize();
        var normal = new Vector(-direction.Y, direction.X);
        return new Polygon {
            Points = new PointCollection {
                tip,
                tip - direction * 11.0 + normal * 5.0,
                tip - direction * 11.0 - normal * 5.0,
            },
            Fill = fill,
            Opacity = opacity,
            IsHitTestVisible = false,
        };
    }

    private Border CreateRouteStepBadge(Point tip, Point tail, int step) {
        var direction = tip - tail;
        var isVertical = Math.Abs(direction.Y) >= Math.Abs(direction.X);
        var badge = new Border {
            Width = 20.0,
            Height = 20.0,
            Background = new SolidColorBrush(Color.FromRgb(78, 65, 28)),
            BorderBrush = Brushes.Gold,
            BorderThickness = new Thickness(1.0),
            Child = new TextBlock {
                Text = step.ToString(),
                FontSize = 11.0,
                FontWeight = FontWeights.SemiBold,
                Foreground = Brushes.White,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
            },
            IsHitTestVisible = false,
        };
        Canvas.SetLeft(badge, isVertical ? tip.X + 8.0 : tip.X - 10.0);
        Canvas.SetTop(badge, isVertical ? tip.Y - 10.0 : tip.Y - 28.0);
        Panel.SetZIndex(badge, 100);
        return badge;
    }

    private int GetPathStep(string transitionId) {
        return this.selectedPath
            .Select((transition, index) => new { transition.Id, Step = index + 1 })
            .Single(item => item.Id == transitionId)
            .Step;
    }

    private sealed record ConnectionGeometry(
        PathGeometry Data,
        Point FirstPoint,
        Point FirstNextPoint,
        Point SecondPreviousPoint,
        Point SecondPoint);

    private sealed record RouteSlot(int Index, int Count);

    private sealed record RoutedTransition(NavigationTransition Transition, string LaneKey);

    private sealed record PagePair(string FirstPageId, string SecondPageId) {
        public static PagePair Create(string sourcePageId, string targetPageId) {
            return string.CompareOrdinal(sourcePageId, targetPageId) <= 0
                ? new PagePair(sourcePageId, targetPageId)
                : new PagePair(targetPageId, sourcePageId);
        }
    }
}

public sealed class NavigationGraphDocument {
    public string Path { get; set; } = string.Empty;
    public string CurrentPageId { get; set; } = string.Empty;
    public string LayoutRootPageId { get; set; } = string.Empty;
    public List<NavigationPage> Pages { get; set; } = [];
    public List<NavigationTransition> Transitions { get; set; } = [];

    public void Validate() {
        if (string.IsNullOrWhiteSpace(this.CurrentPageId) || !this.Pages.Any(page => page.Id == this.CurrentPageId)) {
            throw new InvalidDataException("CurrentPageId must identify a page.");
        }
        if (string.IsNullOrWhiteSpace(this.LayoutRootPageId) || !this.Pages.Any(page => page.Id == this.LayoutRootPageId)) {
            throw new InvalidDataException("LayoutRootPageId must identify a page.");
        }
        if (this.Pages.Select(page => page.Id).Distinct(StringComparer.Ordinal).Count() != this.Pages.Count) {
            throw new InvalidDataException("Page identifiers must be unique.");
        }
        if (this.Transitions.Select(transition => transition.Id).Distinct(StringComparer.Ordinal).Count() != this.Transitions.Count) {
            throw new InvalidDataException("Transition identifiers must be unique.");
        }
        var pageIds = this.Pages.Select(page => page.Id).ToHashSet(StringComparer.Ordinal);
        if (this.Transitions.Any(transition => !pageIds.Contains(transition.SourcePageId))) {
            throw new InvalidDataException("Every transition must have an existing source page.");
        }
        if (this.Transitions.Any(transition => transition.IsBackNavigation && transition.TargetPageId.Length > 0)) {
            throw new InvalidDataException("A previousPage transition must not declare TargetPageId.");
        }
        if (this.Transitions.Any(transition => !transition.IsBackNavigation && !pageIds.Contains(transition.TargetPageId))) {
            throw new InvalidDataException("A page transition must reference an existing target page.");
        }
    }
}

public sealed class NavigationPage {
    public string Id { get; set; } = string.Empty;
    public string Title { get; set; } = string.Empty;
}

public sealed class NavigationTransition {
    public string Id { get; set; } = string.Empty;
    public string SourcePageId { get; set; } = string.Empty;
    public string TargetPageId { get; set; } = string.Empty;
    public string TargetKind { get; set; } = "page";
    public string Title { get; set; } = string.Empty;
    public bool IsDefault { get; set; }
    public bool IsBackNavigation => this.TargetKind == "previousPage";
}
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using Polygon = System.Windows.Shapes.Polygon;
using Polyline = System.Windows.Shapes.Polyline;

namespace XamlPreviewer;

internal sealed class NavigationGraphController {
    private sealed record GraphEdge(string First, string Second) {
        public static GraphEdge FromRoute(PreviewRoute route) {
            return string.CompareOrdinal(route.Source, route.Target) <= 0
                ? new GraphEdge(route.Source, route.Target)
                : new GraphEdge(route.Target, route.Source);
        }

        public bool Connects(string source, string target) {
            return (this.First == source && this.Second == target)
                || (this.First == target && this.Second == source);
        }
    }

    private const double CardWidth = 172.0;
    private const double CardHeight = 78.0;
    private const double LayerSpacing = 120.0;
    private readonly Canvas graph;
    private readonly Action<string> selectPage;
    private readonly Action<string> reportInformation;
    private readonly Dictionary<string, Button> nodes = [];
    private readonly Dictionary<GraphEdge, Polyline> edges = [];
    private IReadOnlyList<PreviewRoute> routes = [];
    private IReadOnlyList<string> selectedPath = [];
    private IReadOnlyList<IReadOnlyList<string>> pathCandidates = [];
    private string? selectedTarget;
    private string? currentPage;

    public event Action<string>? ActivePageChanged;
    public event Action<IReadOnlyList<string>>? RouteConfirmed;

    public NavigationGraphController(Canvas graph, Action<string> selectPage, Action<string> reportInformation) {
        this.graph = graph;
        this.selectPage = selectPage;
        this.reportInformation = reportInformation;
    }

    public void SetRoutes(IReadOnlyList<PreviewRoute> routes, string currentPage) {
        this.routes = routes;
        this.ResetSelection();
        this.Synchronize(currentPage, true);
    }

    public void Synchronize(string currentPage, bool force = false) {
        if (!force && string.Equals(this.currentPage, currentPage, StringComparison.Ordinal)) {
            return;
        }
        var isPageChange = this.currentPage is not null
            && !string.Equals(this.currentPage, currentPage, StringComparison.Ordinal);
        this.currentPage = currentPage;
        this.Render();
        if (isPageChange) {
            this.ActivePageChanged?.Invoke(currentPage);
        }
    }

    public void CompleteNavigation(string currentPage) {
        this.ResetSelection();
        this.Synchronize(currentPage, true);
    }

    public void Clear() {
        this.routes = [];
        this.currentPage = null;
        this.ResetSelection();
        this.graph.Children.Clear();
    }

    private void NavigationNodeClick(object sender, RoutedEventArgs eventArgs) {
        if (sender is not Button { Tag: string target } || this.currentPage is null) {
            return;
        }
        if (string.Equals(this.selectedTarget, target, StringComparison.Ordinal)
            && this.selectedPath.Count > 0) {
            NativeRuntime.xr_log_info($"Preview graph confirmed route: {string.Join('>', this.selectedPath)}");
            this.RouteConfirmed?.Invoke(this.selectedPath);
            return;
        }
        var paths = this.FindPaths(this.currentPage, target);
        if (paths.Count == 0) {
            this.reportInformation($"Нет маршрута из {this.currentPage} в {target}.");
            return;
        }
        this.selectedTarget = target;
        this.pathCandidates = paths;
        this.selectedPath = paths[0];
        NativeRuntime.xr_log_info($"Preview graph selected route: {string.Join('>', this.selectedPath)}; candidates={paths.Count}");
        this.selectPage(target);
        this.Render();
    }

    private IReadOnlyList<IReadOnlyList<string>> FindPaths(string source, string target) {
        var result = new List<IReadOnlyList<string>>();
        var path = new List<string> { source };
        void Visit(string page) {
            if (page == target) {
                result.Add(path.ToArray());
                return;
            }
            foreach (var edge in this.routes.Where(edge => edge.Source == page)) {
                if (path.Contains(edge.Target, StringComparer.Ordinal)) {
                    continue;
                }
                path.Add(edge.Target);
                Visit(edge.Target);
                path.RemoveAt(path.Count - 1);
            }
        }
        Visit(source);
        return result.OrderBy(path => path.Count).ToArray();
    }

    private void NavigationEdgeClick(object sender, MouseButtonEventArgs eventArgs) {
        if (sender is not Polyline { Tag: GraphEdge edge } || this.selectedTarget is null) {
            return;
        }
        var paths = this.pathCandidates.Where(candidate => this.ContainsEdge(candidate, edge)).ToArray();
        if (paths.Length == 0) {
            return;
        }
        var currentIndex = Array.FindIndex(paths, candidate => candidate.SequenceEqual(this.selectedPath));
        this.selectedPath = paths[(currentIndex + 1) % paths.Length];
        this.Render();
        eventArgs.Handled = true;
    }

    private void Render() {
        this.graph.Children.Clear();
        this.nodes.Clear();
        this.edges.Clear();
        var pages = this.routes
            .SelectMany(route => new[] { route.Source, route.Target })
            .Distinct()
            .Order()
            .ToArray();
        if (pages.Length == 0) {
            return;
        }
        var positions = this.CalculateNodePositions(pages);
        var graphEdges = this.routes.Select(GraphEdge.FromRoute).Distinct().ToArray();
        foreach (var edge in graphEdges) {
            var (source, target) = this.GetEdgeEndpoints(edge, positions);
            var points = this.CreateRoutePoints(new PreviewRoute(source, target), positions);
            var selectedRoute = this.GetSelectedRoute(edge);
            var stroke = selectedRoute is not null
                ? new SolidColorBrush(Color.FromRgb(239, 191, 65))
                : new SolidColorBrush(Color.FromRgb(91, 91, 91));
            var line = new Polyline {
                Points = new PointCollection(points),
                StrokeThickness = selectedRoute is null ? 1.5 : 3.0,
                Stroke = stroke,
                Tag = edge,
                Cursor = Cursors.Hand,
            };
            line.MouseLeftButtonDown += this.NavigationEdgeClick;
            this.edges.Add(edge, line);
            this.graph.Children.Add(line);
            if (selectedRoute is not null) {
                var arrowPoints = selectedRoute.Source == source ? points : points.Reverse().ToArray();
                this.graph.Children.Add(this.CreateRouteArrow(arrowPoints, stroke));
            }
        }
        foreach (var page in pages) {
            var position = positions[page];
            var isCurrent = string.Equals(page, this.currentPage, StringComparison.Ordinal);
            var isTarget = string.Equals(page, this.selectedTarget, StringComparison.Ordinal);
            var button = new Button {
                Width = CardWidth,
                Height = CardHeight,
                Tag = page,
                Content = this.PageTitle(page),
                Background = new SolidColorBrush(isTarget ? Color.FromRgb(61, 72, 45) : Color.FromRgb(54, 54, 54)),
                BorderBrush = new SolidColorBrush(isCurrent || isTarget
                    ? isCurrent ? Color.FromRgb(239, 191, 65) : Color.FromRgb(169, 204, 105)
                    : Color.FromRgb(89, 89, 89)),
                Foreground = new SolidColorBrush(Color.FromRgb(242, 242, 242)),
                FontSize = 17,
                FontWeight = FontWeights.SemiBold,
                ToolTip = isTarget ? "Повторный клик запустит выбранный маршрут" : "Выбрать маршрут к странице",
            };
            button.Click += this.NavigationNodeClick;
            Canvas.SetLeft(button, position.X);
            Canvas.SetTop(button, position.Y);
            this.nodes.Add(page, button);
            this.graph.Children.Add(button);
        }
    }

    private void ResetSelection() {
        this.selectedPath = [];
        this.pathCandidates = [];
        this.selectedTarget = null;
    }

    private PreviewRoute? GetSelectedRoute(GraphEdge edge) {
        for (var index = 1; index < this.selectedPath.Count; ++index) {
            var source = this.selectedPath[index - 1];
            var target = this.selectedPath[index];
            if (edge.Connects(source, target)) {
                return new PreviewRoute(source, target);
            }
        }
        return null;
    }

    private (string Source, string Target) GetEdgeEndpoints(
        GraphEdge edge,
        IReadOnlyDictionary<string, Point> positions) {
        return positions[edge.First].Y <= positions[edge.Second].Y
            ? (edge.First, edge.Second)
            : (edge.Second, edge.First);
    }

    private Dictionary<string, Point> CalculateNodePositions(IReadOnlyList<string> pages) {
        var root = pages.Contains("MainPage", StringComparer.Ordinal) ? "MainPage" : pages[0];
        var depths = new Dictionary<string, int> { [root] = 0 };
        for (var depth = 0; depth < pages.Count; ++depth) {
            foreach (var edge in this.routes.Where(edge => depths.TryGetValue(edge.Source, out var sourceDepth) && sourceDepth == depth)) {
                if (!depths.ContainsKey(edge.Target)) {
                    depths.Add(edge.Target, depth + 1);
                }
            }
        }
        foreach (var page in pages.Where(page => !depths.ContainsKey(page))) {
            depths.Add(page, depths.Values.DefaultIfEmpty().Max() + 1);
        }
        var availableWidth = Math.Max(this.graph.ActualWidth, 410.0);
        var result = new Dictionary<string, Point>();
        foreach (var layer in pages.GroupBy(page => depths[page]).OrderBy(group => group.Key)) {
            var items = layer.Order().ToArray();
            var gap = (availableWidth - CardWidth * items.Length) / (items.Length + 1);
            for (var index = 0; index < items.Length; ++index) {
                result.Add(items[index], new Point(gap + index * (CardWidth + gap), 18 + layer.Key * LayerSpacing));
            }
        }
        var deepestLayer = depths.Values.DefaultIfEmpty().Max();
        this.graph.Height = Math.Max(500.0, 36.0 + CardHeight + deepestLayer * LayerSpacing);
        return result;
    }

    private IReadOnlyList<Point> CreateRoutePoints(PreviewRoute route, IReadOnlyDictionary<string, Point> positions) {
        var source = positions[route.Source];
        var target = positions[route.Target];
        if (this.UsesLayerCorridor(source, target)) {
            var corridorY = (source.Y + CardHeight + target.Y) / 2.0;
            return [
                new Point(source.X + CardWidth / 2.0, source.Y + CardHeight),
                new Point(source.X + CardWidth / 2.0, corridorY),
                new Point(target.X + CardWidth / 2.0, corridorY),
                new Point(target.X + CardWidth / 2.0, target.Y),
            ];
        }
        var goesLeft = this.RouteGoesLeft(route, source, target);
        var laneIndex = this.GetSideLaneIndex(route, positions, goesLeft);
        var graphWidth = Math.Max(this.graph.ActualWidth, 410.0);
        var laneX = goesLeft ? 18.0 + laneIndex * 14.0 : graphWidth - 18.0 - laneIndex * 14.0;
        var sourcePort = new Point(goesLeft ? source.X : source.X + CardWidth, source.Y + CardHeight / 2.0);
        var targetPort = new Point(goesLeft ? target.X : target.X + CardWidth, target.Y + CardHeight / 2.0);
        return [sourcePort, new Point(laneX, sourcePort.Y), new Point(laneX, targetPort.Y), targetPort];
    }

    private bool UsesLayerCorridor(Point source, Point target) {
        return target.Y > source.Y && target.Y - source.Y <= LayerSpacing + 0.01;
    }

    private bool RouteGoesLeft(PreviewRoute route, Point source, Point target) {
        var sourceCenter = source.X + CardWidth / 2.0;
        var targetCenter = target.X + CardWidth / 2.0;
        if (Math.Abs(sourceCenter - targetCenter) > 0.01) {
            return targetCenter < sourceCenter;
        }
        return string.CompareOrdinal(route.Source, route.Target) > 0;
    }

    private int GetSideLaneIndex(PreviewRoute route, IReadOnlyDictionary<string, Point> positions, bool goesLeft) {
        return this.routes
            .Where(candidate => !this.UsesLayerCorridor(positions[candidate.Source], positions[candidate.Target]))
            .Where(candidate => this.RouteGoesLeft(candidate, positions[candidate.Source], positions[candidate.Target]) == goesLeft)
            .TakeWhile(candidate => candidate != route)
            .Count();
    }

    private Polygon CreateRouteArrow(IReadOnlyList<Point> points, Brush fill) {
        var tip = points[^1];
        var tail = points[^2];
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
            IsHitTestVisible = false,
        };
    }

    private string PageTitle(string page) {
        return page switch {
            "MainPage" => "⌂  Главная",
            "AddAlarmPage" => "Новый будильник",
            "XiaomiThemesPage" => "Xiaomi Themes",
            "SettingsPage" => "⚙  Настройки",
            _ => page.Replace("Page", string.Empty, StringComparison.Ordinal),
        };
    }

    private bool ContainsEdge(IReadOnlyList<string> path, GraphEdge edge) {
        for (var index = 1; index < path.Count; ++index) {
            if (edge.Connects(path[index - 1], path[index])) {
                return true;
            }
        }
        return false;
    }
}
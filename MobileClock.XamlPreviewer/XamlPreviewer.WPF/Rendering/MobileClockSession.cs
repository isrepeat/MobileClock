using System.Windows.Media.Imaging;
using System.Text;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace XamlPreviewer;

// Native application mode. It is intentionally separate from editable-XAML mode:
// WPF hosts the image and input only; MobileClock owns the runtime tree.
internal sealed class MobileClockSession : IDisposable {
    private readonly AnglePreviewRenderer renderer;
    private readonly PreviewCursorSet cursorSet;
    private readonly Image image;
    private IntPtr session;
    private string? loadedPage;
    private readonly Dictionary<string, string> scenarios = [];
    private readonly Dictionary<string, string> markups = [];
    private bool hasPointerCapture;
    private bool isElementInspectionEnabled;
    private bool useDefaultCursorForElementInspection;

    public event Action<NativeInspectionResult>? ElementSelected;
    public event Action<string>? PageNavigated;
    public event Action? RuntimeMarkupReloaded;

    public MobileClockSession(string resourcesDirectory, int width, int height) {
        this.renderer = new AnglePreviewRenderer(resourcesDirectory, width, height);
        this.cursorSet = new PreviewCursorSet();
        this.session = NativeRuntime.mc_create_session(width, height);
        try {
            NativeRuntime.Ensure(this.session != IntPtr.Zero);
            this.image = new Image {
                Width = width,
                Height = height,
                Stretch = Stretch.Fill
            };
            this.image.MouseLeftButtonDown += this.ImageMouseLeftButtonDown;
            this.image.MouseLeftButtonUp += this.ImageMouseLeftButtonUp;
            this.image.MouseMove += this.ImageMouseMove;
            this.image.MouseLeave += this.ImageMouseLeave;
        }
        catch {
            if (this.session != IntPtr.Zero) {
                NativeRuntime.mc_destroy_session(this.session);
                this.session = IntPtr.Zero;
            }
            this.renderer.Dispose();
            throw;
        }
    }

    public int Height => this.renderer.Height;
    public FrameworkElement Surface => this.image;
    public int Width => this.renderer.Width;

    public void LoadPage(string page) {
        if (this.loadedPage == page) {
            return;
        }
        NativeRuntime.Ensure(NativeRuntime.mc_load_page(this.session, page) != 0);
        this.loadedPage = page;

        this.Render();
    }

    public void LoadRuntimeMarkup(string page, string markup, string sourcePath) {
        if (this.markups.TryGetValue(sourcePath, out var previous) && previous == markup) {
            return;
        }
        NativeRuntime.Ensure(NativeRuntime.mc_reload_markup(this.session, page, markup, sourcePath) != 0);
        if (Path.GetFileNameWithoutExtension(sourcePath) == page) {
            this.markups.Clear();
        }
        this.markups[sourcePath] = markup;
        this.Render();
        this.RuntimeMarkupReloaded?.Invoke();
    }

    public void SetAnimationPlaybackRate(double value) {
        NativeRuntime.Ensure(NativeRuntime.mc_set_animation_playback_rate(this.session, (float)value) != 0);
    }

    public void SetElementInspectionEnabled(bool value, bool useDefaultCursor) {
        this.isElementInspectionEnabled = value;
        this.useDefaultCursorForElementInspection = value && useDefaultCursor;
        this.image.Cursor = this.useDefaultCursorForElementInspection ? Cursors.Arrow : null;
        if (!value) {
            NativeRuntime.Ensure(NativeRuntime.mc_clear_inspection_wireframe(this.session) != 0);
            // Закреплённая голубая рамка принадлежит режиму выбора так же, как
            // временная рамка наведения, поэтому при выходе очищаем обе.
            NativeRuntime.Ensure(NativeRuntime.mc_clear_selected_inspection_element(this.session) != 0);
            this.Render();
        }
    }

    public void SetElementInspectionWireframes(
        ElementInspectionWireframeSettings hovered,
        ElementInspectionWireframeSettings active,
        bool renderMargin,
        bool renderPadding) {
        NativeRuntime.Ensure(NativeRuntime.mc_set_inspection_wireframe(
            this.session,
            (float)hovered.LineThickness,
            hovered.LineStyle == "solid" ? 0 : 1,
            ParseColor(hovered.LineColor),
            renderMargin ? ParseColor(hovered.MarginColor) : default,
            renderPadding ? ParseColor(hovered.PaddingColor) : default) != 0);
        NativeRuntime.Ensure(NativeRuntime.mc_set_selected_wireframe(
            this.session,
            (float)active.LineThickness,
            active.LineStyle == "solid" ? 0 : 1,
            ParseColor(active.LineColor),
            renderMargin ? ParseColor(active.MarginColor) : default,
            renderPadding ? ParseColor(active.PaddingColor) : default) != 0);
        this.Render();
    }

    public bool SelectElementInspection(string sourcePath, int line, int column) {
        var isSelected = NativeRuntime.mc_select_inspection_element(this.session, sourcePath, line, column) != 0;
        if (isSelected) {
            this.Render();
        }
        return isSelected;
    }

    public void ApplyPreviewScenario(string page, string json) {
        if (this.scenarios.TryGetValue(page, out var previous) && previous == json) {
            return;
        }
        NativeRuntime.Ensure(NativeRuntime.mc_apply_preview_scenario(this.session, page, json) != 0);
        this.scenarios[page] = json;
        this.Render();
    }

    public void UpdateAndRender() {
        NativeRuntime.Ensure(NativeRuntime.mc_update(this.session) != 0);
        var currentPage = this.GetCurrentPage();
        if (!string.IsNullOrEmpty(currentPage) && !string.Equals(this.loadedPage, currentPage, StringComparison.Ordinal)) {
            this.loadedPage = currentPage;
            this.PageNavigated?.Invoke(currentPage);
        }
        this.Render();
    }

    public void Dispose() {
        this.image.MouseLeftButtonDown -= this.ImageMouseLeftButtonDown;
        this.image.MouseLeftButtonUp -= this.ImageMouseLeftButtonUp;
        this.image.MouseMove -= this.ImageMouseMove;
        this.image.MouseLeave -= this.ImageMouseLeave;
        if (this.hasPointerCapture) {
            this.image.ReleaseMouseCapture();
            this.hasPointerCapture = false;
        }
        if (this.session != IntPtr.Zero) {
            NativeRuntime.mc_destroy_session(this.session);
            this.session = IntPtr.Zero;
        }

        this.cursorSet.Dispose();
        this.renderer.Dispose();
    }

    private void ImageMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        var point = eventArgs.GetPosition(this.image);
        if (this.isElementInspectionEnabled) {
            if (this.TryInspect(point, out var inspection)) {
                NativeRuntime.Ensure(NativeRuntime.mc_pin_inspection_element(this.session) != 0);
                this.Render();
                this.ElementSelected?.Invoke(inspection);
            }
            eventArgs.Handled = true;
            return;
        }
        NativeRuntime.Ensure(NativeRuntime.mc_pointer_down(
            this.session,
            this.ScaleX(point.X),
            this.ScaleY(point.Y)) != 0);
        this.hasPointerCapture = this.image.CaptureMouse();
        this.SetCursor(this.CursorKind(point) switch {
            PreviewCursorKind.Tap => this.cursorSet.TapPressed,
            PreviewCursorKind.Grab => this.cursorSet.Grabbing,
            _ => null,
        });
        this.UpdateAndRender();
        eventArgs.Handled = true;
    }

    private void ImageMouseLeftButtonUp(object sender, MouseButtonEventArgs eventArgs) {
        if (!this.hasPointerCapture) {
            return;
        }
        var point = eventArgs.GetPosition(this.image);
        NativeRuntime.Ensure(NativeRuntime.mc_pointer_up(
            this.session,
            this.ScaleX(point.X),
            this.ScaleY(point.Y)) != 0);
        this.image.ReleaseMouseCapture();
        this.hasPointerCapture = false;
        this.SetCursor(this.CursorKind(point));
        this.UpdateAndRender();
        eventArgs.Handled = true;
    }

    private void ImageMouseMove(object sender, MouseEventArgs eventArgs) {
        var point = eventArgs.GetPosition(this.image);
        if (this.isElementInspectionEnabled) {
            if (this.useDefaultCursorForElementInspection) {
                this.image.Cursor = Cursors.Arrow;
            }
            this.TryInspect(point, out _);
            return;
        }
        if (!this.hasPointerCapture) {
            this.SetCursor(this.CursorKind(point));
            return;
        }
        NativeRuntime.Ensure(NativeRuntime.mc_pointer_move(
            this.session,
            this.ScaleX(point.X),
            this.ScaleY(point.Y)) != 0);
        this.UpdateAndRender();
    }

    private void ImageMouseLeave(object sender, MouseEventArgs eventArgs) {
        this.image.Cursor = null;
        NativeRuntime.Ensure(NativeRuntime.mc_clear_inspection_wireframe(this.session) != 0);
        this.Render();
    }

    private bool TryInspect(Point point, out NativeInspectionResult result) {
        if (this.image.ActualWidth <= 0.0 || this.image.ActualHeight <= 0.0) {
            result = default;
            return false;
        }
        var isInspected = NativeRuntime.mc_inspect(this.session, this.ScaleX(point.X), this.ScaleY(point.Y), out result) != 0;
        this.Render();
        return isInspected;
    }

    private string GetCurrentPage() {
        var page = new StringBuilder(128);
        NativeRuntime.Ensure(NativeRuntime.mc_current_page(this.session, page, page.Capacity) != 0);
        return page.ToString();
    }

    private void Render() {
        this.image.Source = this.renderer.RenderMobileClockSession(this.session);
    }

    private static NativeColor ParseColor(string color) {
        if (ColorConverter.ConvertFromString(color) is not Color parsedColor) {
            throw new InvalidOperationException("Не удалось разобрать цвет подсветки элемента.");
        }
        return new NativeColor {
            Red = parsedColor.R / 255.0f,
            Green = parsedColor.G / 255.0f,
            Blue = parsedColor.B / 255.0f,
            Alpha = parsedColor.A / 255.0f,
        };
    }

    private float ScaleX(double value) {
        return (float)(value / this.image.ActualWidth * this.renderer.Width);
    }

    private float ScaleY(double value) {
        return (float)(value / this.image.ActualHeight * this.renderer.Height);
    }

    private PreviewCursorKind CursorKind(Point point) {
        if (this.image.ActualWidth <= 0.0 || this.image.ActualHeight <= 0.0) {
            return PreviewCursorKind.None;
        }
        return (PreviewCursorKind)NativeRuntime.mc_cursor_kind(
            this.session,
            this.ScaleX(point.X),
            this.ScaleY(point.Y));
    }

    private void SetCursor(PreviewCursorKind kind) {
        this.SetCursor(kind switch {
            PreviewCursorKind.Tap => this.cursorSet.Tap,
            PreviewCursorKind.Grab => this.cursorSet.Grab,
            _ => null,
        });
    }

    private void SetCursor(Cursor? cursor) {
        this.image.Cursor = cursor;
    }

    private enum PreviewCursorKind {
        None,
        Tap,
        Grab,
    }
}
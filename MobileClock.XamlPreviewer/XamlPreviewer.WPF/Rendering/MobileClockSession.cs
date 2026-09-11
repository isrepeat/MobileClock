using System.Windows.Media.Imaging;
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
    private bool hasPointerCapture;
    private bool isElementInspectionEnabled;
    private bool useDefaultCursorForElementInspection;

    public event Action<NativeInspectionResult>? ElementSelected;

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
        NativeRuntime.Ensure(NativeRuntime.mc_load_page(this.session, page) != 0);
        this.Render();
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
            this.Render();
        }
    }

    public void SetElementInspectionWireframe(string color, double thickness, string lineStyle, bool renderMargin, bool renderPadding) {
        if (ColorConverter.ConvertFromString(color) is not Color parsedColor) {
            throw new InvalidOperationException("Не удалось разобрать цвет подсветки элемента.");
        }
        NativeRuntime.Ensure(NativeRuntime.mc_set_inspection_wireframe(
            this.session,
            (float)thickness,
            lineStyle == "solid" ? 0 : 1,
            new NativeColor {
                Red = parsedColor.R / 255.0f,
                Green = parsedColor.G / 255.0f,
                Blue = parsedColor.B / 255.0f,
                Alpha = parsedColor.A / 255.0f,
            },
            renderMargin ? new NativeColor { Red = 0.0f, Green = 1.0f, Blue = 0.0f, Alpha = 1.0f } : default,
            renderPadding ? new NativeColor { Red = 0.0f, Green = 0.478f, Blue = 1.0f, Alpha = 1.0f } : default) != 0);
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
        NativeRuntime.Ensure(NativeRuntime.mc_apply_preview_scenario(this.session, page, json) != 0);
        this.Render();
    }

    public void UpdateAndRender() {
        NativeRuntime.Ensure(NativeRuntime.mc_update(this.session) != 0);
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

    private void Render() {
        this.image.Source = this.renderer.RenderMobileClockSession(this.session);
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
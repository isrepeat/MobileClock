using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace XamlPreviewer;

internal sealed class PreviewSession : IDisposable {
    private readonly AnglePreviewRenderer renderer;
    private readonly Image image;
    private readonly Border inspectionOutline;
    private readonly Grid surface;
    private readonly PreviewCursorSet cursorSet;
    private IntPtr root;
    private IntPtr animations;
    private IntPtr interactions;
    private bool hasPointerCapture;
    private bool isElementInspectionEnabled;
    private bool isDisposed;
    private IReadOnlyDictionary<IntPtr, (int Line, int Column)> sourceLocations =
        new Dictionary<IntPtr, (int Line, int Column)>();

    public PreviewSession(
        IntPtr root,
        string markupDirectory,
        int width,
        int height,
        PreviewCursorSet cursorSet,
        bool startHidden = false) {
        this.root = root;
        this.cursorSet = cursorSet;
        NativeRuntime.Ensure(NativeRuntime.xr_layout(root, width, height) != 0);
        this.renderer = new AnglePreviewRenderer(markupDirectory, width, height);
        try {
            this.animations = NativeRuntime.xr_create_animation_controller();
            NativeRuntime.Ensure(this.animations != IntPtr.Zero);
            this.interactions = NativeRuntime.xr_create_interaction_controller();
            NativeRuntime.Ensure(this.interactions != IntPtr.Zero);
            if (startHidden) {
                NativeRuntime.Ensure(NativeRuntime.xr_set_attribute(root, "visibility", "Collapsed") != 0);
            }
            NativeRuntime.Ensure(NativeRuntime.xr_attach_animations(root, this.animations) != 0);
            this.image = new Image {
                Width = this.renderer.Width,
                Height = this.renderer.Height,
                Stretch = Stretch.Fill
            };
            this.inspectionOutline = new Border {
                BorderBrush = Brushes.Red,
                BorderThickness = new Thickness(2.0),
                IsHitTestVisible = false,
                Visibility = Visibility.Collapsed
            };
            this.surface = new Grid {
                Width = this.renderer.Width,
                Height = this.renderer.Height
            };
            this.surface.Children.Add(this.image);
            this.surface.Children.Add(this.inspectionOutline);
            this.image.MouseLeftButtonDown += this.ImageMouseLeftButtonDown;
            this.image.MouseLeftButtonUp += this.ImageMouseLeftButtonUp;
            this.image.MouseMove += this.ImageMouseMove;
            this.image.MouseLeave += this.ImageMouseLeave;
            this.image.MouseWheel += this.ImageMouseWheel;
            if (!startHidden) {
                this.Render();
            }
        } catch {
            if (this.animations != IntPtr.Zero) {
                NativeRuntime.xr_destroy_animation_controller(this.animations);
                this.animations = IntPtr.Zero;
            }
            this.renderer.Dispose();
            throw;
        }
    }

    public FrameworkElement Surface => this.surface;

    public event EventHandler? AnimationStarted;
    public event EventHandler<string>? Tapped;
    public event EventHandler<(IntPtr Element, string ElementId, int ItemIndex)>? Panned;
    public event EventHandler<(int Line, int Column)>? ElementSelected;

    public void SetSourceLocations(IReadOnlyDictionary<IntPtr, (int Line, int Column)> locations) {
        this.sourceLocations = locations;
    }

    public bool TryGetElementBounds(string elementId, out NativeRect bounds) {
        this.ThrowIfDisposed();
        bounds = default;
        var element = NativeRuntime.xr_find_element(this.root, elementId);
        return element != IntPtr.Zero && NativeRuntime.xr_element_bounds(element, out bounds) != 0;
    }

    public bool TryGetElementBounds(IntPtr element, out NativeRect bounds) {
        this.ThrowIfDisposed();
        bounds = default;
        return element != IntPtr.Zero && NativeRuntime.xr_element_bounds(element, out bounds) != 0;
    }

    public void RemoveItem(IntPtr target) {
        this.ThrowIfDisposed();
        NativeRuntime.Ensure(NativeRuntime.xr_items_remove_item(target) > 0);
        this.Render();
    }

    public void Transition(string from, string to, bool backward, bool visible) {
        this.ThrowIfDisposed();
        this.hasPointerCapture = false;
        this.image.ReleaseMouseCapture();
        this.inspectionOutline.Visibility = Visibility.Collapsed;
        NativeRuntime.Ensure(NativeRuntime.xr_set_page_transition(
            this.root, this.animations, from, to, backward ? 1 : 0, visible ? 1 : 0) != 0);
        this.Render();
    }

    public void SetAnimationSpeed(double value) {
        this.ThrowIfDisposed();
        NativeRuntime.Ensure(NativeRuntime.xr_set_animation_playback_rate(this.animations, (float)value) != 0);
    }

    public bool SetElementVisibility(string elementId, bool isVisible) {
        this.ThrowIfDisposed();
        var element = NativeRuntime.xr_find_element(this.root, elementId);
        if (element == IntPtr.Zero) {
            return false;
        }
        NativeRuntime.Ensure(NativeRuntime.xr_set_attribute(
            element, "visibility", isVisible ? "Visible" : "Collapsed") != 0);
        NativeRuntime.Ensure(NativeRuntime.xr_layout(this.root, this.renderer.Width, this.renderer.Height) != 0);
        this.Render();
        this.AnimationStarted?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public bool SetElementAttribute(string elementId, string name, string value) {
        this.ThrowIfDisposed();
        var element = NativeRuntime.xr_find_element(this.root, elementId);
        if (element == IntPtr.Zero) {
            return false;
        }
        NativeRuntime.Ensure(NativeRuntime.xr_set_attribute(element, name, value) != 0);
        NativeRuntime.Ensure(NativeRuntime.xr_layout(this.root, this.renderer.Width, this.renderer.Height) != 0);
        this.Render();
        return true;
    }

    public bool GoToVisualState(string scopeId, string groupName, string stateName) {
        this.ThrowIfDisposed();
        var scope = NativeRuntime.xr_find_element(this.root, scopeId);
        if (scope == IntPtr.Zero) {
            return false;
        }
        NativeRuntime.Ensure(NativeRuntime.xr_go_to_visual_state(scope, groupName, stateName, 1) != 0);
        NativeRuntime.Ensure(NativeRuntime.xr_layout(this.root, this.renderer.Width, this.renderer.Height) != 0);
        this.Render();
        this.AnimationStarted?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public void SetElementInspectionEnabled(bool value) {
        this.ThrowIfDisposed();
        if (this.isElementInspectionEnabled == value) {
            return;
        }
        this.isElementInspectionEnabled = value;
        this.image.Cursor = null;
        if (value) {
            this.hasPointerCapture = false;
            this.image.ReleaseMouseCapture();
        }
        if (!value) {
            this.inspectionOutline.Visibility = Visibility.Collapsed;
            return;
        }
        if (this.image.IsMouseOver) {
            this.UpdateInspectionOutline(Mouse.GetPosition(this.image));
        }
    }

    public bool Update() {
        this.ThrowIfDisposed();
        var isNativeAnimating = NativeRuntime.xr_update_animations(this.animations) != 0;
        var isNativeScrolling = NativeRuntime.xr_interaction_update(this.interactions) != 0;
        if (!isNativeAnimating && !isNativeScrolling) {
            return false;
        }
        NativeRuntime.Ensure(NativeRuntime.xr_layout(this.root, this.renderer.Width, this.renderer.Height) != 0);
        this.Render();
        return true;
    }

    public void Dispose() {
        if (this.isDisposed) {
            return;
        }
        this.isDisposed = true;
        this.image.MouseLeftButtonDown -= this.ImageMouseLeftButtonDown;
        this.image.MouseLeftButtonUp -= this.ImageMouseLeftButtonUp;
        this.image.MouseMove -= this.ImageMouseMove;
        this.image.MouseLeave -= this.ImageMouseLeave;
        this.image.MouseWheel -= this.ImageMouseWheel;
        if (this.interactions != IntPtr.Zero) {
            NativeRuntime.xr_destroy_interaction_controller(this.interactions);
            this.interactions = IntPtr.Zero;
        }
        if (this.animations != IntPtr.Zero) {
            NativeRuntime.xr_destroy_animation_controller(this.animations);
            this.animations = IntPtr.Zero;
        }
        if (this.root != IntPtr.Zero) {
            NativeRuntime.xr_destroy_element(this.root);
            this.root = IntPtr.Zero;
        }
        this.renderer.Dispose();
    }

    private void ImageMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        var point = eventArgs.GetPosition(this.image);
        if (this.isElementInspectionEnabled) {
            this.InspectElement(point);
            eventArgs.Handled = true;
            return;
        }
        this.hasPointerCapture = NativeRuntime.xr_interaction_pointer_down(
            this.interactions, this.root, this.animations, this.ScaleX(point.X), this.ScaleY(point.Y)) != 0;
        if (this.hasPointerCapture) {
            this.image.CaptureMouse();
            this.image.Cursor = this.GetCursorKind(point) == PreviewCursorKind.Tap
                ? this.cursorSet.TapPressed
                : null;
            this.Render();
            this.AnimationStarted?.Invoke(this, EventArgs.Empty);
            eventArgs.Handled = true;
        }
    }

    private void ImageMouseLeftButtonUp(object sender, MouseButtonEventArgs eventArgs) {
        if (this.isElementInspectionEnabled) {
            this.hasPointerCapture = false;
            this.image.ReleaseMouseCapture();
            eventArgs.Handled = true;
            return;
        }
        if (!this.hasPointerCapture) {
            return;
        }
        var point = eventArgs.GetPosition(this.image);
        NativeRuntime.Ensure(NativeRuntime.xr_interaction_pointer_up(
            this.interactions, this.root, this.animations, this.ScaleX(point.X), this.ScaleY(point.Y), out var result) != 0);
        this.hasPointerCapture = false;
        this.image.ReleaseMouseCapture();
        this.SetCursor(this.GetCursorKind(point));
        this.Render();
        this.AnimationStarted?.Invoke(this, EventArgs.Empty);
        if (result.Kind == 1 && result.Target != IntPtr.Zero) {
            this.Tapped?.Invoke(this, NativeRuntime.GetElementId(result.Target));
        }
        if (result.Kind == 2 && result.Target != IntPtr.Zero) {
            this.Panned?.Invoke(this, (result.Target, NativeRuntime.GetElementId(result.Target), result.ItemIndex));
        }
        eventArgs.Handled = true;
    }

    private void ImageMouseMove(object sender, MouseEventArgs eventArgs) {
        if (this.hasPointerCapture) {
            var pointerPoint = eventArgs.GetPosition(this.image);
            if (NativeRuntime.xr_interaction_pointer_move(
                this.interactions, this.ScaleX(pointerPoint.X), this.ScaleY(pointerPoint.Y)) != 0) {
                NativeRuntime.Ensure(NativeRuntime.xr_layout(this.root, this.renderer.Width, this.renderer.Height) != 0);
                this.Render();
            }
            return;
        }
        var point = eventArgs.GetPosition(this.image);
        this.UpdateInspectionOutline(point);
        if (this.isElementInspectionEnabled) {
            this.image.Cursor = Cursors.Cross;
            return;
        }
        this.SetCursor(this.GetCursorKind(point));
    }

    private void ImageMouseLeave(object sender, MouseEventArgs eventArgs) {
        this.image.Cursor = null;
        this.inspectionOutline.Visibility = Visibility.Collapsed;
    }

    private PreviewCursorKind GetCursorKind(Point point) {
        return (PreviewCursorKind)NativeRuntime.xr_hit_test_cursor_kind(
            this.root,
            this.ScaleX(point.X),
            this.ScaleY(point.Y));
    }

    private void SetCursor(PreviewCursorKind kind) {
        this.image.Cursor = kind switch {
            PreviewCursorKind.Tap => this.cursorSet.Tap,
            _ => null,
        };
    }

    private enum PreviewCursorKind {
        None,
        Tap,
        Grab,
    }

    private void ImageMouseWheel(object sender, MouseWheelEventArgs eventArgs) {
        var point = eventArgs.GetPosition(this.image);
        if (NativeRuntime.xr_interaction_scroll_wheel(this.interactions, this.root, this.ScaleX(point.X), this.ScaleY(point.Y), 0.0f, -eventArgs.Delta / 120.0f * 72.0f) == 0) {
            return;
        }
        NativeRuntime.Ensure(NativeRuntime.xr_layout(this.root, this.renderer.Width, this.renderer.Height) != 0);
        this.Render();
        eventArgs.Handled = true;
    }

    private void UpdateInspectionOutline(Point point) {
        if (!this.isElementInspectionEnabled || this.image.ActualWidth <= 0.0 || this.image.ActualHeight <= 0.0) {
            this.inspectionOutline.Visibility = Visibility.Collapsed;
            return;
        }
        var element = NativeRuntime.xr_hit_test_visual(
            this.root,
            this.ScaleX(point.X),
            this.ScaleY(point.Y));
        if (element == IntPtr.Zero || NativeRuntime.xr_element_bounds(element, out var bounds) == 0) {
            this.inspectionOutline.Visibility = Visibility.Collapsed;
            return;
        }
        this.inspectionOutline.Width = bounds.Width / this.renderer.Width * this.image.ActualWidth;
        this.inspectionOutline.Height = bounds.Height / this.renderer.Height * this.image.ActualHeight;
        this.inspectionOutline.HorizontalAlignment = HorizontalAlignment.Left;
        this.inspectionOutline.VerticalAlignment = VerticalAlignment.Top;
        this.inspectionOutline.Margin = new Thickness(
            bounds.X / this.renderer.Width * this.image.ActualWidth,
            bounds.Y / this.renderer.Height * this.image.ActualHeight,
            0.0,
            0.0);
        this.inspectionOutline.Visibility = Visibility.Visible;
    }

    private void InspectElement(Point point) {
        if (this.image.ActualWidth <= 0.0 || this.image.ActualHeight <= 0.0) {
            return;
        }
        var element = NativeRuntime.xr_hit_test_visual(this.root, this.ScaleX(point.X), this.ScaleY(point.Y));
        if (this.sourceLocations.TryGetValue(element, out var location)) {
            this.ElementSelected?.Invoke(this, location);
        }
    }

    private void Render() {
        this.image.Source = this.renderer.Render(this.root);
    }

    private float ScaleX(double value) {
        return (float)(value / this.image.ActualWidth * this.renderer.Width);
    }

    private float ScaleY(double value) {
        return (float)(value / this.image.ActualHeight * this.renderer.Height);
    }

    private void ThrowIfDisposed() {
        ObjectDisposedException.ThrowIf(this.isDisposed, this);
    }

}
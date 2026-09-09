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
    private readonly Image image;
    private IntPtr session;
    private bool hasPointerCapture;

    public MobileClockSession(string resourcesDirectory, int width, int height) {
        this.renderer = new AnglePreviewRenderer(resourcesDirectory, width, height);
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

    public void UpdateAndRender() {
        NativeRuntime.Ensure(NativeRuntime.mc_update(this.session) != 0);
        this.Render();
    }

    public void Dispose() {
        this.image.MouseLeftButtonDown -= this.ImageMouseLeftButtonDown;
        this.image.MouseLeftButtonUp -= this.ImageMouseLeftButtonUp;
        this.image.MouseMove -= this.ImageMouseMove;
        if (this.hasPointerCapture) {
            this.image.ReleaseMouseCapture();
            this.hasPointerCapture = false;
        }
        if (this.session != IntPtr.Zero) {
            NativeRuntime.mc_destroy_session(this.session);
            this.session = IntPtr.Zero;
        }
        this.renderer.Dispose();
    }

    private void ImageMouseLeftButtonDown(object sender, MouseButtonEventArgs eventArgs) {
        var point = eventArgs.GetPosition(this.image);
        NativeRuntime.Ensure(NativeRuntime.mc_pointer_down(
            this.session,
            this.ScaleX(point.X),
            this.ScaleY(point.Y)) != 0);
        this.hasPointerCapture = this.image.CaptureMouse();
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
        this.UpdateAndRender();
        eventArgs.Handled = true;
    }

    private void ImageMouseMove(object sender, MouseEventArgs eventArgs) {
        if (!this.hasPointerCapture) {
            return;
        }
        var point = eventArgs.GetPosition(this.image);
        NativeRuntime.Ensure(NativeRuntime.mc_pointer_move(
            this.session,
            this.ScaleX(point.X),
            this.ScaleY(point.Y)) != 0);
        this.UpdateAndRender();
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
}
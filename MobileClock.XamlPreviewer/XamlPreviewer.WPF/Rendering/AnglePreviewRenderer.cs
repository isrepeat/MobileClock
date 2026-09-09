using System.Windows.Media;
using System.Windows.Media.Imaging;

namespace XamlPreviewer;

internal sealed class AnglePreviewRenderer : IDisposable {
    private const string FontPath = @"C:\WORK\Android\Projects\MobileClock\MobileClock.Android\src\main\assets\Roboto-Regular.ttf";

    private IntPtr surface;

    public int Height { get; }
    public int Width { get; }

    public AnglePreviewRenderer(string markupDirectory, int width, int height) {
        this.Width = width;
        this.Height = height;
        this.surface = NativeRuntime.xr_create_angle_surface(
            this.Width,
            this.Height,
            AnglePreviewRenderer.FontPath,
            markupDirectory);
        NativeRuntime.Ensure(this.surface != IntPtr.Zero);
    }

    public BitmapSource Render(IntPtr root) {
        const int bytesPerPixel = 4;
        var stride = this.Width * bytesPerPixel;
        var pixels = new byte[stride * this.Height];
        NativeRuntime.Ensure(NativeRuntime.xr_render_angle_surface(
            this.surface,
            root,
            pixels,
            stride,
            pixels.Length) != 0);
        var bitmap = BitmapSource.Create(
            this.Width,
            this.Height,
            96,
            96,
            PixelFormats.Bgra32,
            null,
            pixels,
            stride);
        bitmap.Freeze();
        return bitmap;
    }

    public BitmapSource RenderMobileClockSession(IntPtr session) {
        const int bytesPerPixel = 4;
        var stride = this.Width * bytesPerPixel;
        var pixels = new byte[stride * this.Height];
        NativeRuntime.Ensure(NativeRuntime.mc_render_angle_surface(
            session,
            this.surface,
            pixels,
            stride,
            pixels.Length) != 0);
        var bitmap = BitmapSource.Create(this.Width, this.Height, 96, 96, PixelFormats.Bgra32, null, pixels, stride);
        bitmap.Freeze();
        return bitmap;
    }

    public void Dispose() {
        if (this.surface == IntPtr.Zero) {
            return;
        }
        NativeRuntime.xr_destroy_angle_surface(this.surface);
        this.surface = IntPtr.Zero;
    }
}
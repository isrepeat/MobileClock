using System.Windows.Media.Imaging;

namespace XamlPreviewer;

// Native application mode. It is intentionally separate from editable-XAML mode:
// WPF hosts the image and input only; MobileClock owns the runtime tree.
internal sealed class MobileClockSession : IDisposable {
    private readonly AnglePreviewRenderer renderer;
    private IntPtr session;

    public MobileClockSession(string resourcesDirectory, int width, int height) {
        this.renderer = new AnglePreviewRenderer(resourcesDirectory, width, height);
        this.session = NativeRuntime.mc_create_session(width, height);
        NativeRuntime.Ensure(this.session != IntPtr.Zero);
    }

    public void LoadPage(string page) => NativeRuntime.Ensure(NativeRuntime.mc_load_page(this.session, page) != 0);
    public void PointerDown(float x, float y) => NativeRuntime.Ensure(NativeRuntime.mc_pointer_down(this.session, x, y) != 0);
    public void PointerMove(float x, float y) => NativeRuntime.Ensure(NativeRuntime.mc_pointer_move(this.session, x, y) != 0);
    public void PointerUp(float x, float y) => NativeRuntime.Ensure(NativeRuntime.mc_pointer_up(this.session, x, y) != 0);
    public void CancelPointer() => NativeRuntime.Ensure(NativeRuntime.mc_pointer_cancel(this.session) != 0);

    public BitmapSource UpdateAndRender() {
        NativeRuntime.Ensure(NativeRuntime.mc_update(this.session) != 0);
        return this.renderer.RenderMobileClockSession(this.session);
    }

    public void Dispose() {
        if (this.session != IntPtr.Zero) {
            NativeRuntime.mc_destroy_session(this.session);
            this.session = IntPtr.Zero;
        }
        this.renderer.Dispose();
    }
}
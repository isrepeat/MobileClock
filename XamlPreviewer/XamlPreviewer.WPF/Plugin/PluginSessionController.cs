using System.IO;
using System.Windows;

namespace XamlPreviewer;

internal sealed class PluginSessionController : IDisposable {
    private NativePreviewSession? session;
    private string? pluginPath;

    public bool IsAvailable => this.pluginPath is not null;
    public string? PluginPath => this.pluginPath;
    public NativePreviewSession? Session => this.session;

    public void Initialize(string? path) {
        if (string.IsNullOrWhiteSpace(path)) {
            return;
        }
        NativeRuntime.ConfigurePlugin(path);
        NativeRuntime.EnsurePluginCompatibility();
        this.pluginPath = Path.GetFullPath(path);
        NativeRuntime.xr_configure_logging(Path.Combine(AppContext.BaseDirectory, "xaml-previewer.log"));
    }

    public NativePreviewSession CreateSession(string resourcesDirectory, int width, int height) {
        if (!this.IsAvailable) {
            throw new InvalidOperationException("Native plugin is not loaded.");
        }
        this.Reset();
        this.session = new NativePreviewSession(resourcesDirectory, width, height);
        return this.session;
    }

    public string? PickPlugin(Window owner) {
        var dialog = new Microsoft.Win32.OpenFileDialog {
            Filter = "XAML Previewer plugin (*.dll)|*.dll|Dynamic libraries (*.dll)|*.dll",
            Title = "Выберите native DLL приложения",
        };
        return dialog.ShowDialog(owner) == true ? dialog.FileName : null;
    }

    public void LogInfo(string message) {
        if (this.IsAvailable) {
            NativeRuntime.xr_log_info(message);
        }
    }

    public void Reset() {
        this.session?.Dispose();
        this.session = null;
    }

    public void Dispose() {
        this.Reset();
    }
}
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace NavigationGraphLab;

internal static class WindowTheme {
    private const int UseImmersiveDarkMode = 20;
    private const int UseImmersiveDarkModeBeforeWindows10Version2004 = 19;

    public static void EnableDarkTitleBar(Window window) {
        window.SourceInitialized += (_, _) => {
            var value = 1;
            var windowHandle = new WindowInteropHelper(window).Handle;
            if (WindowTheme.DwmSetWindowAttribute(
                windowHandle,
                UseImmersiveDarkMode,
                ref value,
                sizeof(int)) != 0) {
                WindowTheme.DwmSetWindowAttribute(
                    windowHandle,
                    UseImmersiveDarkModeBeforeWindows10Version2004,
                    ref value,
                    sizeof(int));
            }
        };
    }

    [DllImport("dwmapi.dll")]
    private static extern int DwmSetWindowAttribute(
        IntPtr windowHandle,
        int attribute,
        ref int value,
        int valueSize);
}
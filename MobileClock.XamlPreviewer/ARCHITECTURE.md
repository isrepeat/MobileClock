# XamlPreviewer extraction plan

`XamlPreviewer.SDK/include/XamlPreviewerPlugin.h` is the only native compile-time contract shared by a previewer and an application plugin. Its ABI is C, versioned by `xp_get_abi_version`, and all text is UTF-8 copied into caller-owned memory.

`XamlPreviewer.WPF` is the reusable desktop host. It loads a plugin selected with `--plugin <path>` or from `PreviewPluginPath` in `previewer.settings.json`. The application picker persists the path and restarts the host because Windows cannot safely unload a plugin while WPF still owns native resources.

`MobileClock.PreviewPlugin` owns MobileClock headers, static libraries and `MOBILECLOCK_XAML_PREVIEWER`. It is the project-specific native adapter that moves with MobileClock when the repositories are extracted.

The final repositories are:

- `XamlPreviewer`: `XamlPreviewer.WPF`, `XamlPreviewer.SDK`, host-owned fonts and generic launch script.
- `MobileClock`: `MobileClock.PreviewPlugin`, MobileClock-specific generation/build script and its plugin package dependencies.

The host fonts are already owned by `XamlPreviewer.WPF/Resources/Fonts`. The MobileClock launch wrapper builds its plugin and invokes the Previewer with `--plugin`.
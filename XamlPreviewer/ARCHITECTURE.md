# XamlPreviewer extraction plan

`XamlPreviewer.SDK/include/XamlPreviewerPlugin.h` is the only native compile-time contract shared by a previewer and an application plugin. Its ABI is C, versioned by `xp_get_abi_version`, and all text is UTF-8 copied into caller-owned memory. `xp_get_plugin_info` identifies the application, declares the package-relative resource root, and provides absolute paths to the source XAML root, entry markup and controls directory.

`XamlPreviewer.WPF` is the reusable desktop host. It loads a plugin selected with `--plugin <path>` or from `PreviewPluginPath` in `previewer.settings.json`. The application picker persists the path and restarts the host because Windows cannot safely unload a plugin while WPF still owns native resources.

`../MobileClock.PreviewPlugin` owns MobileClock headers, static libraries and `MOBILECLOCK_XAML_PREVIEWER`. Its build copies MobileClock resources beside the DLL as `Resources/`. The host obtains both package resources and the MobileClock source workspace solely from `xp_get_plugin_info`; it does not use saved editor paths as a fallback.

The final repositories are:

- `XamlPreviewer`: `XamlPreviewer.WPF`, `XamlPreviewer.SDK`, host-owned fonts and generic launch script.
- `MobileClock`: `MobileClock.PreviewPlugin`, MobileClock-specific generation/build script and its plugin package dependencies.

The host fonts are already owned by `XamlPreviewer.WPF/Resources/Fonts`. A plugin package consists of its selected DLL and the resource directories declared by its metadata. The MobileClock launch wrapper builds its plugin and invokes the Previewer with `--plugin`.
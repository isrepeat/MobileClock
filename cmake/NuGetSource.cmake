include_guard(GLOBAL)
# A local directory or NuGet endpoint can be supplied per machine, without
# editing tracked presets. Preserve the existing local-feed default.
set(_mobileclock_default_nuget_source "$ENV{MOBILECLOCK_NUGET_SOURCE}")
if(NOT _mobileclock_default_nuget_source)
    if(EXISTS "C:/NugetFeed")
        set(_mobileclock_default_nuget_source "C:/NugetFeed")
    else()
        set(_mobileclock_default_nuget_source "https://api.nuget.org/v3/index.json")
    endif()
endif()
set(MOBILECLOCK_NUGET_SOURCE "${_mobileclock_default_nuget_source}" CACHE STRING
    "NuGet source containing XamlRuntime and AndroidAppPreviewer.PluginSDK")
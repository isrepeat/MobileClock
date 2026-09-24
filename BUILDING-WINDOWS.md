# Сборка на Windows 11

## Необходимые инструменты

- Visual Studio 2022/2026 с MSVC x64/x86, Windows SDK и компонентом «Средства CMake C++ для Windows» (CMake + Ninja). Текущая проверка выполнена в VS 2026.
- Android NDK с Windows Clang. Проверенная версия — r27c. Другие версии могут потребовать пересборки нативных NuGet-пакетов тем же NDK.
- `nuget.exe` в PATH и доступ к пакетам `XamlRuntime` и, для Windows, `AndroidAppPreviewer.PluginSDK`.
- Инициализированные сабмодули: `git submodule update --init --recursive`.
- Для APK дополнительно нужны Android SDK и JDK, совместимые с Gradle проекта. CMake сам собирает только нативную библиотеку, не APK.

## Открытие в Visual Studio

Откройте корневую папку MobileClock через **File → Open → Folder**, выберите
**Build Windows x64 Debug** или **Build Android ARM64 Debug**, затем
**CMake Targets View**. Дождитесь `CMake generation finished` для выбранной
платформы. В Android-дереве должна появиться цель `mobileclock_android_host`.
Для сборки используйте её контекстное меню **Build mobileclock_android_host**:
автоматически найденный `UtilityHelpersLib.Nugets.sln` в списке запуска — другой проект.

В `CMakePresets.json` Android намеренно стоит первым в обоих списках:
`configurePresets` и `buildPresets`. В проверенной VS 2026 18.10.1 при первом
открытии без `.vs` список конфигураций выбирает Android, а автоматическая
генерация запускала первый configure preset из файла. Когда первым был Windows,
VS получала модель Windows, но Android-дерево оставалось в `still parsing`.
Порядок пресетов согласован с начальным выбором интерфейса: Android, затем Windows.
Не меняйте его независимо от проверки чистого запуска IDE.

Папка `.vs` не является входом сборки, её не нужно переносить с другого ПК.
XAML-код и Windows-инструмент XamlCompiler подготавливаются при конфигурации.
Установка VS определяется через `vswhere`, без привязки к Community или диску C.
Для NDK используются `clang.exe`/`clang++.exe` и Android-toolchain CMake.
Это настройка переносимости компилятора; зависание первого открытия отдельно
устраняется согласованным порядком пресетов, описанным выше.

## Локальные пути без изменений в Git

NDK автоматически ищется в `ANDROID_NDK_HOME`, `ANDROID_NDK_ROOT`, SDK из
`ANDROID_SDK_ROOT` / `ANDROID_HOME`, `%LOCALAPPDATA%/Android/Sdk/ndk` и стандартных
каталогах NDK установщика VS. Для нестандартной установки задайте
`ANDROID_NDK_HOME` до запуска VS или `CMAKE_ANDROID_NDK` в локальном пресете.
Явное `CMAKE_ANDROID_NDK` имеет приоритет.

Источник NuGet задаётся переменной окружения или CMake cache variable
`MOBILECLOCK_NUGET_SOURCE` (каталог с `.nupkg` либо URL feed).
По умолчанию используется существующий `C:/NugetFeed`, иначе nuget.org.
Наличие внутренних пакетов на nuget.org не гарантируется: на новом ПК укажите
реальный feed с ними. Уже распакованные пакеты находятся в
`Build/MobileClock/NuGetPackages`.

Для настроек только одного компьютера можно создать игнорируемый Git файл
`CMakeUserPresets.json`, унаследовать `android-arm64-debug` или `windows-x64-debug`
и задать в `cacheVariables` пути NDK и feed. Для нового configure preset добавьте
соответствующий build preset и выберите его в VS. После изменения переменных
окружения перезапустите VS; после смены NDK выполните Delete Cache and Reconfigure.

## Командная строка

Из корня репозитория:

```powershell
# Конфигурация и сборка libmobileclock.so, без APK:
./Scripts/PowerShell/build-android.ps1 -NativeOnly
# Дополнительно сборка APK через Gradle:
./Scripts/PowerShell/build-android.ps1
```

Windows-preview использует готовый AndroidAppPreviewer EXE. Его исходный WPF-проект
не должен собираться как зависимость CMake. Запуск previewer и установка APK
на устройство — отдельные проверки, не проверка дерева CMake.
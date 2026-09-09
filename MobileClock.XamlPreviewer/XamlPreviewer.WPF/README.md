# MobileClock XAML Previewer

Desktop-host для native `mobileclock::ui::ApplicationSession`. Решение Visual Studio: `XamlPreviewer.sln`; собирать нужно в `Debug|x64` или `Release|x64`.

WPF не разбирает XAML и не строит `Element`-дерево. Он отвечает только за редактор файлов, viewport, выбор страницы, настройки и преобразование координат мыши. `XamlPreviewer.NativeBridge` создаёт native-сессию, передаёт ей ввод и копирует кадр из ANGLE-поверхности в WPF `Image`. Состояние страницы, ViewModel, команды, жесты, эффекты и анимации принадлежат C++-слоям `MobileClock.Application`, `MobileClock.UI` и `MobileClock.Presentation`.

Несохранённый текст в редакторе не влияет на кадр: для отображения реальной страницы нужен сохранённый XAML и пересборка native-слоя, если изменение затрагивает скомпилированную UI-логику. JSON-сценарии, `$interactions`, `$visualStates`, WPF-инспекция элементов и WPF-переходы страниц удалены.

При первом запуске previewer создаёт только `previewer.settings.json` рядом с исполняемым файлом. В нём хранятся каталоги XAML и ресурсов, размер/ориентация экрана, масштаб, положение viewport, параметры окна и редактора.

## Сворачивание XAML

В XAML-редакторе слева от номеров строк показаны маркеры сворачивания. Состояние свёрнутых областей сохраняется для каждого файла в `previewer.settings.json`.

## Размер preview

Разрешения и скорости animation tick задаются в `previewer.settings.json`. Размер экрана и ориентация пересоздают native-сессию; скорость применяется к частоте обновления WPF-host, а внутренняя анимация остаётся native.
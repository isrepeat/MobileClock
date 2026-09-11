# Runtime Hot Reload

## Назначение

Runtime Hot Reload работает только в `XamlPreviewer`. После первоначальной сборки Previewer передаёт сохранённый UTF-8 текст XAML и путь к файлу в native bridge. Новый XAML разбирается и строится в памяти библиотекой `XamlRuntime`; `XamlCompiler` и C++-сборка при этом не запускаются.

Android-приложение продолжает использовать generated C++ XAML и не включает runtime-механизм: точки входа находятся под `MOBILECLOCK_XAML_PREVIEWER`.

Новая разметка может менять структуру поддерживаемых элементов, статические атрибуты, binding, команды, шаблоны `ListView`, storyboard и visual states. Новый C++ тип контрола, новое свойство ViewModel или новая анимация требуют обычной C++-сборки и явной регистрации.

## Путь перезагрузки

```text
Сохранение XAML
    -> WPF watcher
    -> mc_reload_markup
    -> ApplicationSession::ReloadMarkup
    -> PageManager::ReloadMarkup
    -> RuntimeReloadTransaction::Prepare
    -> RuntimeTreeBuilder
    -> IPage::ReplaceRuntimeTree
```

Watcher объединяет файловые события с задержкой 200 мс и не перезагружает неизменившийся текст. Native bridge передаёт имя страницы, текст XAML и путь файла в `ApplicationSession`.

`PageManager::ReloadMarkup` сначала разбирает XAML в AST. Затем он выбирает один из двух путей:

1. Корень `Page` означает замену полного дерева страницы.
2. Корень `UserControl` с `x:Class` означает замену шаблона существующего native-контрола, реализующего `IRuntimeReloadableControl`.

В обоих случаях подготовка выполняется до изменения видимого UI. Ошибка разбора, схемы, binding, команды, элемента или анимации возвращается вместе с путём, строкой и колонкой. Старое дерево остаётся отображаемым.

## Полная замена страницы

`RuntimeReloadTransaction::Prepare` создаёт `RuntimeBuildResult` в локальной переменной. В нём находятся новый `root` и `BindingScope` с подписками нового дерева.

Во время подготовки происходят следующие действия:

1. `XamlParser` строит AST с исходными координатами.
2. `XamlSchemaValidator` проверяет допустимые элементы и атрибуты.
3. `RuntimeTreeBuilder` создаёт новый `xaml::Element`-граф, подключает binding и команды.
4. Новому корню передаётся прежний `DataContext`.
5. Для одноимённых элементов одинакового типа восстанавливаются offset, выделение и visibility.
6. Проверяются анимации и visual states, выполняется layout.

Только после успешного завершения этих действий `PageManager` отменяет активный жест и вызывает `IPage::ReplaceRuntimeTree(std::move(result))`. Передача `result` — единственная точка, в которой новое дерево заменяет старое. Затем восстанавливается сохранённое состояние pan/scroll.

## RuntimeContext

`RuntimeTreeBuilder` универсален: он не знает `MainPageViewModel`, `Alarm` и native-контролы приложения. Для построения конкретной страницы он получает `RuntimeBindingContext` из `IPage::RuntimeContext()`.

Контекст содержит:

| Поле | Назначение |
| --- | --- |
| `bindings` | Словарь имён, разрешённых в runtime-XAML: свойств, команд и коллекций. |
| `owner` | Имя владельца для понятных diagnostics. |
| `controls` | Фабрики native-контролов. |
| `prepareTree` | Инициализация нового дерева до его отображения. |
| `beforeCommit` | Действие непосредственно перед заменой дерева. |

Простейшая страница без binding возвращает пустой registry:

```cpp
xaml::runtime::RuntimeBindingContext PageViewModel::RuntimeContext() {
    auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
    return {registry, "PageViewModel", {}};
}
```

Если XAML содержит `text="{Binding Status}"`, имя `Status` должно быть опубликовано. `RuntimeBindingPublisher` убирает повторяющуюся механику getter-а, подписки на `Property` и отписки вместе с `BindingScope`:

```cpp
auto registry = std::make_shared<xaml::runtime::RuntimeBindingRegistry>();
xaml::runtime::RuntimeBindingPublisher publisher{*registry, *this};

publisher.Text("Status", Property::status, &MainPageViewModel::Status);
publisher.Command("CreateAlarmCommand", &MainPageViewModel::CreateAlarmCommand);
publisher.Boolean("IsAlarmActionsMenuVisible", Property::isAlarmActionsMenuVisible,
    &MainPageViewModel::IsAlarmActionsMenuVisible,
    &MainPageViewModel::SetIsAlarmActionsMenuVisible);
```

Эти строки позволяют использовать в XAML:

```xml
<TextBlock text="{Binding Status}" />
<Button command="{Binding CreateAlarmCommand}" />
<ToggleSwitch isOn="{Binding IsAlarmActionsMenuVisible, Mode=TwoWay}" />
```

`Text` вызывает getter при создании дерева и при соответствующем `NotifyPropertyChanged`. `Command` назначает обработчик кнопке. `Boolean` добавляет getter, подписку и, при переданном setter-е, обратную запись от `ToggleSwitch` в ViewModel.

Registry остаётся явным allow-list. Runtime-XAML не может обратиться к непубликованному полю или методу ViewModel; ошибка показывает имя владельца и список разрешённых binding.

## Коллекции и ItemTemplate

Для `ListView` одного registry страницы недостаточно. Разметка ниже использует два контекста binding:

```xml
<ListView itemsSource="{Binding Alarms}">
    <ListView.ItemTemplate>
        <DataTemplate>
            <TextBlock text="{Binding Time}" />
            <ToggleSwitch isOn="{Binding IsEnabled, Mode=TwoWay}" />
        </DataTemplate>
    </ListView.ItemTemplate>
</ListView>
```

`Alarms` принадлежит `MainPageViewModel`, но `Time` и `IsEnabled` принадлежат конкретному `Alarm`. Поэтому `RuntimeCollectionDescriptor` задаёт два действия:

1. `itemBindings` создаёт registry строки по адресу конкретного `Alarm`. Runtime также вызывает его с `nullptr` для проверки `ItemTemplate` у пустой коллекции.
2. `bind` получает построенный `ItemTemplate` и вызывает `element.SetItemsSource(this->alarms, std::move(itemTemplate))`.

После `registry->AddCollection("Alarms", collection)` runtime встречает `{Binding Alarms}`, берёт descriptor и вызывает `bind`. `SetItemsSource` хранит коллекцию, сам получает её размер и элементы, подписывается на изменения `ObservableCollection` и создаёт или удаляет строки. Поля `RuntimeCollectionDescriptor::count` и `at` пока не используются текущим `RuntimeTreeBuilder`.

## Native-контролы и замена шаблонов

`result.controls` содержит фабрики известных C++-контролов:

```cpp
result.controls["InteractiveList"] = [this](xaml::BindingScope& scope) {
    return controls::InteractiveList::Create(*this, this->alarms, scope);
};
```

При встрече `InteractiveList` runtime вызывает фабрику, а не создаёт обычный `xaml::Element`. Это сохраняет логику жестов и внутреннее состояние контрола.

`InteractiveList.xaml` и `TimelineTabs.xaml` имеют корень `UserControl`. `PageManager` находит уже существующий native-контрол по `x:Class` и вызывает `ReplaceTemplate`. Контрол строит новый шаблон, проверяет обязательные элементы, переносит scroll/pan и меняет только внутреннее content-дерево. Сам C++-экземпляр остаётся прежним.

В шаблоне `InteractiveList` должны сохраняться контракты `interactiveListScrollViewer`, `interactiveListItems` и `interactiveListGestureTarget`.

## prepareTree и состояние UI

`prepareTree` синхронизирует новое дерево с состоянием уже работающей ViewModel. Например, после замены страницы нужно сразу вернуть панели действий будильника корректный visual state:

```cpp
result.prepareTree = [this](xaml::Element& root) {
    if (auto* host = _details::FindElement(root, "alarmActionsHost")) {
        xaml::VisualStateManager::GoToState(*host, "AlarmActionsPanelStates",
            this->IsAlarmActionsMenuVisible() ? "Expanded" : "Collapsed", false);
    }
};
```

Последний аргумент `false` устанавливает состояние без анимации, поэтому Hot Reload не проигрывает переход при каждом сохранении файла.

## Использование и проверка

1. Один раз соберите и запустите XamlPreviewer.
2. Выберите страницу приложения.
3. Измените и сохраните страницу либо используемый `UserControl`.
4. Проверьте результат в Previewer. Если разметка невалидна, исправьте diagnostics: предыдущий рабочий UI остаётся доступным.

Desktop Debug CMake-конфигурация содержит цель `RuntimeMarkupTests`; тест запускается через CTest с именем `RuntimeMarkup`. Он проверяет rollback, подписки, коллекции, пустой item context, two-way boolean, scroll и перенос pan при замене шаблона.
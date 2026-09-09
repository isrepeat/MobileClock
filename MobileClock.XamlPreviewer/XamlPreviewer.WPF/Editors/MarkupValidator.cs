using System.IO;
using System.Xml;
using System.Xml.Linq;

namespace XamlPreviewer;

internal static class MarkupValidator {
    public const string Namespace = "urn:mobileclock:xaml";
    public const string ControlsNamespace = "using:mobileclock.ui.controls";

    public static void Validate(XDocument document) {
        var root = document.Root
            ?? throw new InvalidDataException("Разметка не содержит корневого элемента.");
        if (root.Name.LocalName != "Page") {
            throw new InvalidDataException(
                "Рендер поддерживает только страницы с корневым элементом <Page>.");
        }
        if (root.Name.NamespaceName != MarkupValidator.Namespace) {
            throw new InvalidDataException(
                $"Страница должна использовать xmlns=\"{MarkupValidator.Namespace}\".");
        }
        foreach (var element in root.DescendantsAndSelf()) {
            if (element.Name.NamespaceName != MarkupValidator.Namespace
                && element.Name.NamespaceName != MarkupValidator.ControlsNamespace) {
                throw new InvalidDataException(
                    "Элементы разметки должны использовать namespace MobileClock XAML или namespace пользовательских контролов.");
            }
        }
        foreach (var textNode in document.DescendantNodes().OfType<XText>()) {
            if (!string.IsNullOrWhiteSpace(textNode.Value)) {
                MarkupValidator.ThrowInvalidTextNode(textNode);
            }
        }
    }

    private static void ThrowInvalidTextNode(XText textNode) {
        var lineInfo = (IXmlLineInfo)textNode;
        var location = lineInfo.HasLineInfo()
            ? $"Строка {lineInfo.LineNumber}, позиция {lineInfo.LinePosition}."
            : "Расположение в документе не определено.";
        throw new InvalidDataException($"Недопустимый текст вне элемента. {location}");
    }
}
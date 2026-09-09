using System.IO;
using System.Text.Json.Nodes;

namespace XamlPreviewer;

internal static class ScenarioInteraction {
    public static JsonObject? GetTap(JsonObject scenario, string elementId) {
        if (scenario["$interactions"] is not JsonObject interactions
            || interactions[elementId] is not JsonObject element
            || element["tap"] is not JsonObject tap) {
            return null;
        }
        return tap;
    }

    public static bool HandleTap(JsonObject scenario, string elementId) {
        var tap = ScenarioInteraction.GetTap(scenario, elementId);
        if (tap is null) {
            return false;
        }
        var type = tap["type"]?.GetValue<string>();
        var path = tap["path"]?.GetValue<string>();
        if (string.IsNullOrWhiteSpace(type) || string.IsNullOrWhiteSpace(path)) {
            throw new InvalidDataException("Обработчик tap требует type и path.");
        }
        switch (type) {
        case "append":
            if (ScenarioInteraction.GetValue(scenario, path) is not JsonArray collection) {
                throw new InvalidDataException($"Свойство {path} должно быть массивом для append.");
            }
            collection.Add(tap["value"]?.DeepClone());
            return true;
        case "set":
            if (!tap.TryGetPropertyValue("value", out var value)) {
                throw new InvalidDataException("Обработчик set требует value.");
            }
            ScenarioInteraction.SetValue(scenario, path, value?.DeepClone());
            return true;
        case "toggle":
            var target = ScenarioInteraction.GetValue(scenario, path);
            if (target is not JsonValue jsonValue || !jsonValue.TryGetValue<bool>(out var current)) {
                throw new InvalidDataException($"Свойство {path} должно быть boolean для toggle.");
            }
            ScenarioInteraction.SetValue(scenario, path, JsonValue.Create(!current));
            return true;
        default:
            throw new InvalidDataException($"Неподдерживаемый тип обработчика tap: {type}.");
        }
    }

    public static bool HandlePan(JsonObject scenario, string elementId, int itemIndex) {
        if (scenario["$interactions"] is not JsonObject interactions
            || interactions[elementId] is not JsonObject element
            || element["pan"] is not JsonObject pan) {
            return false;
        }
        var type = pan["type"]?.GetValue<string>();
        var path = pan["path"]?.GetValue<string>();
        if (type != "removeAt" || string.IsNullOrWhiteSpace(path)) {
            throw new InvalidDataException("Обработчик pan требует type=removeAt и path.");
        }
        if (ScenarioInteraction.GetValue(scenario, path) is not JsonArray collection
            || itemIndex < 0 || itemIndex >= collection.Count) {
            return false;
        }
        collection.RemoveAt(itemIndex);
        return true;
    }

    private static JsonNode? GetValue(JsonObject scenario, string path) {
        JsonObject current = scenario;
        var segments = ScenarioInteraction.ParsePath(path);
        for (var index = 0; index < segments.Length - 1; ++index) {
            current = current[segments[index]] as JsonObject
                ?? throw new InvalidDataException($"Путь обработчика не найден: {path}.");
        }
        return current[segments[^1]];
    }

    private static void SetValue(JsonObject scenario, string path, JsonNode? value) {
        JsonObject current = scenario;
        var segments = ScenarioInteraction.ParsePath(path);
        for (var index = 0; index < segments.Length - 1; ++index) {
            current = current[segments[index]] as JsonObject
                ?? throw new InvalidDataException($"Путь обработчика не найден: {path}.");
        }
        current[segments[^1]] = value;
    }

    private static string[] ParsePath(string path) {
        var segments = path.Split('.', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        if (segments.Length == 0) {
            throw new InvalidDataException("Путь обработчика не может быть пустым.");
        }
        return segments;
    }
}
using System.Text.Json.Nodes;

namespace XamlPreviewer;

internal sealed class PreviewGestureController {
    private readonly Func<JsonObject> getScenarioRoot;
    private readonly Func<JsonObject, JsonObject> getSelectedScenario;
    private readonly Action<JsonObject, bool> saveChanges;

    public PreviewGestureController(
        Func<JsonObject> getScenarioRoot,
        Func<JsonObject, JsonObject> getSelectedScenario,
        Action<JsonObject, bool> saveChanges) {
        this.getScenarioRoot = getScenarioRoot;
        this.getSelectedScenario = getSelectedScenario;
        this.saveChanges = saveChanges;
    }

    public bool HandleTap(string elementId) {
        var root = this.getScenarioRoot();
        var scenario = this.getSelectedScenario(root);
        if (!ScenarioInteraction.HandleTap(scenario, elementId)) {
            return false;
        }
        this.saveChanges(root, true);
        return true;
    }

    public bool HandlePan(string elementId, int itemIndex) {
        var root = this.getScenarioRoot();
        var scenario = this.getSelectedScenario(root);
        if (!ScenarioInteraction.HandlePan(scenario, elementId, itemIndex)) {
            return false;
        }
        this.saveChanges(root, false);
        return true;
    }

}
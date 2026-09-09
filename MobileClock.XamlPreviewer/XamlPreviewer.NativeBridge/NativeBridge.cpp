#include <Helpers.Logging/Logging.h>
#include <HelpersNew/Geometry/ContainsPoint.h>

#include <XamlRuntime/InteractionController.h>
#include <XamlRuntime/ScrollController.h>
#include <XamlRuntime/ElementBuilder.h>
#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "../../MobileClock.Presentation/PreviewSession.h"
#include "../../MobileClock.Application/UI/ApplicationSession.h"
#include "AngleRenderSurface.h"
#include "NativeBridge.h"

#include <unordered_map>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <cmath>

namespace xaml::bridge::_details {
    Element* FindElement(Element& element, std::string_view id) {
        if (element.Id() == id) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (Element* const found = FindElement(*child, id)) {
                return found;
            }
        }
        return nullptr;
    }

    int CountElements(const Element& element, std::string_view id) {
        int count = element.Id() == id ? 1 : 0;
        for (const auto& child : element.Children()) {
            count += CountElements(*child, id);
        }
        return count;
    }

    Element* FindElementAt(Element& element, std::string_view id, int& index) {
        if (element.Id() == id) {
            if (index == 0) {
                return &element;
            }
            --index;
        }
        for (const auto& child : element.Children()) {
            if (Element* const found = FindElementAt(*child, id, index)) {
                return found;
            }
        }
        return nullptr;
    }

    // Нужен previewer-у для выбора любого видимого элемента под указателем.
    // В отличие от xaml::HitTest не требует, чтобы элемент был enabled или interactive.
    Element* HitTestVisual(Element& element, float x, float y, float offsetX = 0.0f, float offsetY = 0.0f) {
        const Rect& clipBounds = element.ClipBounds();

        if (element.VisibilityValue() != attr::Visibility::visible
            || !utility_helpers::new_helpers::geometry::ContainsPoint(
                clipBounds.x,
                clipBounds.y,
                clipBounds.width,
                clipBounds.height,
                x - offsetX,
                y - offsetY)) {
            return nullptr;
        }

        const auto& children = element.Children();
        const float childrenOffsetX = element.Type() == ElementType::scrollViewer
            ? offsetX - element.HorizontalOffset() : offsetX;
        const float childrenOffsetY = element.Type() == ElementType::scrollViewer
            ? offsetY - element.VerticalOffset() : offsetY;

        for (auto child = children.rbegin(); child != children.rend(); ++child) {
            if (Element* const hit = xaml::bridge::_details::HitTestVisual(**child, x, y, childrenOffsetX, childrenOffsetY)) {
                return hit;
            }
        }

        const Rect& bounds = element.Bounds();
        return utility_helpers::new_helpers::geometry::ContainsPoint(
            bounds.x,
            bounds.y,
            bounds.width,
            bounds.height,
            x - offsetX,
            y - offsetY) ? &element : nullptr;
    }

    Element* FindScrollViewer(Element& root, float x, float y) {
        Element* element = xaml::bridge::_details::HitTestVisual(root, x, y);
        while (element != nullptr && element->Type() != ElementType::scrollViewer) {
            element = element->Parent();
        }
        return element;
    }

    bool RemoveItem(Element& target) {
        Element* item = &target;
        for (Element* parent = item->Parent(); parent != nullptr; parent = parent->Parent()) {
            if (parent->Type() == ElementType::listView) {
                parent->RemoveChildImmediately(*item);
                return true;
            }
            item = parent;
        }
        return false;
    }

}

namespace xaml::bridge {
    class RecordingBackend final : public IRenderBackend {
    public:
        //
        // IRenderBackend
        //
        void BeginClip(const Rect& bounds) override {
            this->Append(xr_command_type_begin_clip, bounds);
        }

        void EndClip() override {
            this->Append(xr_command_type_end_clip, {});
        }

        void DrawOutline(const Rect& bounds, attr::Color color) override {
            this->Append(xr_command_type_outline, bounds, color);
        }

        void DrawRoundedRect(const Rect& bounds, attr::Color color, float cornerRadius) override {
            this->Append(xr_command_type_rounded_rect, bounds, color, cornerRadius);
        }

        void DrawRoundedRectOutline(
            const Rect& bounds,
            attr::Color color,
            float cornerRadius,
            float thickness) override {
            xr_command& command = this->Append(
                xr_command_type_rounded_rect_outline,
                bounds,
                color,
                cornerRadius);
            std::memcpy(command.auxiliary, &thickness, sizeof(thickness));
        }

        void DrawShader(
            std::string_view,
            const Rect&,
            std::initializer_list<ShaderUniform>) override {
        }

        void DrawText(
            const Rect& bounds,
            std::string_view text,
            attr::Color color,
            float fontSize,
            std::string_view fontWeight,
            attr::Alignment) override {
            xr_command& command = this->Append(xr_command_type_text, bounds, color, fontSize);
            Copy(text, command.text, sizeof(command.text));
            Copy(fontWeight, command.auxiliary, sizeof(command.auxiliary));
        }

        void DrawImage(
            const Rect& bounds,
            std::string_view source,
            attr::Color tint) override {
            xr_command& command = this->Append(xr_command_type_image, bounds, tint);
            Copy(source, command.text, sizeof(command.text));
        }

        const std::vector<xr_command>& Commands() const {
            return this->commands;
        }

    private:
        xr_command& Append(
            xr_command_type type,
            const Rect& bounds,
            attr::Color color = {},
            float value = 0.0f) {
            this->commands.push_back({
                static_cast<int>(type),
                {bounds.x, bounds.y, bounds.width, bounds.height},
                {color.red, color.green, color.blue, color.alpha},
                value,
            });
            return this->commands.back();
        }

        static void Copy(std::string_view source, char* destination, size_t capacity) {
            const size_t length = std::min(source.size(), capacity - 1);
            std::memcpy(destination, source.data(), length);
            destination[length] = '\0';
        }

    private:
        std::vector<xr_command> commands;
    };

    thread_local std::string lastError;
}

struct xr_animation_controller {
    mobileclock::presentation::PreviewSession value;
    xaml::ScrollController scrollController;
};

struct xr_interaction_controller {
    xaml::InteractionController value;
};

struct xr_angle_surface {
    explicit xr_angle_surface(
        int width,
        int height,
        const char* fontPath,
        const char* resourceRoot)
        : width(width)
        , height(height)
        , value(width, height, fontPath, resourceRoot) {
    }

    int width;
    int height;
    xaml::bridge::AngleRenderSurface value;
};

namespace mobileclock::preview::_details {
    class PreviewApplicationActions final : public ui::IApplicationActions {
    public:
        void CreateAlarm() override {
        }

        void ToggleAlarm() override {
        }

        void UpdateApplication() override {
        }

        void UploadScreenshot() override {
        }

        void ShareLogs() override {
        }

        void ExportLogs() override {
        }
    };
}

struct mc_session {
    explicit mc_session(int width, int height)
        : session(actions)
        , width(width)
        , height(height) {
        if (width <= 0 || height <= 0) {
            throw std::invalid_argument("Session dimensions must be positive");
        }
        this->session.Initialize({static_cast<float>(width), static_cast<float>(height)});
    }

    mobileclock::preview::_details::PreviewApplicationActions actions;
    mobileclock::ui::ApplicationSession session;
    int width;
    int height;
};

const char* xr_last_error(void) {
    return xaml::bridge::lastError.c_str();
}

mc_session* mc_create_session(int width, int height) {
    try {
        xaml::bridge::lastError.clear();
        return new mc_session(width, height);
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

void mc_destroy_session(mc_session* session) {
    delete session;
}

int mc_load_page(mc_session* session, const char* page) {
    try {
        xaml::bridge::lastError.clear();
        if (session == nullptr || page == nullptr || !session->session.LoadPage(page)) {
            throw std::invalid_argument("Unknown MobileClock page");
        }
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int mc_resize(mc_session* session, int width, int height) {
    try {
        xaml::bridge::lastError.clear();
        if (session == nullptr || width <= 0 || height <= 0) {
            throw std::invalid_argument("Session and positive dimensions are required");
        }
        session->width = width;
        session->height = height;
        session->session.Initialize({static_cast<float>(width), static_cast<float>(height)});
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int mc_set_status(mc_session* session, const char* value) {
    try {
        xaml::bridge::lastError.clear();
        if (session == nullptr || value == nullptr) {
            throw std::invalid_argument("Session and status are required");
        }
        session->session.SetStatus(value);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int mc_pointer_down(mc_session* session, float x, float y) {
    if (session == nullptr) {
        return 0;
    }
    session->session.PointerDown(x, y);
    return 1;
}

int mc_pointer_move(mc_session* session, float x, float y) {
    if (session == nullptr) {
        return 0;
    }
    session->session.PointerMove(x, y);
    return 1;
}

int mc_pointer_up(mc_session* session, float x, float y) {
    if (session == nullptr) {
        return 0;
    }
    session->session.PointerUp(x, y);
    return 1;
}

int mc_pointer_cancel(mc_session* session) {
    if (session == nullptr) {
        return 0;
    }
    session->session.CancelPointer();
    return 1;
}

int mc_cursor_kind(mc_session* session, float x, float y) {
    if (session == nullptr) {
        return 0;
    }
    return session->session.CursorKind(x, y);
}

int mc_update(mc_session* session) {
    if (session == nullptr) {
        return 0;
    }
    session->session.Update();
    return 1;
}

int mc_render_angle_surface(
    mc_session* session,
    xr_angle_surface* surface,
    unsigned char* destination,
    int destinationStride,
    int destinationCapacity) {
    try {
        xaml::bridge::lastError.clear();
        if (session == nullptr || surface == nullptr || destination == nullptr
            || destinationStride < surface->width * 4
            || destinationCapacity / destinationStride < surface->height) {
            throw std::invalid_argument("Invalid MobileClock ANGLE render arguments");
        }
        surface->value.Render(session->session, destination, destinationStride);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

void xr_configure_logging(const char* filePath) {
    utility_helpers::logging::Configure({
        filePath == nullptr ? std::filesystem::path{} : std::filesystem::path(filePath),
    });
    utility_helpers::logging::Initialize("XamlPreviewer");
    LOG_INFO("XamlPreviewer.NativeBridge", "Logging initialized");
}

void xr_log_info(const char* message) {
    if (message != nullptr) {
        LOG_INFO("XamlPreviewer.Interaction", "{}", message);
    }
}

xr_element* xr_create_element(const char* type) {
    try {
        xaml::bridge::lastError.clear();
        return reinterpret_cast<xr_element*>(new xaml::Element(xaml::ParseElementType(type)));
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

void xr_destroy_element(xr_element* element) {
    delete reinterpret_cast<xaml::Element*>(element);
}

int xr_items_remove_item(xr_element* target) {
    try {
        xaml::bridge::lastError.clear();
        if (target == nullptr) {
            throw std::invalid_argument("target is required");
        }
        return xaml::bridge::_details::RemoveItem(*reinterpret_cast<xaml::Element*>(target)) ? 1 : 0;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return -1;
    }
}

int xr_add_child(xr_element* parent, xr_element* child) {
    try {
        xaml::bridge::lastError.clear();
        if (parent == nullptr || child == nullptr) {
            throw std::invalid_argument("parent and child are required");
        }
        xaml::ValidateChild(
            *reinterpret_cast<const xaml::Element*>(parent),
            *reinterpret_cast<const xaml::Element*>(child));
        reinterpret_cast<xaml::Element*>(parent)->AddChild(
            std::unique_ptr<xaml::Element>(reinterpret_cast<xaml::Element*>(child)));
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_set_attribute(
    xr_element* element,
    const char* name,
    const char* value) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr) {
            throw std::invalid_argument("element is required");
        }
        xaml::SetAttribute(*reinterpret_cast<xaml::Element*>(element), name, value);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

xr_element* xr_find_element(xr_element* root, const char* id) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || id == nullptr || *id == '\0') {
            throw std::invalid_argument("root and id are required");
        }
        return reinterpret_cast<xr_element*>(xaml::bridge::_details::FindElement(
            *reinterpret_cast<xaml::Element*>(root), id));
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

int xr_find_element_count(xr_element* root, const char* id) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || id == nullptr || *id == '\0') {
            throw std::invalid_argument("root and id are required");
        }
        return xaml::bridge::_details::CountElements(*reinterpret_cast<xaml::Element*>(root), id);
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return -1;
    }
}

xr_element* xr_find_element_at(xr_element* root, const char* id, int index) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || id == nullptr || *id == '\0' || index < 0) {
            throw std::invalid_argument("root, id and non-negative index are required");
        }
        return reinterpret_cast<xr_element*>(xaml::bridge::_details::FindElementAt(
            *reinterpret_cast<xaml::Element*>(root), id, index));
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

int xr_add_storyboard_animation(xr_element* element, int trigger, const char* name,
    const char* const* keys, const char* const* values, int count) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || trigger < 0 || trigger > 6 || count < 0
            || (count > 0 && (keys == nullptr || values == nullptr))) {
            throw std::invalid_argument("invalid storyboard animation");
        }
        xaml::Storyboard storyboard;
        storyboard.trigger = static_cast<xaml::AnimationTrigger>(trigger);
        if (name != nullptr) {
            if (*name == '\\0') {
                throw std::invalid_argument("animation name is required");
            }
            xaml::AnimationTrack track;
            track.name = name;
            for (int index = 0; index < count; ++index) {
                if (keys[index] == nullptr || values[index] == nullptr) {
                    throw std::invalid_argument("animation settings are required");
                }
                track.settings.Set(keys[index], values[index]);
            }
            storyboard.tracks.push_back(std::move(track));
        }
        reinterpret_cast<xaml::Element*>(element)->AddStoryboard(std::move(storyboard));
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_attach_animations(xr_element* root, xr_animation_controller* animations) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr) {
            throw std::invalid_argument("root is required");
        }
        if (animations == nullptr) {
            throw std::invalid_argument("animations is required");
        }
        animations->value.Attach(*reinterpret_cast<xaml::Element*>(root));
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_set_page_transition(
    xr_element* root,
    xr_animation_controller* animations,
    const char* from,
    const char* to,
    int backward,
    int visible) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || animations == nullptr || from == nullptr || to == nullptr) {
            throw std::invalid_argument("root, animations, from and to are required");
        }
        animations->value.SetPageTransition(
            *reinterpret_cast<xaml::Element*>(root),
            from,
            to,
            backward != 0,
            visible != 0);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_add_storyboard_track(
    xr_element* element,
    int trigger,
    int property,
    float from,
    float to,
    int durationMilliseconds,
    int easing) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || trigger < 0 || trigger > 6 || property < 0 || property > 5
            || durationMilliseconds < 0 || easing < 0 || easing > 1) {
            throw std::invalid_argument("invalid storyboard track");
        }
        xaml::Storyboard storyboard;
        storyboard.trigger = static_cast<xaml::AnimationTrigger>(trigger);
        storyboard.tracks.push_back({
            static_cast<xaml::AnimatedProperty>(property),
            from,
            to,
            std::isnan(from),
            std::isnan(to),
            std::chrono::milliseconds(durationMilliseconds),
            static_cast<xaml::Easing>(easing),
        });
        reinterpret_cast<xaml::Element*>(element)->AddStoryboard(std::move(storyboard));
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_add_visual_state_track(
    xr_element* scope,
    const char* groupName,
    const char* stateName,
    const char* targetName,
    int property,
    float from,
    float to,
    int durationMilliseconds,
    int easing) {
    try {
        xaml::bridge::lastError.clear();
        if (scope == nullptr || groupName == nullptr || stateName == nullptr || targetName == nullptr
            || *groupName == '\0' || *stateName == '\0' || *targetName == '\0'
            || property < 0 || property > 5 || durationMilliseconds < 0 || easing < 0 || easing > 1) {
            throw std::invalid_argument("invalid visual state track");
        }
        auto& groups = reinterpret_cast<xaml::Element*>(scope)->VisualStateGroups();
        auto group = std::find_if(groups.begin(), groups.end(), [groupName](const xaml::VisualStateGroup& value) {
            return value.name == groupName;
        });
        if (group == groups.end()) {
            groups.push_back({groupName});
            group = std::prev(groups.end());
        }
        auto state = std::find_if(group->states.begin(), group->states.end(), [stateName](const xaml::VisualState& value) {
            return value.name == stateName;
        });
        if (state == group->states.end()) {
            group->states.push_back({stateName});
            state = std::prev(group->states.end());
        }
        state->tracks.push_back({targetName, {
            static_cast<xaml::AnimatedProperty>(property), from, to, std::isnan(from), std::isnan(to),
            std::chrono::milliseconds(durationMilliseconds), static_cast<xaml::Easing>(easing)}});
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_go_to_visual_state(
    xr_element* scope,
    const char* groupName,
    const char* stateName,
    int useTransitions) {
    try {
        xaml::bridge::lastError.clear();
        if (scope == nullptr || groupName == nullptr || stateName == nullptr) {
            throw std::invalid_argument("scope, groupName and stateName are required");
        }
        return xaml::VisualStateManager::GoToState(*reinterpret_cast<xaml::Element*>(scope),
            groupName, stateName, useTransitions != 0) ? 1 : 0;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_supported_attribute_count(const char* elementType) {
    try {
        xaml::bridge::lastError.clear();
        if (elementType == nullptr) {
            throw std::invalid_argument("element type is required");
        }
        if (std::string_view(elementType) == "columnDefinition" || std::string_view(elementType) == "rowDefinition") {
            return 1;
        }
        return static_cast<int>(xaml::SupportedAttributeNames(xaml::ParseElementType(elementType)).size());
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

const char* xr_supported_attribute_name(
    const char* elementType,
    int index) {
    try {
        xaml::bridge::lastError.clear();
        if (elementType == nullptr || index < 0) {
            throw std::invalid_argument("element type and non-negative index are required");
        }
        if (std::string_view(elementType) == "columnDefinition" && index == 0) {
            return "width";
        }
        if (std::string_view(elementType) == "rowDefinition" && index == 0) {
            return "height";
        }
        const std::vector<std::string_view> names = xaml::SupportedAttributeNames(
            xaml::ParseElementType(elementType));
        if (index >= static_cast<int>(names.size())) {
            throw std::out_of_range("attribute index is out of range");
        }
        return names[static_cast<size_t>(index)].data();
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return "";
    }
}

int xr_supported_element_count(void) {
    return 18;
}

const char* xr_supported_element_name(int index) {
    static constexpr std::string_view names[]{
        "Page", "StackPanel", "Grid", "Border", "TextBlock", "Button", "IconButton", "ToggleSwitch",
        "ScrollViewer", "Image", "SvgImage", "ListView", "ListView.ItemTemplate", "DataTemplate",
        "columnDefinitions", "columnDefinition", "rowDefinitions", "rowDefinition"
    };
    return index >= 0 && index < static_cast<int>(std::size(names)) ? names[index].data() : "";
}

int xr_layout(xr_element* root, float width, float height) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr) {
            throw std::invalid_argument("root is required");
        }
        xaml::layout(*reinterpret_cast<xaml::Element*>(root), {width, height});
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

xr_element* xr_hit_test(xr_element* root, float x, float y) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr) {
            throw std::invalid_argument("root is required");
        }
        return reinterpret_cast<xr_element*>(xaml::HitTest(
            *reinterpret_cast<xaml::Element*>(root), x, y));
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

// Возвращает previewer-у любой видимый элемент под указателем,
// включая элементы, которые не являются enabled или interactive.
xr_element* xr_hit_test_visual(xr_element* root, float x, float y) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr) {
            throw std::invalid_argument("root is required");
        }
        return reinterpret_cast<xr_element*>(xaml::bridge::_details::HitTestVisual(
            *reinterpret_cast<xaml::Element*>(root), x, y));
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

int xr_hit_test_cursor_kind(xr_element* root, float x, float y) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr) {
            throw std::invalid_argument("root is required");
        }
        auto& nativeRoot = *reinterpret_cast<xaml::Element*>(root);
        xaml::Element* const visual = xaml::bridge::_details::HitTestVisual(nativeRoot, x, y);
        if (visual == nullptr) {
            return 0;
        }
        xaml::Element* const interactive = xaml::HitTest(nativeRoot, x, y);
        if (interactive != nullptr && interactive->Type() != xaml::ElementType::scrollViewer) {
            return 1;
        }
        for (xaml::Element* element = visual; element != nullptr; element = element->Parent()) {
            if (element->Type() == xaml::ElementType::scrollViewer) {
                return 2;
            }
        }
        return 0;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

// Копирует рассчитанные layout-границы элемента в структуру C bridge.
int xr_element_bounds(const xr_element* element, xr_rect* bounds) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || bounds == nullptr) {
            throw std::invalid_argument("element and bounds are required");
        }
        const xaml::Rect elementBounds = reinterpret_cast<const xaml::Element*>(element)->Bounds();
        *bounds = {elementBounds.x, elementBounds.y, elementBounds.width, elementBounds.height};
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_get_scroll_offsets(
    xr_element* root,
    const char* scrollViewerId,
    float* horizontalOffset,
    float* verticalOffset) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || scrollViewerId == nullptr || *scrollViewerId == '\0'
            || horizontalOffset == nullptr || verticalOffset == nullptr) {
            throw std::invalid_argument("root, scroll viewer id and offsets are required");
        }
        xaml::Element* const scrollViewer = xaml::bridge::_details::FindElement(
            *reinterpret_cast<xaml::Element*>(root), scrollViewerId);
        if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
            return 0;
        }
        *horizontalOffset = scrollViewer->HorizontalOffset();
        *verticalOffset = scrollViewer->VerticalOffset();
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_set_scroll_offsets(
    xr_element* root,
    const char* scrollViewerId,
    float horizontalOffset,
    float verticalOffset) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || scrollViewerId == nullptr || *scrollViewerId == '\0'
            || !std::isfinite(horizontalOffset) || !std::isfinite(verticalOffset)) {
            throw std::invalid_argument("root, scroll viewer id and finite offsets are required");
        }
        xaml::Element* const scrollViewer = xaml::bridge::_details::FindElement(
            *reinterpret_cast<xaml::Element*>(root), scrollViewerId);
        if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
            return 0;
        }
        scrollViewer->SetHorizontalOffset(horizontalOffset);
        scrollViewer->SetVerticalOffset(verticalOffset);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_scroll_by(xr_element* root, float x, float y, float horizontalDelta, float verticalDelta) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || !std::isfinite(horizontalDelta) || !std::isfinite(verticalDelta)) {
            throw std::invalid_argument("root and finite deltas are required");
        }
        xaml::Element* element = xaml::bridge::_details::FindScrollViewer(
            *reinterpret_cast<xaml::Element*>(root), x, y);
        if (element == nullptr) {
            return 0;
        }
        const float horizontalOffset = std::clamp(
            element->HorizontalOffset() + horizontalDelta,
            0.0f,
            std::max(0.0f, element->Extent().width - element->Viewport().width));
        const float verticalOffset = std::clamp(
            element->VerticalOffset() + verticalDelta,
            0.0f,
            std::max(0.0f, element->Extent().height - element->Viewport().height));
        const bool changed = horizontalOffset != element->HorizontalOffset()
            || verticalOffset != element->VerticalOffset();
        if (!changed) {
            return 0;
        }
        element->SetHorizontalOffset(horizontalOffset);
        element->SetVerticalOffset(verticalOffset);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_scroll_begin(
    xr_element* root,
    xr_animation_controller* animations,
    float x,
    float y) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || animations == nullptr || !std::isfinite(x) || !std::isfinite(y)) {
            throw std::invalid_argument("root, animations and finite coordinates are required");
        }
        xaml::Element* const scrollViewer = xaml::bridge::_details::FindScrollViewer(
            *reinterpret_cast<xaml::Element*>(root), x, y);
        if (scrollViewer == nullptr) {
            return 0;
        }
        animations->scrollController.Begin(*scrollViewer);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_scroll_drag(xr_animation_controller* animations, float verticalDelta) {
    try {
        xaml::bridge::lastError.clear();
        if (animations == nullptr || !std::isfinite(verticalDelta)) {
            throw std::invalid_argument("animations and finite vertical delta are required");
        }
        return animations->scrollController.Drag(verticalDelta) ? 1 : 0;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

void xr_scroll_end(xr_animation_controller* animations) {
    if (animations != nullptr) {
        animations->scrollController.End();
    }
}

int xr_set_render_offset_x(xr_element* element, float value) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || !std::isfinite(value)) {
            throw std::invalid_argument("element and finite value are required");
        }
        reinterpret_cast<xaml::Element*>(element)->SetRenderOffsetX(value);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_animate_render_offset_x(
    xr_element* element,
    xr_animation_controller* animations,
    float value,
    int durationMilliseconds) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || animations == nullptr || !std::isfinite(value)
            || durationMilliseconds < 0) {
            throw std::invalid_argument("element, animations, value and duration are required");
        }
        xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
        animations->value.Animations().Animate(
            target,
            xaml::AnimatedProperty::renderOffsetX,
            target.RenderOffsetX(),
            value,
            std::chrono::milliseconds(durationMilliseconds));
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

const char* xr_element_id(const xr_element* element) {
    if (element == nullptr) {
        return "";
    }
    return reinterpret_cast<const xaml::Element*>(element)->Id().c_str();
}

int xr_handle_tap(xr_element* element, xr_animation_controller* animations) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || animations == nullptr) {
            throw std::invalid_argument("element and animations are required");
        }
        xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
        if (!xaml::HandleTap(target)) {
            return 0;
        }
        if (target.Type() == xaml::ElementType::toggleSwitch) {
            animations->value.Animations().Start(target, xaml::AnimationTrigger::toggled);
        }
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_handle_pointer_down(xr_element* element, xr_animation_controller* animations) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || animations == nullptr) {
            throw std::invalid_argument("element and animations are required");
        }
        xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
        animations->value.Animations().Start(target, xaml::AnimationTrigger::pointerDown);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_handle_pointer_up(xr_element* element, xr_animation_controller* animations) {
    try {
        xaml::bridge::lastError.clear();
        if (element == nullptr || animations == nullptr) {
            throw std::invalid_argument("element and animations are required");
        }
        xaml::Element& target = *reinterpret_cast<xaml::Element*>(element);
        if (target.Type() == xaml::ElementType::button) {
            animations->value.Animations().Start(target, xaml::AnimationTrigger::pointerUp);
            return 1;
        }
        return xr_handle_tap(element, animations);
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

xr_animation_controller* xr_create_animation_controller(void) {
    try {
        xaml::bridge::lastError.clear();
        return new xr_animation_controller();
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

void xr_destroy_animation_controller(xr_animation_controller* animations) {
    delete animations;
}

xr_interaction_controller* xr_create_interaction_controller(void) {
    return new xr_interaction_controller();
}

void xr_destroy_interaction_controller(xr_interaction_controller* controller) {
    delete controller;
}

int xr_interaction_pointer_down(
    xr_interaction_controller* controller,
    xr_element* root,
    xr_animation_controller* animations,
    float x,
    float y) {
    if (controller == nullptr || root == nullptr || animations == nullptr) {
        return 0;
    }
    controller->value.PointerDown(
        *reinterpret_cast<xaml::Element*>(root),
        animations->value.Animations(),
        x,
        y);
    return controller->value.HasCapture() ? 1 : 0;
}

int xr_interaction_pointer_move(xr_interaction_controller* controller, float x, float y) {
    return controller != nullptr && controller->value.PointerMove(x, y) ? 1 : 0;
}

int xr_interaction_pointer_up(
    xr_interaction_controller* controller,
    xr_element* root,
    xr_animation_controller* animations,
    float x,
    float y,
    xr_interaction_result* result) {
    if (controller == nullptr || root == nullptr || animations == nullptr || result == nullptr) {
        return 0;
    }
    const xaml::GestureResult nativeResult = controller->value.PointerUp(
        *reinterpret_cast<xaml::Element*>(root), animations->value.Animations(), x, y);
    result->kind = static_cast<int>(nativeResult.kind);
    result->direction = static_cast<int>(nativeResult.direction);
    result->target = reinterpret_cast<xr_element*>(nativeResult.target);
    result->item_index = nativeResult.itemIndex;
    return 1;
}

int xr_interaction_scroll_wheel(
    xr_interaction_controller* controller,
    xr_element* root,
    float x,
    float y,
    float horizontalDelta,
    float verticalDelta) {
    return controller != nullptr && root != nullptr
        && controller->value.ScrollWheel(*reinterpret_cast<xaml::Element*>(root), x, y, horizontalDelta, verticalDelta) ? 1 : 0;
}

int xr_interaction_update(xr_interaction_controller* controller) {
    return controller != nullptr && controller->value.Update() ? 1 : 0;
}

int xr_set_animation_playback_rate(
    xr_animation_controller* animations,
    float playbackRate) {
    try {
        xaml::bridge::lastError.clear();
        if (animations == nullptr || !std::isfinite(playbackRate)) {
            throw std::invalid_argument("animations and finite playbackRate are required");
        }
        animations->value.SetPlaybackRate(playbackRate);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_update_animations(xr_animation_controller* animations) {
    try {
        xaml::bridge::lastError.clear();
        if (animations == nullptr) {
            throw std::invalid_argument("animations are required");
        }
        const bool wasScrolling = animations->scrollController.Update();
        return animations->value.Update() || wasScrolling ? 1 : 0;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return -1;
    }
}

xr_angle_surface* xr_create_angle_surface(
    int width,
    int height,
    const char* fontPath,
    const char* resourceRoot) {
    try {
        xaml::bridge::lastError.clear();
        if (fontPath == nullptr || resourceRoot == nullptr) {
            throw std::invalid_argument("fontPath and resourceRoot are required");
        }
        return new xr_angle_surface(width, height, fontPath, resourceRoot);
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return nullptr;
    }
}

void xr_destroy_angle_surface(xr_angle_surface* surface) {
    delete surface;
}

int xr_render_angle_surface(
    xr_angle_surface* surface,
    const xr_element* root,
    unsigned char* destination,
    int destinationStride,
    int destinationCapacity) {
    try {
        xaml::bridge::lastError.clear();
        if (surface == nullptr || root == nullptr || destination == nullptr
            || destinationStride < surface->width * 4
            || destinationCapacity / destinationStride < surface->height) {
            throw std::invalid_argument("Invalid persistent ANGLE render arguments");
        }
        surface->value.Render(
            *reinterpret_cast<xaml::Element*>(const_cast<xr_element*>(root)),
            destination,
            destinationStride);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}

int xr_render(
    const xr_element* root,
    xr_command* destination,
    int capacity) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr) {
            throw std::invalid_argument("root is required");
        }
        xaml::bridge::RecordingBackend backend;
        xaml::Render(*reinterpret_cast<xaml::Element*>(const_cast<xr_element*>(root)), backend);
        const auto& commands = backend.Commands();
        if (destination != nullptr && capacity > 0) {
            const size_t count = std::min(commands.size(), static_cast<size_t>(capacity));
            std::copy_n(commands.data(), count, destination);
        }
        return static_cast<int>(commands.size());
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return -1;
    }
}

int xr_render_angle(
    const xr_element* root,
    const char* fontPath,
    int width,
    int height,
    const char* resourceRoot,
    unsigned char* destination,
    int destinationStride,
    int destinationCapacity) {
    try {
        xaml::bridge::lastError.clear();
        if (root == nullptr || fontPath == nullptr || destination == nullptr
            || width <= 0 || height <= 0 || destinationStride < width * 4
            || destinationCapacity / destinationStride < height) {
            throw std::invalid_argument("Invalid ANGLE render arguments");
        }

        xaml::bridge::AngleRenderSurface surface(width, height, fontPath, resourceRoot);
        surface.Render(
            *reinterpret_cast<xaml::Element*>(const_cast<xr_element*>(root)),
            destination,
            destinationStride);
        return 1;
    } catch (const std::exception& error) {
        xaml::bridge::lastError = error.what();
        return 0;
    }
}
#include "PreviewPluginBridge.h"

#include <XamlRuntime/XamlLayout.h>

#include <HelpersNew/Geometry/ContainsPoint.h>

#include <filesystem>
#include <algorithm>
#include <cctype>

namespace xaml::bridge::_details {
    bool SameSourcePath(std::string_view left, std::string_view right) {
        const std::string normalizedLeft = std::filesystem::path(left).lexically_normal().generic_string();
        const std::string normalizedRight = std::filesystem::path(right).lexically_normal().generic_string();
        return normalizedLeft.size() == normalizedRight.size()
            && std::equal(normalizedLeft.begin(), normalizedLeft.end(), normalizedRight.begin(), [](char first, char second) {
                return std::tolower(static_cast<unsigned char>(first)) == std::tolower(static_cast<unsigned char>(second));
            });
    }

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

    Element* FindElementAtSource(
        Element& element,
        std::string_view sourcePath,
        int line,
        int column) {
        // Редактор передаёт путь со слешами '/', а generated XAML на Windows
        // может хранить '\\'. Сопоставляем нормализованные пути без учёта регистра.
        if (SameSourcePath(element.SourcePath(), sourcePath)
            && element.SourceLine() == line
            && element.SourceColumn() == column) {
            return &element;
        }
        for (const auto& child : element.Children()) {
            if (Element* const candidate = FindElementAtSource(*child, sourcePath, line, column)) {
                return candidate;
            }
        }
        return nullptr;
    }

    Element* HitTestVisual(Element& element, float x, float y, float offsetX, float offsetY) {
        const Rect& clipBounds = element.ClipBounds();
        if (element.VisibilityValue() != attr::Visibility::visible
            || !utility_helpers::new_helpers::geometry::ContainsPoint(
                clipBounds.x, clipBounds.y, clipBounds.width, clipBounds.height, x - offsetX, y - offsetY)) {
            return nullptr;
        }

        const auto& children = element.Children();
        const float childrenOffsetX = element.Type() == ElementType::scrollViewer
            ? offsetX - element.HorizontalOffset() : offsetX;
        const float childrenOffsetY = element.Type() == ElementType::scrollViewer
            ? offsetY - element.VerticalOffset() : offsetY;
        for (auto child = children.rbegin(); child != children.rend(); ++child) {
            if (Element* const hit = HitTestVisual(**child, x, y, childrenOffsetX, childrenOffsetY)) {
                return hit;
            }
        }

        const Rect& bounds = element.Bounds();
        return utility_helpers::new_helpers::geometry::ContainsPoint(
            bounds.x, bounds.y, bounds.width, bounds.height, x - offsetX, y - offsetY) ? &element : nullptr;
    }

    Element* FindScrollViewer(Element& root, float x, float y) {
        Element* element = HitTestVisual(root, x, y);
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
    thread_local std::string lastError;
}

namespace AndroidAppPreviewerPluginSDK {
    xp_angle_surface::xp_angle_surface(
        int width,
        int height,
        const char* fontPath,
        const char* resourceRoot)
        : width(width)
        , height(height)
        , value(width, height, fontPath, resourceRoot) {
    }

    xp_session::xp_session(int width, int height)
        : value(width, height) {
    }
}

namespace mobileclock::preview::_details {
    void ClearInspectionWireframe(AndroidAppPreviewerPluginSDK::xp_session& session) {
        if (session.inspectionElement != nullptr && !session.inspectionElementLifetime.expired()) {
            session.inspectionElement->ClearInspectionWireframe();
        }
        session.inspectionElement = nullptr;
        session.inspectionElementLifetime.reset();
    }

    void ClearSelectedWireframe(AndroidAppPreviewerPluginSDK::xp_session& session) {
        if (session.selectedElement != nullptr && !session.selectedElementLifetime.expired()) {
            session.selectedElement->ClearSelectedWireframe();
        }
        session.selectedElement = nullptr;
        session.selectedElementLifetime.reset();
    }

    void SetInspectionWireframe(AndroidAppPreviewerPluginSDK::xp_session& session, xaml::Element& element) {
        if (session.inspectionElement != &element) {
            ClearInspectionWireframe(session);
            session.inspectionElement = &element;
            session.inspectionElementLifetime = element.LifetimeToken();
        }
        element.SetInspectionWireframe(session.inspectionWireframe);
    }

    void SetSelectedWireframe(AndroidAppPreviewerPluginSDK::xp_session& session, xaml::Element& element) {
        if (session.selectedElement != &element) {
            ClearSelectedWireframe(session);
            session.selectedElement = &element;
            session.selectedElementLifetime = element.LifetimeToken();
        }
        element.SetSelectedWireframe(session.selectedWireframe);
    }
}
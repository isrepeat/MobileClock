#include "IGestureTarget.h"

#include <XamlRuntime/XamlLayout.h>

#include <algorithm>
#include <vector>

namespace mobileclock::ui::interface::_details {
    // UserControl is not always reachable through Element::Parent() from its
    // generated XAML content, so live gesture targets register explicitly.
    std::vector<IGestureTarget*> gestureTargets;
} // namespace _details

namespace mobileclock::ui::interface {
    IGestureTarget::~IGestureTarget() {
        std::erase(_details::gestureTargets, this);
    }

    IGestureTarget* IGestureTarget::Find(
        const xaml::Element& element,
        const PanState& state,
        GestureDirection direction) {
        for (IGestureTarget* const target : _details::gestureTargets) {
            if (target->Owns(element)
                && target->ResolveGesture(state, direction) != GestureHandling::ignored) {
                return target;
            }
        }
        return nullptr;
    }

    xaml::Element* IGestureTarget::FindContainingScrollViewer(const xaml::Element& element) {
        for (IGestureTarget* const target : _details::gestureTargets) {
            if (target->Owns(element)) {
                if (xaml::Element* const scrollViewer = target->FindScrollViewer(element)) {
                    return scrollViewer;
                }
            }
        }
        return nullptr;
    }

    void IGestureTarget::Update(xaml::Element& pageRoot, xaml::AnimationController& animations) {
        for (IGestureTarget* const target : _details::gestureTargets) {
            // Update has no hit-tested element, so page membership is required
            // to exclude registered controls belonging to other page trees.
            if (target->IsIn(pageRoot)) {
                target->UpdateGestures(pageRoot, animations);
            }
        }
    }

    GestureHandling IGestureTarget::ResolveGesture(const PanState&, GestureDirection) const {
        return GestureHandling::ignored;
    }

    void IGestureTarget::BeginGesture(const PanState&) {
    }

    void IGestureTarget::UpdateGesture(const PanState&) {
    }

    bool IGestureTarget::EndGesture(const PanState&, xaml::AnimationController&) {
        return false;
    }

    void IGestureTarget::CancelGesture(xaml::Element&) {
    }

    void IGestureTarget::RegisterGestureTarget() {
        _details::gestureTargets.push_back(this);
    }
}
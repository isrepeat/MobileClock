#include "MobileClock.UI/Controls/IGestureTarget.h"

#include <XamlRuntime/XamlLayout.h>

#include <algorithm>
#include <vector>

namespace mobileclock::ui::_details {
    // UserControl is not always reachable through Element::Parent() from its
    // generated XAML content, so live gesture targets register explicitly.
    std::vector<IGestureTarget*> gestureTargets;
}

namespace mobileclock::ui {
    IGestureTarget::~IGestureTarget() {
        std::erase(_details::gestureTargets, this);
    }

    IGestureTarget* IGestureTarget::Find(const xaml::Element& element) {
        for (IGestureTarget* const target : _details::gestureTargets) {
            // Owns identifies the C++ owner by traversing its XAML content from
            // the control down to the hit-tested element.
            if (target->Owns(element) && target->CanHandlePan(element)) {
                return target;
            }
        }
        return nullptr;
    }

    xaml::Element* IGestureTarget::FindContainingScrollViewer(const xaml::Element& element) {
        IGestureTarget* const target = Find(element);
        return target == nullptr ? nullptr : target->FindScrollViewer(element);
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

    bool IGestureTarget::IsVerticalPan() const {
        return false;
    }

    void IGestureTarget::RegisterGestureTarget() {
        _details::gestureTargets.push_back(this);
    }
}
#include "MobileClock.UI/Controls/PannableList.h"

#if defined(MOBILECLOCK_XAML_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/Animation.h>

namespace mobileclock::ui::controls::_details {
    bool Contains(const xaml::Element& root, const xaml::Element& element) {
        if (&root == &element) {
            return true;
        }
        for (const auto& child : root.Children()) {
            if (Contains(*child, element)) {
                return true;
            }
        }
        return false;
    }

    xaml::Element* FindPannableElement(xaml::Element& root, std::string_view id) {
        if (root.Id() == id) {
            return &root;
        }
        for (const auto& child : root.Children()) {
            if (auto* result = FindPannableElement(*child, id)) {
                return result;
            }
        }
        return nullptr;
    }
}

namespace mobileclock::ui::controls {
#if defined(MOBILECLOCK_XAML_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    bool PannableList::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) {
        try {
            const auto& content = templateNode.name == "UserControl"
                ? templateNode.children.at(0) : templateNode;
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(content, context,
                {this->Bounds().width, this->Bounds().height});
            xaml::Element* const scrollViewer = _details::FindPannableElement(*result.root, this->ScrollViewerId());
            if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
                throw std::invalid_argument("PannableList requires its configured ScrollViewer");
            }
            if (const auto* previous = this->FindElement(this->ScrollViewerId())) {
                scrollViewer->SetHorizontalOffset(previous->HorizontalOffset());
                scrollViewer->SetVerticalOffset(previous->VerticalOffset());
            }
            if (context.prepareTree) {
                context.prepareTree(*result.root);
            }
            if (context.beforeCommit) {
                context.beforeCommit();
            }
            this->ReplaceContent(std::move(result.root));
            this->runtimeBindings = std::move(result.bindings);
            diagnostics.clear();
            return true;
        } catch (const std::exception& error) {
            diagnostics = error.what();
            return false;
        }
    }
#endif

    //
    // IGestureTarget
    //
    bool PannableList::CanHandlePan(const xaml::Element& element) const {
        return this->Owns(element);
    }

    bool PannableList::IsVerticalPan() const {
        return true;
    }

    xaml::Element* PannableList::FindScrollViewer(const xaml::Element& element) const {
        return this->Owns(element) ? this->FindElement(this->ScrollViewerId()) : nullptr;
    }

    void PannableList::BeginPan(const PanState& state) {
        xaml::Element* const scrollViewer = this->FindScrollViewer(state.target);
        if (scrollViewer != nullptr) {
            this->scrollController.Begin(*scrollViewer);
        }
        this->lastPanY = state.downY;
    }

    void PannableList::UpdatePan(const PanState& state) {
        this->scrollController.Drag(this->lastPanY - state.currentY);
        this->lastPanY = state.currentY;
    }

    bool PannableList::EndPan(const PanState& state, xaml::AnimationController&) {
        this->UpdatePan(state);
        this->scrollController.End();
        return true;
    }

    void PannableList::CancelPan(xaml::Element&) {
        this->scrollController.Cancel();
    }

    void PannableList::UpdateGestures(xaml::Element&, xaml::AnimationController&) {
        this->scrollController.Update();
    }

    //
    // Internal
    //
    xaml::Element* PannableList::FindElement(std::string_view id) const {
        return _details::FindPannableElement(*const_cast<PannableList*>(this), id);
    }

    void PannableList::OnInitialized() {
        this->RegisterGestureTarget();
    }

    bool PannableList::IsIn(const xaml::Element& pageRoot) const {
        return _details::Contains(pageRoot, *this);
    }

    bool PannableList::Owns(const xaml::Element& element) const {
        return _details::Contains(*this, element);
    }
}
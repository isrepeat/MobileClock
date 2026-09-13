#include "MobileClock.UI/Controls/ScrollableList.h"

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
    bool ScrollableList::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& context, std::string& diagnostics) {
        try {
            const auto& content = templateNode.name == "UserControl"
                ? templateNode.children.at(0) : templateNode;
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(content, context,
                {this->Bounds().width, this->Bounds().height});
            xaml::Element* const scrollViewer = _details::FindPannableElement(*result.root, this->ScrollViewerId());
            if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
                throw std::invalid_argument("ScrollableList requires its configured ScrollViewer");
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
    xaml::Element* ScrollableList::FindScrollViewer(const xaml::Element& element) const {
        return this->Owns(element) ? this->FindElement(this->ScrollViewerId()) : nullptr;
    }

    void ScrollableList::UpdateGestures(xaml::Element&, xaml::AnimationController&) {
    }

    //
    // Internal
    //
    xaml::Element* ScrollableList::FindElement(std::string_view id) const {
        return _details::FindPannableElement(*const_cast<ScrollableList*>(this), id);
    }

    void ScrollableList::OnInitialized() {
        this->RegisterGestureTarget();
    }

    bool ScrollableList::IsIn(const xaml::Element& pageRoot) const {
        return _details::Contains(pageRoot, *this);
    }

    bool ScrollableList::Owns(const xaml::Element& element) const {
        return _details::Contains(*this, element);
    }
}
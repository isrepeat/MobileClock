#include "ScrollableListBase.h"

#if defined(ANDROID_APP_PREVIEWER)
#include <XamlRuntime/RuntimeMarkup/RuntimeTreeBuilder.h>
#endif
#include <XamlRuntime/Animation.h>

namespace mobileclock::ui::base::_details {
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
} // namespace _details

namespace mobileclock::ui::base {
    //
    // IGestureTarget
    //
    xaml::Element* ScrollableListBase::FindScrollViewer(const xaml::Element& element) const {
        return this->Owns(element) ? this->FindElement(this->ScrollViewerId()) : nullptr;
    }

    void ScrollableListBase::UpdateGestures(xaml::Element&, xaml::AnimationController&) {
    }

#if defined(ANDROID_APP_PREVIEWER)
    //
    // IRuntimeReloadableControl
    //
    bool ScrollableListBase::ReplaceTemplate(const xaml::runtime::XamlElementNode& templateNode,
        const xaml::runtime::RuntimeBindingContext& runtimeBindingContext, std::string& diagnostics) {
        try {
            const auto& content = templateNode.name == "UserControl"
                ? templateNode.children.at(0) : templateNode;
            auto result = xaml::runtime::RuntimeTreeBuilder{}.BuildPage(content, runtimeBindingContext,
                {this->Bounds().width, this->Bounds().height});
            xaml::Element* const scrollViewer = _details::FindPannableElement(*result.root, this->ScrollViewerId());
            if (scrollViewer == nullptr || scrollViewer->Type() != xaml::ElementType::scrollViewer) {
                throw std::invalid_argument("ScrollableListBase requires its configured ScrollViewer");
            }
            if (const auto* previous = this->FindElement(this->ScrollViewerId())) {
                scrollViewer->SetHorizontalOffset(previous->HorizontalOffset());
                scrollViewer->SetVerticalOffset(previous->VerticalOffset());
            }
            if (runtimeBindingContext.prepareTree) {
                runtimeBindingContext.prepareTree(*result.root);
            }
            if (runtimeBindingContext.beforeCommit) {
                runtimeBindingContext.beforeCommit();
            }
            this->ReplaceContent(std::move(result.root), std::move(result.bindings));
            this->preview_OnTemplateReplaced();
            diagnostics.clear();
            return true;
        } catch (const std::exception& error) {
            diagnostics = error.what();
            return false;
        }
    }

    void ScrollableListBase::preview_OnTemplateReplaced() {
    }
#endif

    xaml::Element* ScrollableListBase::FindElement(std::string_view id) const {
        return _details::FindPannableElement(*const_cast<ScrollableListBase*>(this), id);
    }

    void ScrollableListBase::OnInitialized() {
        this->RegisterGestureTarget();
    }

    bool ScrollableListBase::IsIn(const xaml::Element& pageRoot) const {
        return _details::Contains(pageRoot, *this);
    }

    bool ScrollableListBase::Owns(const xaml::Element& element) const {
        return _details::Contains(*this, element);
    }
}
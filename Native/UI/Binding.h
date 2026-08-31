#pragma once

#include "UI/IViewModel.h"
#include "XamlRuntime/XamlLayout.h"

#include <vector>

namespace mobileclock::ui {
    class BindingCollection final {
    public:
        BindingCollection() = default;
        ~BindingCollection();

        BindingCollection(const BindingCollection&) = delete;
        BindingCollection& operator=(const BindingCollection&) = delete;

        void Connect(Element& root, IViewModel& viewModel);
        void UpdateSource(Element& element, IViewModel& viewModel) const;

    private:
        void ConnectTree(Element& element, IViewModel& viewModel);
        void ConnectElement(Element& element, IViewModel& viewModel);
        static void Apply(Element& element, const attr::Binding& binding, const IViewModelValue& value);
        static IViewModelValue Read(const Element& element, const attr::Binding& binding);

    private:
        std::vector<IViewModel::Unsubscribe> unsubscriptions;
    };
}
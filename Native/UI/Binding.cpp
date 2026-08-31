#include "UI/Binding.h"

#include <stdexcept>

namespace mobileclock::ui {
    BindingCollection::~BindingCollection() {
        for (const IViewModel::Unsubscribe& unsubscribe : this->unsubscriptions) {
            unsubscribe();
        }
    }

    //
    // API
    //
    void BindingCollection::Connect(Element& root, IViewModel& viewModel) {
        for (const IViewModel::Unsubscribe& unsubscribe : this->unsubscriptions) {
            unsubscribe();
        }
        this->unsubscriptions.clear();
        this->ConnectTree(root, viewModel);
    }

    //
    // Internal
    //
    void BindingCollection::ConnectTree(Element& element, IViewModel& viewModel) {
        this->ConnectElement(element, viewModel);
        for (const auto& child : element.Children()) {
            this->ConnectTree(*child, viewModel);
        }
    }

    void BindingCollection::UpdateSource(Element& element, IViewModel& viewModel) const {
        for (const attr::Binding& binding : element.Bindings()) {
            if (binding.mode == attr::BindingMode::twoWay) {
                viewModel.SetValue(binding.path, this->Read(element, binding));
            }
        }
    }

    void BindingCollection::ConnectElement(Element& element, IViewModel& viewModel) {
        for (const attr::Binding& binding : element.Bindings()) {
            this->Apply(element, binding, viewModel.GetValue(binding.path));
            this->unsubscriptions.push_back(viewModel.Subscribe([&element, binding, &viewModel](std::string_view path) {
                if (path == binding.path) {
                    BindingCollection::Apply(element, binding, viewModel.GetValue(path));
                }
            }));
        }
    }

    void BindingCollection::Apply(Element& element, const attr::Binding& binding, const IViewModelValue& value) {
        if (binding.property == "isOn") {
            element.SetIsOn(std::get<bool>(value));
            return;
        }
        if (binding.property == "text") {
            element.SetText(std::get<std::string>(value));
            return;
        }
        throw std::invalid_argument("Unsupported binding target: " + binding.property);
    }

    IViewModelValue BindingCollection::Read(const Element& element, const attr::Binding& binding) {
        if (binding.property == "isOn") {
            return element.IsOn();
        }
        if (binding.property == "text") {
            return element.Text();
        }
        throw std::invalid_argument("Unsupported binding target: " + binding.property);
    }
}
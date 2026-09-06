#include <XamlRuntime/RenderEngine.h>
#include <XamlRuntime/XamlLayout.h>
#include <XamlRuntime/Animation.h>
#include <XamlRuntime/Input.h>

#include "Renderer/AnimationRenderers.h"
#include "UI/PageTransition.h"
#include "PageTransitions.xaml.h"

#include <stdexcept>
#include <iostream>
#include <cmath>

namespace _details {
    void Require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    bool Near(float a, float b) {
        return std::abs(a - b) < 0.001f;
    }

    class RecordingBackend final : public xaml::IRenderBackend {
    public:
        //
        // IRenderBackend
        //
        void BeginClip(const xaml::Rect& bounds) override;
        void EndClip() override;
        void DrawOutline(const xaml::Rect& bounds, xaml::attr::Color color) override;
        void DrawRoundedRect(const xaml::Rect& bounds, xaml::attr::Color color, float radius) override;
        void DrawRoundedRectOutline(const xaml::Rect& bounds, xaml::attr::Color color, float radius, float thickness) override;
        void DrawShader(std::string_view shader, const xaml::Rect& bounds,
            std::initializer_list<xaml::ShaderUniform> uniforms) override;
        void DrawText(const xaml::Rect& bounds, std::string_view text, xaml::attr::Color color,
            float size, std::string_view weight, xaml::attr::Alignment alignment) override;
        void DrawImage(const xaml::Rect& bounds, std::string_view source, xaml::attr::Color tint) override;

        float TextOpacity() const;
        float TextY() const;
        int TextCount() const;
        int ClipDepth() const;
        float OutlineOpacity() const;

    private:
        float textOpacity = 0.0f;
        float textY = 0.0f;
        int textCount = 0;
        int clipDepth = 0;
        float outlineOpacity = 0.0f;
    };

    //
    // IRenderBackend
    //
    void RecordingBackend::BeginClip(const xaml::Rect&) { ++this->clipDepth; }
    void RecordingBackend::EndClip() { --this->clipDepth; }
    void RecordingBackend::DrawOutline(const xaml::Rect&, xaml::attr::Color) {}
    void RecordingBackend::DrawRoundedRect(const xaml::Rect&, xaml::attr::Color, float) {}
    void RecordingBackend::DrawRoundedRectOutline(const xaml::Rect&, xaml::attr::Color color, float, float) {
        this->outlineOpacity = color.alpha;
    }
    void RecordingBackend::DrawShader(std::string_view, const xaml::Rect&, std::initializer_list<xaml::ShaderUniform>) {}
    void RecordingBackend::DrawText(const xaml::Rect& bounds, std::string_view, xaml::attr::Color color,
        float, std::string_view, xaml::attr::Alignment) {
        this->textOpacity = color.alpha;
        this->textY = bounds.y;
        ++this->textCount;
    }
    void RecordingBackend::DrawImage(const xaml::Rect&, std::string_view, xaml::attr::Color) {}

    //
    // API
    //
    float RecordingBackend::TextOpacity() const { return this->textOpacity; }
    float RecordingBackend::TextY() const { return this->textY; }
    int RecordingBackend::TextCount() const { return this->textCount; }
    int RecordingBackend::ClipDepth() const { return this->clipDepth; }
    float RecordingBackend::OutlineOpacity() const { return this->outlineOpacity; }

    void SetAnimation(xaml::Element& element, const char* name, const char* duration = "180") {
        xaml::AnimationTrack track;
        track.name = name;
        if (std::string(name) != "animationPageTransition") {
            track.settings.Set("duration", duration);
        }
        element.SetStoryboards({
            {xaml::AnimationTrigger::show, {track}},
            {xaml::AnimationTrigger::hide, {track}},
        });
    }

    std::unique_ptr<xaml::Element> Child(xaml::ElementType type, const char* animation) {
        auto child = std::make_unique<xaml::Element>(type);
        SetAnimation(*child, animation);
        child->SetHeight(40.0f);
        child->SetWidth(100.0f);
        return child;
    }

    void PageTransitions() {
        using namespace xaml;
        AnimationController controller;
        using namespace mobileclock::ui;
        AnimationRegistry animations;
        mobileclock::renderer::RegisterAnimations(animations);
        Element page(ElementType::page);
        page.SetDefaultAnimation("animationPageTransition");
        PageTransitionData data{"main", "settings", NavigationDirection::forward};
        page.SetAnimationParametersProvider([&]() { return AnimationParameters::Create(data); });
        layout(page, {320, 200});
        page.SetVisibility(attr::Visibility::collapsed);
        controller.Attach(page, animations);
        page.SetVisibility(attr::Visibility::visible);
        RecordingBackend backend;
        Require(page.State<ContainerAnimation>().offsetX == 320, "forward enter offset");
        Require(page.State<ContainerAnimation>().opacity == 0, "forward enter opacity");
        AnimationController::Update(page, std::chrono::milliseconds(240));
        Require(page.State<ContainerAnimation>().offsetX == 0 && page.State<ContainerAnimation>().opacity == 1, "forward enter end");
        page.SetVisibility(attr::Visibility::collapsed);
        AnimationController::Update(page, std::chrono::milliseconds(240));
        Require(page.State<ContainerAnimation>().offsetX == -320, "forward exit offset");

        data = {"settings", "main", NavigationDirection::backward};
        page.SetVisibility(attr::Visibility::visible);
        Require(page.State<ContainerAnimation>().offsetX == -320, "backward enter offset");
        AnimationController::Update(page, std::chrono::milliseconds(240));
        page.SetVisibility(attr::Visibility::collapsed);
        AnimationController::Update(page, std::chrono::milliseconds(240));
        Require(page.State<ContainerAnimation>().offsetX == 320, "backward exit offset");

        SetAnimation(page, "animationSettingsReveal", "120");
        data = {"main", "settings", NavigationDirection::forward};
        page.SetVisibility(attr::Visibility::visible);
        Require(page.State<ContainerAnimation>().offsetY == 32, "custom settings animation not selected");
        AnimationController::Update(page, std::chrono::milliseconds(320));
        page.SetVisibility(attr::Visibility::collapsed);
        AnimationController::Update(page, std::chrono::milliseconds(120));
        Require(!page.IsPresent() && page.State<ContainerAnimation>().offsetX == -48, "asymmetric hide not applied");
    }

    void Lifecycle() {
        using namespace xaml;
        AnimationController controller;
        Element root(ElementType::stackPanel);
        SetAnimation(root, "animationFade", "100");
        auto child = Child(ElementType::button, "animationFade");
        auto* button = child.get();
        button->SetCommand("click");
        SetAnimation(*button, "animationFade", "500");
        root.AddChild(std::move(child));
        auto hidden = Child(ElementType::button, "animationFade");
        hidden->SetVisibility(attr::Visibility::collapsed);
        root.AddChild(std::move(hidden));
        layout(root, {300, 300});
        controller.Attach(root, AnimationRegistry{});
        const float height = root.DesiredSize().height;
        root.SetVisibility(attr::Visibility::collapsed);
        Require(button->VisibilityValue() == attr::Visibility::visible, "child visibility was changed");
        Require(button->Presence() == PresencePhase::disappearing, "child hide not started");
        Require(root.Children()[1]->Presence() == PresencePhase::hidden, "hidden child reanimated");
        Require(!button->CanReceiveInput(), "hiding descendant accepts input");
        AnimationController::Update(root, std::chrono::milliseconds(120));
        layout(root, {300, 300});
        Require(root.IsPresent() && Near(root.DesiredSize().height, height), "layout released before child completion");
        AnimationController::Update(root, std::chrono::milliseconds(380));
        layout(root, {300, 300});
        Require(!root.IsPresent() && root.DesiredSize().height == 0, "collapsed layout not released");

        root.SetVisibility(attr::Visibility::visible);
        Require(root.Presence() == PresencePhase::appearing, "show not triggered");
        AnimationController::Update(root, std::chrono::milliseconds(500));
        root.SetVisibility(attr::Visibility::hidden);
        AnimationController::Update(root, std::chrono::milliseconds(500));
        layout(root, {300, 300});
        Require(!root.IsPresent() && Near(root.DesiredSize().height, height), "Hidden lost layout space");
        root.SetVisibility(attr::Visibility::visible);
        AnimationController::Update(root, std::chrono::milliseconds(500));
        controller.Animate(*button, AnimatedProperty::pressProgress, 0, 1, std::chrono::milliseconds(1000));
        root.RemoveChild(*button);
        Require(root.Children().size() == 2, "removed child destroyed before hide");
        AnimationController::Update(root, std::chrono::milliseconds(500));
        Require(root.Children().size() == 1, "removed child retained after hide");
        controller.Update();
        Require(!controller.IsAnimating(), "animation retained a destroyed target");
        auto added = Child(ElementType::button, "animationFade");
        auto* addedPointer = added.get();
        root.AddChild(std::move(added));
        Require(addedPointer->Presence() == PresencePhase::appearing, "inserted child did not appear");
    }


    struct TestState {
        float value = 0.0f;
        float target = 1.0f;
        int duration = 100;
        int starts = 0;
        int snapshot = 0;
        bool fromHidden = false;
        float resumed = 0.0f;
    };

    bool ValidDuration(const int& duration) {
        return duration >= 0;
    }

    bool AnimateTest(xaml::AnimationContext<TestState>& context) {
        auto& state = context.State();
        ++state.starts;
        if (const auto* snapshot = context.Parameters().TryGet<int>()) {
            state.snapshot = *snapshot;
        }
        state.fromHidden = context.IsStartingFromHidden();
        state.resumed = state.value;
        context.Animate(&TestState::value, state.target,
            std::chrono::milliseconds(state.duration), xaml::Easing::linear);
        return true;
    }

    bool AnimateFixed(xaml::AnimationContext<xaml::ContainerAnimation>& context) {
        auto& state = context.State();
        state.opacity = 0.5f;
        state.offsetY = 10;
        context.Animate(&xaml::ContainerAnimation::opacity, 0.5f, std::chrono::milliseconds(100));
        return true;
    }

    bool AnimateExtended(xaml::AnimationContext<TestState>& context) {
        context.StartDefaultAnimation();
        context.Animate(&TestState::value, 1, std::chrono::milliseconds(400));
        return true;
    }

    bool AnimateRejected(xaml::AnimationContext<TestState>& context) {
        context.State().value = 99;
        context.Animate(&TestState::value, 100, std::chrono::milliseconds(1000));
        return false;
    }

    bool RenderChildren(const xaml::Element&, xaml::RenderContext<xaml::EmptyState>& context) {
        context.RenderChildren();
        context.RenderDefault();
        return true;
    }

    bool RenderTest(const xaml::Element&, xaml::RenderContext<TestState>& context) {
        context.Backend().DrawRoundedRectOutline(
            context.Bounds(), {1, 1, 1, context.State().value}, 0, 1);
        return true;
    }

    xaml::AnimationTrack Named(const char* name, const char* duration, const char* target) {
        xaml::AnimationTrack track;
        track.name = name;
        track.settings.Set("duration", duration);
        track.settings.Set("target", target);
        return track;
    }

    void TypedRegistration() {
        using namespace xaml;
        StateRegistry states;
        states.Register<TestState>();
        AnimationRegistry registry(states);
        registry.Register<TestState>("animationTest", {
            Option("target", &TestState::target),
            Option("duration", &TestState::duration, ValidDuration),
        }, AnimateTest);
        RendererRegistry renderers(states);
        renderers.Register<TestState>("rendererTest", RenderTest);
        Element first(ElementType::button);
        Element second(ElementType::button);
        first.SetRenderer("rendererTest");
        second.SetRenderer("rendererTest");
        first.AddStoryboard({AnimationTrigger::pointerDown, {Named("animationTest", "100", "0.8")}});
        second.AddStoryboard({AnimationTrigger::pointerDown, {Named("animationTest", "200", "0.4")}});
        AnimationController controller;
        controller.Attach(first, registry);
        controller.Attach(second, registry);
        controller.Start(first, AnimationTrigger::pointerDown);
        AnimationController::Update(first, std::chrono::milliseconds(50));
        Require(Near(first.State<TestState>().value, 0.4f), "typed track not updated");
        Require(second.State<TestState>().value == 0, "two elements share state");
        RecordingBackend backend;
        Render(first, backend, renderers);
        Require(Near(backend.OutlineOpacity(), 0.4f), "renderer did not read animation state");

        // Omitted options use defaults without resetting the current field.
        AnimationTrack defaults;
        defaults.name = "animationTest";
        first.SetStoryboards({{AnimationTrigger::pointerDown, {defaults}}});
        controller.Start(first, AnimationTrigger::pointerDown);
        Require(Near(first.State<TestState>().value, 0.4f), "binding reset animated field");
        Require(first.State<TestState>().target == 1 && first.State<TestState>().duration == 100,
            "omitted options retained settings from previous event");

        for (const auto& invalid : std::vector<std::pair<std::string, std::string>>{
            {"unknown", "1"}, {"duration", "oops"}, {"duration", "-1"}, {"target", "nan"}}) {
            Element element(ElementType::border);
            AnimationTrack track;
            track.name = "animationTest";
            track.settings.Set(invalid.first, invalid.second);
            element.AddStoryboard({AnimationTrigger::show, {track}});
            bool rejected = false;
            try {
                controller.Attach(element, registry);
            } catch (const std::exception&) {
                rejected = true;
            }
            Require(rejected, "invalid XAML option was accepted");
        }
        bool duplicate = false;
        try {
            registry.Register<TestState>("animationTest", {}, AnimateTest);
        } catch (const std::invalid_argument&) {
            duplicate = true;
        }
        Require(duplicate, "duplicate animation registration accepted");
        bool undeclared = false;
        try {
            AnimationRegistry missing;
            missing.Register<TestState>("animationTest", {}, AnimateTest);
        } catch (const std::invalid_argument&) {
            undeclared = true;
        }
        Require(undeclared, "undeclared state type accepted");
        bool duplicateOption = false;
        try {
            registry.Register<TestState>("animationDuplicate", {
                Option("target", &TestState::target),
                Option("target", &TestState::value),
            }, AnimateTest);
        } catch (const std::invalid_argument&) {
            duplicateOption = true;
        }
        Require(duplicateOption, "duplicate option accepted");
    }

    void GlowAndMixedTracks() {
        using namespace xaml;
        AnimationRegistry registry;
        AnimationController controller;
        Element button(ElementType::button);
        button.SetRenderer("rendererGlow");
        button.SetForeground({1, 1, 1, 1});
        AnimationTrack glow;
        glow.name = "animationGlow";
        glow.settings.Set("intensity", "0.8");
        glow.settings.Set("duration", "300");
        AnimationTrack press;
        press.property = AnimatedProperty::pressProgress;
        press.from = 0;
        press.to = 1;
        press.duration = std::chrono::milliseconds(100);
        button.AddStoryboard({AnimationTrigger::pointerDown, {glow, press}});
        glow.settings.Set("intensity", "0");
        glow.settings.Set("duration", "150");
        button.AddStoryboard({AnimationTrigger::pointerUp, {glow}});
        layout(button, {200, 100});
        controller.Attach(button, registry);
        controller.Start(button, AnimationTrigger::pointerDown);
        AnimationController::Update(button, std::chrono::milliseconds(100));
        Require(button.PressProgress() == 1, "property track not advanced");
        const float current = button.State<Glow>().intensity;
        Require(current > 0 && current < 0.8f, "glow field not interpolated");
        RendererRegistry renderers;
        RecordingBackend backend;
        Render(button, backend, renderers);
        Require(Near(backend.OutlineOpacity(), current), "glow renderer uses another state");
        controller.Start(button, AnimationTrigger::pointerUp);
        Require(Near(button.State<Glow>().intensity, current), "glow reversal jumped");
        AnimationController::Update(button, std::chrono::milliseconds(150));
        Require(button.State<Glow>().intensity == 0 && !controller.IsAnimating(), "glow not completed");

        Element pulse(ElementType::button);
        AnimationTrack wave;
        wave.name = "animationSoftPulse";
        wave.settings.Set("duration", "100");
        wave.settings.Set("easing", "Linear");
        wave.settings.Set("intensity", "0.75");
        pulse.AddStoryboard({AnimationTrigger::pointerDown, {wave}});
        controller.Attach(pulse, registry);
        controller.Start(pulse, AnimationTrigger::pointerDown);
        AnimationController::Update(pulse, std::chrono::milliseconds(50));
        Require(Near(pulse.WaveProgress(), 0.5f) && Near(pulse.WaveIntensity(), 0.75f),
            "wave options not bound");

        Element property(ElementType::border);
        press.property = AnimatedProperty::opacity;
        press.from = 1;
        press.to = 0;
        property.AddStoryboard({AnimationTrigger::hide, {press}});
        controller.Attach(property, registry);
        property.SetVisibility(attr::Visibility::collapsed);
        Require(property.IsPresent(), "property hide not retained");
        AnimationController::Update(property, std::chrono::milliseconds(100));
        Require(!property.IsPresent(), "property hide not completed");
    }

    void ParametersAndReversal() {
        using namespace xaml;
        StateRegistry states;
        states.Register<TestState>();
        AnimationRegistry registry(states);
        registry.Register<TestState>("animationTest", {
            Option("target", &TestState::target),
            Option("duration", &TestState::duration, ValidDuration),
        }, AnimateTest);
        AnimationController controller;
        Element root(ElementType::page);
        root.SetStoryboards({
            {AnimationTrigger::hide, {Named("animationTest", "100", "1")}},
            {AnimationTrigger::show, {Named("animationTest", "100", "0")}},
        });
        int current = 7;
        int providers = 0;
        root.SetAnimationParametersProvider([&]() {
            ++providers;
            return AnimationParameters::Create(current);
        });
        controller.Attach(root, registry);
        root.SetVisibility(attr::Visibility::collapsed);
        current = 8;
        AnimationController::Update(root, std::chrono::milliseconds(40));
        Require(root.State<TestState>().snapshot == 7 && providers == 1, "snapshot changed per frame");
        root.SetVisibility(attr::Visibility::visible);
        Require(root.State<TestState>().snapshot == 8 && !root.State<TestState>().fromHidden
            && Near(root.State<TestState>().resumed, 0.4f), "reversal lost typed state");
        root.SetVisibility(attr::Visibility::visible);
        Require(root.State<TestState>().starts == 2, "same visibility restarted");
        root.SetVisibility(attr::Visibility::collapsed);
        AnimationController::Update(root, std::chrono::milliseconds(100));
        root.SetVisibility(attr::Visibility::visible);
        Require(root.State<TestState>().fromHidden, "hidden origin lost");
    }

    void RenderingAndFallback() {
        using namespace xaml;
        StateRegistry states;
        states.Register<TestState>();
        AnimationRegistry registry(states);
        registry.Register<ContainerAnimation>("animationFixed", {}, AnimateFixed);
        registry.Register<TestState>("animationExtended", {}, AnimateExtended);
        registry.Register<TestState>("animationRejected", {}, AnimateRejected);
        AnimationController controller;
        Element root(ElementType::page);
        AnimationTrack fixed;
        fixed.name = "animationFixed";
        root.AddStoryboard({AnimationTrigger::show, {fixed}});
        auto child = std::make_unique<Element>(ElementType::textBlock);
        child->SetText("test");
        child->AddStoryboard({AnimationTrigger::show, {fixed}});
        auto* target = child.get();
        root.AddChild(std::move(child));
        layout(root, {300, 300});
        const float originalY = target->Bounds().y;
        controller.Attach(root, registry, true);
        root.SetRenderer("rendererChildren");
        RendererRegistry renderers;
        renderers.Register<EmptyState>("rendererChildren", RenderChildren);
        RecordingBackend backend;
        Render(root, backend, renderers);
        Require(backend.TextCount() == 1 && Near(backend.TextOpacity(), 0.25f),
            "render composition or child count changed");
        Require(Near(backend.TextY(), originalY + 20), "typed transforms not composed");
        Require(root.Opacity() == 1 && target->Opacity() == 1, "base opacity mutated");

        for (const char* name : {"animationMissing", "animationRejected", "animationExtended"}) {
            Element fallback(ElementType::page);
            fallback.SetDefaultAnimation("animationFade");
            AnimationTrack track;
            track.name = name;
            fallback.AddStoryboard({AnimationTrigger::hide, {track}});
            controller.Attach(fallback, registry);
            fallback.SetVisibility(attr::Visibility::collapsed);
            AnimationController::Update(fallback, std::chrono::milliseconds(200));
            const bool extended = std::string(name) == "animationExtended";
            Require(fallback.IsPresent() == extended, "fallback or extension broken");
            if (std::string(name) == "animationRejected") {
                Require(fallback.State<TestState>().value == 0, "refused handler changed state");
            }
            AnimationController::Update(fallback, std::chrono::milliseconds(200));
            Require(!fallback.IsPresent(), "fallback not completed");
        }

        Element empty(ElementType::border);
        empty.SetDefaultAnimation("animationFade");
        empty.AddStoryboard({AnimationTrigger::hide, {}});
        controller.Attach(empty, registry);
        empty.SetVisibility(attr::Visibility::collapsed);
        Require(!empty.IsPresent(), "empty storyboard did not disable default");

        int viewModel = 0;
        BindingScope bindings;
        auto page = generated::PageTransitions::Create(viewModel, bindings);
        layout(*page, {300, 300});
        controller.Attach(*page, registry, true);
        Require(page->State<ContainerAnimation>().offsetY == 32, "generated option lost");
        AnimationController::Update(*page, std::chrono::milliseconds(180));
        Require(page->Presence() == PresencePhase::appearing, "Show uses Hide duration");
        AnimationController::Update(*page, std::chrono::milliseconds(140));
        page->SetVisibility(attr::Visibility::collapsed);
        AnimationController::Update(*page, std::chrono::milliseconds(180));
        Require(!page->IsPresent(), "Hide uses Show duration");
    }
}

int main() {
    _details::PageTransitions();
    _details::Lifecycle();
    _details::TypedRegistration();
    _details::GlowAndMixedTracks();
    _details::ParametersAndReversal();
    _details::RenderingAndFallback();
    std::cout << "All typed animation tests passed";
    return 0;
}
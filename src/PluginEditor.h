#pragma once

#include <memory>

#include "PluginProcessor.h"

class ThereminVisualDisplay final : public juce::Component, private juce::Timer
{
public:
    explicit ThereminVisualDisplay(ThereminProcessor&);
    ~ThereminVisualDisplay() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    ThereminProcessor& processor;
    float smoothPitch    = 0.5f;
    float smoothEnvelope = 0.0f;
    float vibratoPhaseDisplay = 0.0f;
    juce::Image thereminImage;
    std::unique_ptr<juce::Drawable> pitchHandDrawable;
    std::unique_ptr<juce::Drawable> volHandDrawable;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminVisualDisplay)
};

class ThereminEditor final : public juce::AudioProcessorEditor,
                             private juce::AudioProcessorListener
{
public:
    explicit ThereminEditor(ThereminProcessor&);
    ~ThereminEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override;

    ThereminProcessor& processorRef;
    ThereminVisualDisplay visualDisplay;

    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label  attackLabel,  decayLabel,  sustainLabel,  releaseLabel;

    juce::Slider waveSlider;

    struct WaveSelectorOverlay final : public juce::Component
    {
        juce::Slider& slider;
        explicit WaveSelectorOverlay(juce::Slider& s) : slider(s) {}
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

    private:
        void selectFromEvent(const juce::MouseEvent& e);
    };
    WaveSelectorOverlay waveSelectorOverlay { waveSlider };

    juce::Slider vibratoAmountSlider, gainSlider;
    juce::Label  vibratoAmountLabel,  gainLabel;

    juce::ComboBox    presetBox;
    juce::TextButton  deleteBtn { "x" };
    void rebuildPresetBox();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> sustainAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> waveAttachment;
    std::unique_ptr<SliderAttachment> vibratoAmountAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminEditor)
};

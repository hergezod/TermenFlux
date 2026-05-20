#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

class ThereminProcessor final : public juce::AudioProcessor
{
public:
    ThereminProcessor();
    ~ThereminProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    int getProgramWaveform(int index) const noexcept;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    struct UserPreset
    {
        juce::String name;
        float attack = 0.1f, decay = 0.1f, sustain = 0.8f, release = 0.5f, gain = 1.0f;
        float vibratoAmount = 0.3f, vibratoRate = 5.5f;
        int waveform = 0;
    };
    std::vector<UserPreset> userPresets;
    void addUserPreset(const juce::String& name);
    void deleteUserPreset(int index);

    int selectedPresetId = 0;
    int getSelectedPresetId() const noexcept { return selectedPresetId; }

    float getVisualPitchNormalized() const noexcept;
    float getVisualEnvelopeLevel() const noexcept;
    float getVisualGainNormalized() const noexcept;
    float getVibratoAmount() const noexcept;
    float getVibratoRate() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void cacheParameterPointers();
    float readParameter(std::atomic<float>* parameter, float fallback) const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    juce::ADSR adsr;

    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* gainParam = nullptr;
    std::atomic<float>* vibratoAmountParam = nullptr;
    std::atomic<float>* vibratoRateParam = nullptr;
    std::atomic<float>* waveformParam = nullptr;

    double sampleRate = 44100.0;
    double glideAlpha = 0.0;
    int currentProgram = 0;
    int currentMidiNote = -1;
    bool hasPitchState = false;
    float velocityScale = 1.0f;
    float prevPulseOut  = 0.0f;
    double currentFrequency = 0.0;
    double targetFrequency = 0.0;
    double pitchBendSemitones = 0.0;
    double phase = 0.0;
    double vibratoPhase = 0.0;
    float vibratoFade = 0.0f;

    juce::LinearSmoothedValue<float> smoothedAttack;
    juce::LinearSmoothedValue<float> smoothedDecay;
    juce::LinearSmoothedValue<float> smoothedSustain;
    juce::LinearSmoothedValue<float> smoothedRelease;
    juce::LinearSmoothedValue<float> smoothedGain;

    std::atomic<float> visualPitchNorm { 0.5f };
    std::atomic<float> visualEnvelope { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThereminProcessor)
};

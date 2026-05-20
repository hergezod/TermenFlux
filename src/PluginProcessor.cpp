#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace ProcessorPresets
{
struct Entry
{
    const char* name;
    float attack, decay, sustain, release, gain, vibratoAmount, vibratoRate;
    int waveform;
};

static constexpr Entry table[] = {
    // Sine
    { "Classic Theremin",  0.09f, 0.03f, 0.93f, 0.50f, 1.00f, 0.28f, 5.35f, 0 },
    { "Soft Ambient",      0.40f, 0.55f, 0.82f, 1.40f, 0.88f, 0.20f, 4.40f, 0 },
    { "Deep Meditation",   0.60f, 0.40f, 0.95f, 3.00f, 0.85f, 0.12f, 3.80f, 0 },
    // Triangle
    { "Soft Pluck",        0.01f, 0.20f, 0.50f, 0.40f, 1.05f, 0.08f, 5.20f, 1 },
    { "Wooden Flute",      0.12f, 0.15f, 0.80f, 0.60f, 0.95f, 0.14f, 5.00f, 1 },
    { "Cave Echo",         0.40f, 0.60f, 0.65f, 2.80f, 0.90f, 0.10f, 3.60f, 1 },
    // Saw
    { "Raspy Lead",        0.03f, 0.08f, 0.85f, 0.35f, 1.05f, 0.18f, 5.60f, 2 },
    { "Dark Drone",        0.50f, 0.50f, 0.95f, 3.50f, 0.85f, 0.07f, 3.30f, 2 },
    // Pulse
    { "Sharp Pluck",       0.01f, 0.12f, 0.45f, 0.25f, 1.10f, 0.06f, 5.80f, 3 },
    { "Wet Pad",           0.60f, 0.50f, 0.90f, 2.00f, 0.85f, 0.12f, 4.10f, 3 },
};

static constexpr int kCount = (int)(sizeof(table) / sizeof(table[0]));
}

static void applyProcessorPreset(juce::AudioProcessorValueTreeState& apvts, int index)
{
    if (index < 0 || index >= ProcessorPresets::kCount)
        return;

    const auto& pr = ProcessorPresets::table[index];
    auto setFloat = [&](const char* id, float v) {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
            p->setValueNotifyingHost(p->convertTo0to1(v));
    };
    setFloat("attack",  pr.attack);
    setFloat("decay",   pr.decay);
    setFloat("sustain", pr.sustain);
    setFloat("release", pr.release);
    setFloat("gain",    pr.gain);
    setFloat("vibrato_amount", pr.vibratoAmount);
    setFloat("vibrato_rate",   pr.vibratoRate);

    if (auto* w = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("waveform")))
        w->setValueNotifyingHost(w->convertTo0to1(pr.waveform));
}

namespace
{
constexpr int kInstrumentMidiTranspose = 12;
constexpr double kPitchBendRangeSemitones = 12.0;
constexpr double kVibratoFadeSeconds = 0.32;

double midiNoteToFrequency(int midiNote, double pitchBendSemitones)
{
    const int shiftedNote = juce::jlimit(0, 127, midiNote + kInstrumentMidiTranspose);
    const double baseFrequency = juce::MidiMessage::getMidiNoteInHertz(shiftedNote, 440.0);
    const double bendRatio = std::pow(2.0, pitchBendSemitones / 12.0);
    return juce::jlimit(20.0, 12000.0, baseFrequency * bendRatio);
}

float frequencyToDisplay01(double hz)
{
    constexpr double fLo = 65.0;
    constexpr double fHi = 4186.0;
    if (hz <= 1.0)
        return 0.5f;

    const double clamped = juce::jlimit(fLo, fHi, hz);
    const double t = (std::log(clamped) - std::log(fLo)) / (std::log(fHi) - std::log(fLo));
    return (float)juce::jlimit(0.0, 1.0, t);
}

float polyBlep(float t, float dt) noexcept
{
    if (dt <= 0.0f)
        return 0.0f;

    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}
}

juce::AudioProcessorValueTreeState::ParameterLayout ThereminProcessor::createParameterLayout()
{
    const juce::NormalisableRange<float> timeRange(0.0f, 5.0f, 0.0001f, 0.35f);
    const juce::NormalisableRange<float> sustainRange(0.0f, 1.0f, 0.0001f);
    const juce::NormalisableRange<float> gainRange(0.0f, 1.5f, 0.0001f, 0.5f);
    const juce::NormalisableRange<float> vibratoAmountRange(0.0f, 1.0f, 0.001f);
    const juce::NormalisableRange<float> vibratoRateRange(1.0f, 10.0f, 0.01f);

    return { std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "attack", 1 },
                                                         "Attack",
                                                         timeRange,
                                                         0.10f),
             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "decay", 1 },
                                                         "Decay",
                                                         timeRange,
                                                         0.02f),
             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "sustain", 1 },
                                                         "Sustain",
                                                         sustainRange,
                                                         0.92f),
             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "release", 1 },
                                                         "Release",
                                                         timeRange,
                                                         0.45f),
             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "gain", 1 },
                                                         "Gain",
                                                         gainRange,
                                                         1.0f),
             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "vibrato_amount", 1 },
                                                         "Vibrato Amount",
                                                         vibratoAmountRange,
                                                         0.3f),
             std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{ "vibrato_rate", 1 },
                                                         "Vibrato Rate",
                                                         vibratoRateRange,
                                                         5.5f),
             std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{ "waveform", 1 },
                                                          "Waveform",
                                                          juce::StringArray{ "Sine", "Triangle", "Saw", "Pulse" },
                                                          0) };
}

ThereminProcessor::ThereminProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ),
      apvts(*this, nullptr, "ThereminParams", createParameterLayout())
{
    cacheParameterPointers();
}

ThereminProcessor::~ThereminProcessor() = default;

void ThereminProcessor::cacheParameterPointers()
{
    attackParam = apvts.getRawParameterValue("attack");
    decayParam = apvts.getRawParameterValue("decay");
    sustainParam = apvts.getRawParameterValue("sustain");
    releaseParam = apvts.getRawParameterValue("release");
    gainParam = apvts.getRawParameterValue("gain");
    vibratoAmountParam = apvts.getRawParameterValue("vibrato_amount");
    vibratoRateParam = apvts.getRawParameterValue("vibrato_rate");
    waveformParam = apvts.getRawParameterValue("waveform");
}

float ThereminProcessor::readParameter(std::atomic<float>* parameter, float fallback) const noexcept
{
    return parameter != nullptr ? parameter->load(std::memory_order_relaxed) : fallback;
}

float ThereminProcessor::getVisualPitchNormalized() const noexcept
{
    return visualPitchNorm.load(std::memory_order_relaxed);
}

float ThereminProcessor::getVisualEnvelopeLevel() const noexcept
{
    return visualEnvelope.load(std::memory_order_relaxed);
}

float ThereminProcessor::getVisualGainNormalized() const noexcept
{
    return juce::jlimit(0.0f, 1.0f, readParameter(gainParam, 1.0f) / 1.5f);
}

const juce::String ThereminProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ThereminProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ThereminProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ThereminProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ThereminProcessor::getTailLengthSeconds() const
{
    return 5.0;
}

int ThereminProcessor::getNumPrograms()
{
    return ProcessorPresets::kCount;
}

int ThereminProcessor::getCurrentProgram()
{
    return currentProgram;
}

void ThereminProcessor::setCurrentProgram(int index)
{
    if (index < 0 || index >= ProcessorPresets::kCount)
        return;
    currentProgram = index;
    applyProcessorPreset(apvts, index);
    updateHostDisplay(ChangeDetails().withProgramChanged(true));
}

const juce::String ThereminProcessor::getProgramName(int index)
{
    if (index < 0 || index >= ProcessorPresets::kCount)
        return {};
    return ProcessorPresets::table[index].name;
}

int ThereminProcessor::getProgramWaveform(int index) const noexcept
{
    if (index < 0 || index >= ProcessorPresets::kCount)
        return 0;
    return ProcessorPresets::table[index].waveform;
}

void ThereminProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void ThereminProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;

    constexpr double glideTauSeconds = 0.2;
    glideAlpha = 1.0 - std::exp(-1.0 / (sampleRate * glideTauSeconds));
    glideAlpha = juce::jlimit(0.0, 1.0, glideAlpha);

    constexpr float paramRampSeconds = 0.05f;
    smoothedAttack.reset(sampleRate, paramRampSeconds);
    smoothedDecay.reset(sampleRate, paramRampSeconds);
    smoothedSustain.reset(sampleRate, paramRampSeconds);
    smoothedRelease.reset(sampleRate, paramRampSeconds);
    smoothedGain.reset(sampleRate, 0.035f);

    smoothedAttack.setCurrentAndTargetValue(juce::jmax(1.0e-6f, readParameter(attackParam, 0.1f)));
    smoothedDecay.setCurrentAndTargetValue(juce::jmax(1.0e-6f, readParameter(decayParam, 0.02f)));
    smoothedSustain.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.0f, readParameter(sustainParam, 0.92f)));
    smoothedRelease.setCurrentAndTargetValue(juce::jmax(1.0e-6f, readParameter(releaseParam, 0.45f)));
    smoothedGain.setCurrentAndTargetValue(juce::jlimit(0.0f, 1.5f, readParameter(gainParam, 1.0f)));

    adsr.setSampleRate(sampleRate);
    adsr.reset();

    phase          = 0.0;
    vibratoPhase   = 0.0;
    vibratoFade    = 0.0f;
    currentFrequency = 0.0;
    targetFrequency  = 0.0;
    pitchBendSemitones = 0.0;
    hasPitchState  = false;
    velocityScale  = 1.0f;
    prevPulseOut   = 0.0f;
    currentMidiNote = -1;
}

float ThereminProcessor::getVibratoAmount() const noexcept
{
    return juce::jlimit(0.0f, 1.0f, readParameter(vibratoAmountParam, 0.3f));
}

float ThereminProcessor::getVibratoRate() const noexcept
{
    return juce::jlimit(1.0f, 10.0f, readParameter(vibratoRateParam, 5.5f));
}

void ThereminProcessor::releaseResources()
{
    adsr.reset();
}

bool ThereminProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void ThereminProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    const int waveIndex = juce::jlimit(0, 3, (int)std::lround(readParameter(waveformParam, 0.0f)));
    const double vibratoRate = (double)getVibratoRate();

    const double vibratoDepth = (double)getVibratoAmount() * 0.00202;

    smoothedAttack.setTargetValue(juce::jmax(1.0e-6f, readParameter(attackParam, 0.1f)));
    smoothedDecay.setTargetValue(juce::jmax(1.0e-6f, readParameter(decayParam, 0.02f)));
    smoothedSustain.setTargetValue(juce::jlimit(0.0f, 1.0f, readParameter(sustainParam, 0.92f)));
    smoothedRelease.setTargetValue(juce::jmax(1.0e-6f, readParameter(releaseParam, 0.45f)));
    smoothedGain.setTargetValue(juce::jlimit(0.0f, 1.5f, readParameter(gainParam, 1.0f)));

    {
        float atk = 0.0f, dec = 0.0f, sus = 0.0f, rel = 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            atk = juce::jmax(1.0e-6f, smoothedAttack.getNextValue());
            dec = juce::jmax(1.0e-6f, smoothedDecay.getNextValue());
            sus = juce::jlimit(0.0f, 1.0f, smoothedSustain.getNextValue());
            rel = juce::jmax(1.0e-6f, smoothedRelease.getNextValue());
        }
        juce::ADSR::Parameters adsrParams;
        adsrParams.attack  = atk;
        adsrParams.decay   = dec;
        adsrParams.sustain = sus;
        adsrParams.release = rel;
        adsr.setParameters(adsrParams);
    }

    const int lastSampleIndex = numSamples - 1;
    auto applyMidi = [this](const juce::MidiMessage& msg) {
        if (msg.isNoteOn())
        {
            currentMidiNote = msg.getNoteNumber();
            // Octave transpose as Renoise default octave plays low notes.
            targetFrequency = midiNoteToFrequency(currentMidiNote, pitchBendSemitones);

            velocityScale = juce::jlimit(0.0f, 1.0f, msg.getVelocity() / 127.0f);
            vibratoFade = 0.0f;

            if (!hasPitchState)
            {
                currentFrequency = targetFrequency;
                hasPitchState = true;
            }

            adsr.noteOn();
        }
        else if (msg.isPitchWheel())
        {
            const double normalizedBend = juce::jlimit(-1.0, 1.0, ((double)msg.getPitchWheelValue() - 8192.0) / 8192.0);
            pitchBendSemitones = normalizedBend * kPitchBendRangeSemitones;
            if (currentMidiNote >= 0)
                targetFrequency = midiNoteToFrequency(currentMidiNote, pitchBendSemitones);
        }
        else if (msg.isNoteOff())
        {
            if (msg.getNoteNumber() == currentMidiNote)
            {
                currentMidiNote = -1;
                targetFrequency = currentFrequency;
                adsr.noteOff();
            }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            currentMidiNote = -1;
            hasPitchState = false;
            vibratoFade = 0.0f;
            adsr.reset();
        }
    };

    const double twoPi = juce::MathConstants<double>::twoPi;
    const float vibratoFadeIncrement = sampleRate > 0.0 ? (float)(1.0 / (sampleRate * kVibratoFadeSeconds)) : 1.0f;
    constexpr float oscHeadroom = 0.2f;

    float lastEnv = 0.0f;
    double lastFreq = currentFrequency;

    auto midiEvent = midi.begin();
    const auto midiEnd = midi.end();

    for (int i = 0; i < numSamples; ++i)
    {
        while (midiEvent != midiEnd)
        {
            const auto metadata = *midiEvent;
            const int eventSample = juce::jlimit(0, lastSampleIndex, (int)metadata.samplePosition);
            if (eventSample > i)
                break;

            applyMidi(metadata.getMessage());
            ++midiEvent;
        }

        currentFrequency += glideAlpha * (targetFrequency - currentFrequency);
        currentFrequency = juce::jlimit(20.0, 12000.0, currentFrequency);

        const float env = adsr.getNextSample();
        const float masterGain = smoothedGain.getNextValue();
        if (currentMidiNote >= 0)
            vibratoFade = juce::jmin(1.0f, vibratoFade + vibratoFadeIncrement);

        const double vibratoMod = 1.0 + vibratoDepth * (double)(env * vibratoFade) * std::sin(vibratoPhase);
        const double modulatedFreq = currentFrequency * vibratoMod;
        const float phase01 = (float)(phase / twoPi);
        const float phaseStep = juce::jlimit(1.0e-6f, 0.5f, (float)(modulatedFreq / sampleRate));

        float osc;
        switch (waveIndex)
        {
            case 1:
                osc = 1.0f - 4.0f * std::abs(phase01 - 0.5f);
                break;
            case 2:
                osc = 2.0f * phase01 - 1.0f;
                osc -= polyBlep(phase01, phaseStep);
                break;
            case 3:
            {
                float t2 = phase01 + 0.5f;
                if (t2 >= 1.0f)
                    t2 -= 1.0f;

                float raw = phase01 < 0.5f ? 1.0f : -1.0f;
                raw += polyBlep(phase01, phaseStep);
                raw -= polyBlep(t2, phaseStep);
                prevPulseOut += 0.45f * (raw - prevPulseOut);
                osc = prevPulseOut;
                break;
            }
            default:
                osc = (float)std::sin(phase);
                break;
        }

        const float sample = juce::jlimit(-1.0f, 1.0f, osc * env * velocityScale * (oscHeadroom * masterGain));

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, sample);

        phase += twoPi * modulatedFreq / sampleRate;
        if (phase >= twoPi)
        {
            while (phase >= twoPi)
                phase -= twoPi;
        }
        if (!std::isfinite(phase))
            phase = 0.0;

        vibratoPhase += twoPi * vibratoRate / sampleRate;
        if (vibratoPhase >= twoPi)
        {
            while (vibratoPhase >= twoPi)
                vibratoPhase -= twoPi;
        }

        lastEnv = env;
        lastFreq = currentFrequency;
    }

    visualPitchNorm.store(frequencyToDisplay01(lastFreq), std::memory_order_relaxed);
    visualEnvelope.store(lastEnv, std::memory_order_relaxed);
}

bool ThereminProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ThereminProcessor::createEditor()
{
    return new ThereminEditor(*this);
}

void ThereminProcessor::addUserPreset(const juce::String& name)
{
    UserPreset up;
    up.name = name;
    auto get = [&](const char* id) -> float {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(id)))
            return p->get();
        return 0.0f;
    };
    up.attack   = get("attack");
    up.decay    = get("decay");
    up.sustain  = get("sustain");
    up.release  = get("release");
    up.gain     = get("gain");
    up.vibratoAmount = get("vibrato_amount");
    up.vibratoRate   = get("vibrato_rate");
    if (auto* w = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("waveform")))
        up.waveform = w->getIndex();
    userPresets.push_back(up);
}

void ThereminProcessor::deleteUserPreset(int index)
{
    if (index >= 0 && index < (int)userPresets.size())
        userPresets.erase(userPresets.begin() + index);
}

void ThereminProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("currentProgram", currentProgram, nullptr);
    state.setProperty("selectedPresetId", selectedPresetId, nullptr);
    auto userPresetsNode = juce::ValueTree("UserPresets");
    for (const auto& up : userPresets)
    {
        auto node = juce::ValueTree("Preset");
        node.setProperty("name",    up.name,    nullptr);
        node.setProperty("attack",  up.attack,  nullptr);
        node.setProperty("decay",   up.decay,   nullptr);
        node.setProperty("sustain", up.sustain, nullptr);
        node.setProperty("release", up.release, nullptr);
        node.setProperty("gain",    up.gain,    nullptr);
        node.setProperty("vibratoAmount", up.vibratoAmount, nullptr);
        node.setProperty("vibratoRate",   up.vibratoRate,   nullptr);
        node.setProperty("waveform",up.waveform,nullptr);
        userPresetsNode.appendChild(node, nullptr);
    }
    state.appendChild(userPresetsNode, nullptr);
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void ThereminProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!tree.isValid())
            return;
        userPresets.clear();
        auto userPresetsNode = tree.getChildWithName("UserPresets");
        if (userPresetsNode.isValid())
        {
            for (int i = 0; i < userPresetsNode.getNumChildren(); ++i)
            {
                auto node = userPresetsNode.getChild(i);
                UserPreset up;
                up.name     = node.getProperty("name",    "User Preset").toString();
                up.attack   = (float)node.getProperty("attack",   0.1f);
                up.decay    = (float)node.getProperty("decay",    0.1f);
                up.sustain  = (float)node.getProperty("sustain",  0.8f);
                up.release  = (float)node.getProperty("release",  0.5f);
                up.gain     = (float)node.getProperty("gain",     1.0f);
                up.vibratoAmount = (float)node.getProperty("vibratoAmount", 0.3f);
                up.vibratoRate   = (float)node.getProperty("vibratoRate",   5.5f);
                up.waveform = (int)  node.getProperty("waveform", 0);
                userPresets.push_back(up);
            }
            tree.removeChild(userPresetsNode, nullptr);
        }
        currentProgram    = (int)tree.getProperty("currentProgram", 0);
        selectedPresetId  = (int)tree.getProperty("selectedPresetId", 0);
        tree.removeProperty("currentProgram", nullptr);
        tree.removeProperty("selectedPresetId", nullptr);
        apvts.replaceState(tree);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThereminProcessor();
}

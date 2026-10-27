#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/TractEngine.h"

#include <array>
#include <atomic>

namespace pink_ids
{
inline constexpr const char* frequency = "frequency";
inline constexpr const char* tenseness = "tenseness";
inline constexpr const char* intensity = "intensity";
inline constexpr const char* loudness = "loudness";
inline constexpr const char* noise = "noise";
inline constexpr const char* vibratoFrequency = "vibratoFrequency";
inline constexpr const char* vibratoGain = "vibratoGain";
inline constexpr const char* vibratoWobble = "vibratoWobble";
inline constexpr const char* tongueIndex = "tongueIndex";
inline constexpr const char* tongueDiameter = "tongueDiameter";
inline constexpr const char* tractLength = "tractLength";
inline constexpr const char* aspiration = "aspiration";
inline constexpr const char* nasal = "nasal";
inline constexpr const char* mix = "mix";
inline constexpr const char* output = "output";

inline juce::String constrictionIndex (int slot) { return "constriction" + juce::String (slot) + "index"; }
inline juce::String constrictionDiameter (int slot) { return "constriction" + juce::String (slot) + "diameter"; }
inline juce::String constrictionEnable (int slot) { return "constriction" + juce::String (slot) + "enable"; }
} // namespace pink_ids

class PinkTronAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kConstrictionSlots = 4;

    // Copy of the tract shape for the UI (audio thread writes, message thread reads).
    struct TractSnapshot
    {
        static constexpr int maxSections = 320;

        std::array<float, maxSections> diameter {};
        int length = 0;
        int noseStart = 0;
        float tongueIndex = 0.0f;
        float tongueDiameter = 0.0f;
        float velum = 0.0f;
    };

    PinkTronAudioProcessor();
    ~PinkTronAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pink Tron"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    TractSnapshot readSnapshot() const;
    void writeSnapshot (const pink::Tract& tract);

    // UI -> parameters
    int firstFreeConstrictionSlot() const;
    void setConstriction (int slot, float index, float diameter);
    void clearConstriction (int slot);
    bool isConstrictionEnabled (int slot) const;
    float getParamValue (const juce::String& id) const;
    void setParamValue (const juce::String& id, float value);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void readParameters (pink::TractEngine::Params&) const;

    std::array<pink::TractEngine, 2> engines;
    pink::Random noiseRandom { 0x51ED27u };
    pink::Biquad aspirateFilter, fricativeFilter;
    juce::SmoothedValue<float> mixSmoothed, outputSmoothed;

    double currentSampleRate = 44100.0;
    double seconds = 0.0;
    bool needsShapePrime = true;

    std::array<TractSnapshot, 2> snapshotBuffers {};
    std::atomic<int> snapshotIndex { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinkTronAudioProcessor)
};

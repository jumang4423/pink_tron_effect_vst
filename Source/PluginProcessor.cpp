#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace pink_ids;

namespace
{
constexpr int kConstrictionSlots = PinkTronAudioProcessor::kConstrictionSlots;

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const juce::String& id,
                               const juce::String& name,
                               juce::NormalisableRange<float> range,
                               float defaultValue,
                               const juce::String& suffix = {},
                               int decimals = 2) {
        auto attributes = juce::AudioParameterFloatAttributes()
                              .withLabel (suffix)
                              .withStringFromValueFunction ([decimals, suffix] (float value, int) {
                                  return juce::String (value, decimals)
                                         + (suffix.isEmpty() ? juce::String() : " " + suffix);
                              });

        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range,
                                                                 defaultValue, attributes));
    };

    // --- original Pink Trombone parameters ---------------------------------
    addFloat (frequency, "Frequency", juce::NormalisableRange<float> (20.0f, 2000.0f, 0.01f, 0.3f), 140.0f, "Hz", 1);
    addFloat (tenseness, "Tenseness", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, {}, 2);
    addFloat (intensity, "Intensity", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, {}, 2);
    addFloat (loudness, "Loudness", juce::NormalisableRange<float> (0.0f, 2.0f), 1.0f, {}, 2);
    addFloat (noise, "Noise", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, {}, 2);
    addFloat (vibratoFrequency, "Vibrato Freq", juce::NormalisableRange<float> (0.0f, 20.0f), 6.0f, "Hz", 2);
    addFloat (vibratoGain, "Vibrato Gain", juce::NormalisableRange<float> (0.0f, 0.1f, 0.0001f), 0.005f, {}, 3);
    addFloat (vibratoWobble, "Vibrato Wobble", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, {}, 2);
    addFloat (tongueIndex, "Tongue Index", juce::NormalisableRange<float> (0.0f, 88.0f), 12.9f, {}, 1);
    addFloat (tongueDiameter, "Tongue Size", juce::NormalisableRange<float> (2.05f, 3.5f), 2.43f, {}, 2);
    addFloat (tractLength, "Tract Length", juce::NormalisableRange<float> (15.0f, 88.0f), 44.0f, {}, 1);

    for (int slot = 0; slot < kConstrictionSlots; ++slot)
    {
        addFloat (constrictionIndex (slot), "Constriction " + juce::String (slot + 1) + " Index",
                  juce::NormalisableRange<float> (2.0f, 88.0f), 30.0f, {}, 1);
        addFloat (constrictionDiameter (slot), "Constriction " + juce::String (slot + 1) + " Diameter",
                  juce::NormalisableRange<float> (-1.0f, 3.0f), 1.0f, {}, 2);

        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { constrictionEnable (slot), 1 },
                                                                "Constriction " + juce::String (slot + 1),
                                                                false));
    }

    // --- additions ---------------------------------------------------------
    addFloat (aspiration, "Aspiration", juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 0.0f, "%", 0);
    addFloat (nasal, "Nasal", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, {}, 2);
    addFloat (mix, "Mix", juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f, "%", 0);
    addFloat (output, "Output", juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), -6.0f, "dB", 1);

    return layout;
}
} // namespace

PinkTronAudioProcessor::PinkTronAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", makeLayout())
{
}

PinkTronAudioProcessor::~PinkTronAudioProcessor() = default;

void PinkTronAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    seconds = 0.0;

    for (auto& engine : engines)
        engine.prepare (sampleRate);

    aspirateFilter.setBandPass (500.0, 0.5, sampleRate);
    fricativeFilter.setBandPass (1000.0, 0.5, sampleRate);
    aspirateFilter.reset();
    fricativeFilter.reset();

    mixSmoothed.reset (sampleRate, 0.02);
    outputSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (getParamValue (mix) * 0.01f);
    outputSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (getParamValue (output)));

    needsShapePrime = true;
    snapshotBuffers[0] = TractSnapshot {};
    snapshotBuffers[1] = TractSnapshot {};
    snapshotIndex.store (0);
}

void PinkTronAudioProcessor::releaseResources() {}

bool PinkTronAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void PinkTronAudioProcessor::readParameters (pink::TractEngine::Params& p) const
{
    p.frequency = (double) getParamValue (frequency);
    p.tenseness = (double) getParamValue (tenseness);
    p.intensity = (double) getParamValue (intensity);
    p.loudness = (double) getParamValue (loudness);
    p.noiseLevel = (double) getParamValue (noise);
    p.vibratoFrequency = (double) getParamValue (vibratoFrequency);
    p.vibratoGain = (double) getParamValue (vibratoGain);
    p.vibratoWobble = (double) getParamValue (vibratoWobble);
    p.tongueIndex = (double) getParamValue (tongueIndex);
    p.tongueDiameter = (double) getParamValue (tongueDiameter);
    p.tractLength = (double) getParamValue (tractLength);
    p.aspiration = (double) getParamValue (aspiration) * 0.01;
    p.nasal = (double) getParamValue (nasal);

    p.constrictions = pink::ConstrictionSet {};

    for (int slot = 0; slot < kConstrictionSlots; ++slot)
    {
        if (getParamValue (constrictionEnable (slot)) > 0.5f)
            p.constrictions.addSlot (slot,
                                     pink::Constriction { (double) getParamValue (constrictionIndex (slot)),
                                                          (double) getParamValue (constrictionDiameter (slot)) });
    }

    p.constrictions.tongueIndex = p.tongueIndex;
    p.constrictions.tongueDiameter = p.tongueDiameter;
}

void PinkTronAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    const int activeChannels = juce::jmin (numChannels, 2);

    pink::TractEngine::Params params;
    readParameters (params);

    for (int ch = 0; ch < activeChannels; ++ch)
    {
        engines[(size_t) ch].setParams (params);
        engines[(size_t) ch].setBufferInterpolationStep (1.0 / (double) numSamples);
    }

    if (needsShapePrime)
    {
        for (int ch = 0; ch < activeChannels; ++ch)
            engines[(size_t) ch].update (seconds);

        needsShapePrime = false;
    }

    mixSmoothed.setTargetValue (getParamValue (mix) * 0.01f);
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (getParamValue (output)));

    const double noiseLevel = (double) getParamValue (noise);
    const double dt = 1.0 / currentSampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        const double white = noiseRandom.nextBipolar();
        const double noiseSource = (aspirateFilter.process (white) + fricativeFilter.process (white)) * noiseLevel;

        const double bufferInterpolation = (double) i / (double) numSamples;
        const float mixAmount = mixSmoothed.getNextValue();
        const float gain = outputSmoothed.getNextValue();

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const double input = (double) data[i];
            const double wet = engines[(size_t) ch].processSample (input, noiseSource, seconds, bufferInterpolation);

            data[i] = (float) ((input * (1.0 - (double) mixAmount) + wet * (double) mixAmount) * (double) gain);
        }

        seconds += dt;
    }

    for (int ch = 0; ch < activeChannels; ++ch)
        engines[(size_t) ch].update (seconds);

    writeSnapshot (engines[0].getTract());
}

void PinkTronAudioProcessor::writeSnapshot (const pink::Tract& tract)
{
    const int writeIndex = 1 - snapshotIndex.load (std::memory_order_relaxed);
    auto& snap = snapshotBuffers[(size_t) writeIndex];

    const int length = juce::jlimit (0, TractSnapshot::maxSections, tract.getLength());
    snap.length = length;
    snap.noseStart = tract.getNoseStart();
    snap.tongueIndex = (float) tract.getTongueIndex();
    snap.tongueDiameter = (float) tract.getTongueDiameter();
    snap.velum = (float) tract.getVelumTarget();

    const auto& diameters = tract.diameters();

    for (int i = 0; i < length; ++i)
        snap.diameter[(size_t) i] = (float) diameters[(size_t) i];

    snapshotIndex.store (writeIndex, std::memory_order_release);
}

PinkTronAudioProcessor::TractSnapshot PinkTronAudioProcessor::readSnapshot() const
{
    return snapshotBuffers[(size_t) snapshotIndex.load (std::memory_order_acquire)];
}

float PinkTronAudioProcessor::getParamValue (const juce::String& id) const
{
    if (const auto* v = apvts.getRawParameterValue (id))
        return v->load();

    return 0.0f;
}

void PinkTronAudioProcessor::setParamValue (const juce::String& id, float value)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

bool PinkTronAudioProcessor::isConstrictionEnabled (int slot) const
{
    return getParamValue (constrictionEnable (slot)) > 0.5f;
}

int PinkTronAudioProcessor::firstFreeConstrictionSlot() const
{
    for (int slot = 0; slot < kConstrictionSlots; ++slot)
        if (! isConstrictionEnabled (slot))
            return slot;

    return 0;
}

void PinkTronAudioProcessor::setConstriction (int slot, float index, float diameter)
{
    slot = juce::jlimit (0, kConstrictionSlots - 1, slot);
    // Never place a constriction on the glottis itself: a fully closed section at
    // index 0/1 becomes a near-lossless one-sample resonator (very loud).
    setParamValue (constrictionIndex (slot), juce::jlimit (2.0f, 87.0f, index));
    setParamValue (constrictionDiameter (slot), diameter);
    setParamValue (constrictionEnable (slot), 1.0f);
}

void PinkTronAudioProcessor::clearConstriction (int slot)
{
    slot = juce::jlimit (0, kConstrictionSlots - 1, slot);
    setParamValue (constrictionEnable (slot), 0.0f);
}

void PinkTronAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PinkTronAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PinkTronAudioProcessor::createEditor()
{
    return new PinkTronAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PinkTronAudioProcessor();
}

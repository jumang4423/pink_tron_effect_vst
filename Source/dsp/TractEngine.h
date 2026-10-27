#pragma once

#include "GlottisNoise.h"
#include "PinkMath.h"
#include "Tract.h"

namespace pink
{
// One mono vocal-tract chain, driven by an external excitation (the incoming
// audio) instead of the internal glottal waveform.
class TractEngine
{
public:
    struct Params
    {
        // --- original Pink Trombone parameters -----------------------------
        double frequency = 140.0;          // Hz   (breath pulse rate)
        double tenseness = 0.6;            // 0..1
        double intensity = 1.0;            // 0..1
        double loudness = 1.0;             // input drive (was: voiced level)
        double noiseLevel = 1.0;           // noise generator gain
        double vibratoFrequency = 6.0;     // Hz
        double vibratoGain = 0.005;
        double vibratoWobble = 1.0;
        double tongueIndex = 12.9;
        double tongueDiameter = 2.43;
        double tractLength = 44.0;
        ConstrictionSet constrictions;

        // --- additions ------------------------------------------------------
        double aspiration = 0.0; // 0..1 breath amount (relative to input level)
        double nasal = 0.0;      // 0..1 velum override (0 = original behaviour)
    };

    void prepare (double sampleRate)
    {
        sampleRateValue = std::max (8000.0, sampleRate);

        tract.prepare (sampleRateValue);
        glottis.prepare (sampleRateValue, 0x51ED27u);

        smootherFrequency.prepare (sampleRateValue);
        smootherTenseness.prepare (sampleRateValue);
        smootherIntensity.prepare (sampleRateValue);
        smootherVibratoGain.prepare (sampleRateValue);
        smootherVibratoFrequency.prepare (sampleRateValue);
        smootherVibratoWobble.prepare (sampleRateValue);

        envelopeCoeff = 1.0 - std::exp (-1.0 / (0.03 * sampleRateValue));

        reset();
        snapToParams();
    }

    void reset()
    {
        tract.reset();
        glottis.reset();
        envelope = 0.0;
        previousInput = 0.0;
        snapToParams();
    }

    // Block-rate parameters.
    void setParams (const Params& newParams)
    {
        params = newParams;

        smootherFrequency.setTarget (params.frequency);
        smootherTenseness.setTarget (params.tenseness);
        smootherIntensity.setTarget (params.intensity);
        smootherVibratoGain.setTarget (params.vibratoGain);
        smootherVibratoFrequency.setTarget (params.vibratoFrequency);
        smootherVibratoWobble.setTarget (params.vibratoWobble);

        tract.setVelumOverride (params.nasal);
    }

    void snapToParams()
    {
        smootherFrequency.snap (params.frequency);
        smootherTenseness.snap (params.tenseness);
        smootherIntensity.snap (params.intensity);
        smootherVibratoGain.snap (params.vibratoGain);
        smootherVibratoFrequency.snap (params.vibratoFrequency);
        smootherVibratoWobble.snap (params.vibratoWobble);
    }

    // Per-sample processing. `noiseSource` is the shared white-noise / band-passed
    // signal used by the aspiration generator.
    double processSample (double input, double noiseSource, double seconds, double bufferInterpolation)
    {
        const double drive = input * params.loudness;

        // Input level follower so that "aspiration 100%" means "breath as loud
        // as the input" at any input level.
        envelope += (drive * drive - envelope) * envelopeCoeff;
        const double inputRms = std::sqrt (std::max (0.0, envelope));

        const double breath = glottis.process (noiseSource,
                                               smootherFrequency.next(),
                                               smootherTenseness.next(),
                                               smootherIntensity.next(),
                                               smootherVibratoGain.next(),
                                               smootherVibratoFrequency.next(),
                                               smootherVibratoWobble.next(),
                                               seconds);

        double excitation = drive;

        if (params.aspiration > 0.0)
            excitation += breath * (params.aspiration * kAspirationScale * inputRms);

        Tract::ParamSamples ps;
        ps.tractLength = params.tractLength;
        ps.tongueIndex = params.tongueIndex;
        ps.tongueDiameter = params.tongueDiameter;
        ps.glottis = excitation;
        ps.noiseModulator = (noiseModulatorOverride >= 0.0) ? noiseModulatorOverride
                                                            : glottis.getNoiseModulator();

        // The reference runs the tract twice per sample at a half-sample offset
        // (2x oversampling) and sums the result; we halve it to keep unity gain.
        double wet = tract.process (ps, bufferInterpolation, seconds);
        wet += tract.process (ps, bufferInterpolation + 0.5 * bufferInterpolationStep, seconds);

        previousInput = input;
        (void) previousInput;

        return wet * 0.5;
    }

    void setBufferInterpolationStep (double step) noexcept { bufferInterpolationStep = step; }

    // Optional override for the fricative-noise modulator (used by the offline
    // faithfulness test). Negative = derive it from the glottis stage as usual.
    void setNoiseModulatorOverride (double v) noexcept { noiseModulatorOverride = v; }

    void update (double seconds)
    {
        pink::ConstrictionSet set = params.constrictions;
        set.tongueIndex = tract.getTongueIndex();
        set.tongueDiameter = tract.getTongueDiameter();
        tract.update (seconds, set);
    }

    const Tract& getTract() const noexcept { return tract; }
    const Params& getParams() const noexcept { return params; }

    static constexpr double kAspirationScale = 34.0;

private:
    Tract tract;
    GlottisNoise glottis;
    Params params;

    Smoother smootherFrequency, smootherTenseness, smootherIntensity;
    Smoother smootherVibratoGain, smootherVibratoFrequency, smootherVibratoWobble;

    double sampleRateValue = 44100.0;
    double envelope = 0.0;
    double envelopeCoeff = 0.001;
    double bufferInterpolationStep = 0.0;
    double previousInput = 0.0;
    double noiseModulatorOverride = -1.0;
};
} // namespace pink

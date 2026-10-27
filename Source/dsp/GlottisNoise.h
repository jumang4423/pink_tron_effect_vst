#pragma once

#include "SimplexNoise.h"
#include "PinkMath.h"

namespace pink
{
// Port of Glottis.js with the voiced waveform removed: it only produces the
// aspiration (breath) noise component. The phase/period logic is kept so the
// breath pulses at the "frequency" parameter, exactly like the web version.
class GlottisNoise
{
public:
    void prepare (double sampleRate, unsigned int seed = 44100u)
    {
        sampleRateValue = sampleRate;
        noise = SimplexNoise (seed);
        startSeconds = 0.0;
        noiseModulator = 0.0;
    }

    void reset() noexcept { startSeconds = 0.0; noiseModulator = 0.0; }

    double process (double noiseSource,
                    double frequency,
                    double tenseness,
                    double intensity,
                    double vibratoGain,
                    double vibratoFrequency,
                    double vibratoWobble,
                    double seconds)
    {
        constexpr double twoPi = 6.28318530717958647692;

        double vibrato = 0.0;
        vibrato += vibratoGain * std::sin (twoPi * seconds * vibratoFrequency);
        vibrato += 0.02 * noise.simplex1 (seconds * 4.07);
        vibrato += 0.04 * noise.simplex1 (seconds * 2.15);

        if (vibratoWobble > 0.0)
        {
            double wobble = 0.0;
            wobble += 0.2 * noise.simplex1 (seconds * 0.98);
            wobble += 0.4 * noise.simplex1 (seconds * 0.5);
            vibrato += wobble * vibratoWobble;
        }

        double freq = frequency * (1.0 + vibrato);
        if (! (freq > 0.001))
            freq = 0.001;

        double tense = tenseness;
        tense += 0.1 * noise.simplex1 (seconds * 0.46);
        tense += 0.05 * noise.simplex1 (seconds * 0.36);
        tense += (3.0 - tense) * (1.0 - intensity);

        const double period = 1.0 / freq;

        const double secondsOffset = seconds - startSeconds;
        double interpolation = secondsOffset / period;

        if (interpolation >= 1.0)
        {
            startSeconds = seconds + std::fmod (secondsOffset, period);
            interpolation = startSeconds / period;
        }

        double modulator = getNoiseModulator (interpolation);
        modulator += (1.0 - tense * intensity) * 3.0;
        noiseModulator = modulator;

        double n = noiseSource;
        n *= modulator;
        n *= intensity;
        n *= intensity;
        n *= 1.0 - std::sqrt (std::max (tense, 0.0));
        n *= 0.02 * noise.simplex1 (seconds * 1.99) + 0.2;

        // The reference scales the whole (noise + voice) output by intensity.
        n *= intensity;

        return n;
    }

    double getNoiseModulator() const noexcept { return noiseModulator; }

private:
    static double getNoiseModulator (double interpolation) noexcept
    {
        const double angle = 6.28318530717958647692 * interpolation;
        const double amplitude = std::sin (angle);
        const double positiveAmplitude = std::max (0.0, amplitude);

        constexpr double offset = 0.1;
        constexpr double gain = 0.2;

        return positiveAmplitude * gain + offset;
    }

    SimplexNoise noise { 44100u };
    double sampleRateValue = 44100.0;
    double startSeconds = 0.0;
    double noiseModulator = 0.0;
};
} // namespace pink

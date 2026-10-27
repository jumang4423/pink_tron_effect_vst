// Offline render / sanity check for the DSP core (no JUCE, plain C++).
//   c++ -std=c++20 -O2 tests/offline_render.cpp Source/dsp/Tract.cpp -ISource/dsp -o /tmp/pinktron_render
//   /tmp/pinktron_render out_dir

#include "TractEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace
{
void writeWavF32 (const std::string& path, const std::vector<float>& samples, int sampleRate)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (f == nullptr)
    {
        std::printf ("cannot open %s\n", path.c_str());
        return;
    }

    const std::uint32_t dataBytes = (std::uint32_t) (samples.size() * sizeof (float));
    const std::uint32_t riffSize = 36 + dataBytes;
    const std::uint32_t fmtSize = 16;
    const std::uint16_t audioFormat = 3; // IEEE float
    const std::uint16_t channels = 1;
    const std::uint32_t byteRate = (std::uint32_t) sampleRate * channels * 4;
    const std::uint16_t blockAlign = (std::uint16_t) (channels * 4);

    std::fwrite ("RIFF", 1, 4, f);
    std::fwrite (&riffSize, 4, 1, f);
    std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f);
    std::fwrite (&fmtSize, 4, 1, f);
    std::fwrite (&audioFormat, 2, 1, f);
    std::fwrite (&channels, 2, 1, f);
    const std::uint32_t sr = (std::uint32_t) sampleRate;
    std::fwrite (&sr, 4, 1, f);
    std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&blockAlign, 2, 1, f);
    const std::uint16_t bits = 32;
    std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f);
    std::fwrite (&dataBytes, 4, 1, f);
    std::fwrite (samples.data(), 1, dataBytes, f);
    std::fclose (f);
}

double rmsOf (const std::vector<float>& v, int start = 0)
{
    double sum = 0.0;
    for (std::size_t i = (std::size_t) start; i < v.size(); ++i)
        sum += (double) v[i] * (double) v[i];
    return std::sqrt (sum / (double) std::max<std::size_t> (1, v.size() - (std::size_t) start));
}

double peakOf (const std::vector<float>& v)
{
    double p = 0.0;
    for (auto s : v)
        p = std::max (p, (double) std::fabs (s));
    return p;
}

int countNonFinite (const std::vector<float>& v)
{
    int n = 0;
    for (auto s : v)
        if (! std::isfinite (s))
            ++n;
    return n;
}

struct Render
{
    std::vector<float> out;
    int nonFinite = 0;
};

Render render (const std::vector<float>& in, const pink::TractEngine::Params& params, double sr, bool withAspirationInputDep)
{
    (void) withAspirationInputDep;

    pink::TractEngine engine;
    engine.prepare (sr);
    engine.setParams (params);

    constexpr int blockSize = 128;
    pink::Random noiseGen (12345u);
    pink::Biquad bpf1, bpf2;
    bpf1.setBandPass (500.0, 0.5, sr);
    bpf2.setBandPass (1000.0, 0.5, sr);
    engine.setBufferInterpolationStep (1.0 / (double) blockSize);

    Render r;
    r.out.resize (in.size(), 0.0f);

    std::size_t pos = 0;
    double seconds = 0.0;

    while (pos < in.size())
    {
        const std::size_t n = std::min<std::size_t> (blockSize, in.size() - pos);

        for (std::size_t i = 0; i < n; ++i)
        {
            const double white = noiseGen.nextBipolar();
            const double noiseSrc = (bpf1.process (white) + bpf2.process (white)) * params.noiseLevel;
            const double bip = (double) i / (double) n;

            const double out = engine.processSample ((double) in[pos + i], noiseSrc, seconds, bip);
            r.out[pos + i] = (float) out;

            seconds += 1.0 / sr;
        }

        engine.update (seconds);
        pos += n;
    }

    r.nonFinite = countNonFinite (r.out);
    return r;
}
} // namespace

int main (int argc, char** argv)
{
    const std::string outDir = (argc > 1) ? argv[1] : ".";
    const double sr = 44100.0;
    const int n = (int) (sr * 2.0);

    // Source: a sawtooth-ish tone at 220 Hz, -12 dBFS, with an AM envelope.
    std::vector<float> in ((std::size_t) n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const double t = (double) i / sr;
        double phase = std::fmod (t * 220.0, 1.0);
        double saw = 2.0 * phase - 1.0;
        const double env = 0.5 + 0.5 * std::sin (2.0 * M_PI * 1.5 * t);
        in[(std::size_t) i] = (float) (0.25 * saw * env);
    }

    pink::TractEngine::Params p;
    p.tongueIndex = 12.9;
    p.tongueDiameter = 2.43;
    p.tractLength = 44.0;
    p.frequency = 140.0;
    p.tenseness = 0.6;
    p.intensity = 1.0;
    p.loudness = 1.0;
    p.noiseLevel = 1.0;

    auto dry = render (in, p, sr, false);

    auto aspOnly = p;
    aspOnly.loudness = 0.0; // kill the input, keep the breath envelope? (needs input for envelope)
    aspOnly.aspiration = 1.0;

    auto wet = render (in, p, sr, false);

    auto breathy = p;
    breathy.aspiration = 1.0;
    auto breathyOut = render (in, breathy, sr, false);

    std::printf ("input   rms=%.5f peak=%.5f\n", rmsOf (in), peakOf (in));
    std::printf ("effect  rms=%.5f peak=%.5f nonFinite=%d\n", rmsOf (dry.out), peakOf (dry.out), dry.nonFinite);
    std::printf ("+100%%asp rms=%.5f peak=%.5f nonFinite=%d\n", rmsOf (breathyOut.out), peakOf (breathyOut.out), breathyOut.nonFinite);

    // gain ratio in dB
    const double gin = rmsOf (in);
    const double gout = rmsOf (dry.out);
    std::printf ("wet gain = %.2f dB\n", 20.0 * std::log10 (gout / gin));

    writeWavF32 (outDir + "/dry_input.wav", in, (int) sr);
    writeWavF32 (outDir + "/effect_default.wav", dry.out, (int) sr);
    writeWavF32 (outDir + "/effect_breathy.wav", breathyOut.out, (int) sr);

    // --- vocal tract transfer function: white noise in, spectrum out --------
    {
        std::vector<float> white ((std::size_t) n, 0.0f);
        pink::Random rng (999u);
        for (auto& s : white)
            s = (float) (rng.nextBipolar() * 0.1);

        auto res = render (white, p, sr, false);
        writeWavF32 (outDir + "/tract_ir.wav", res.out, (int) sr);
        writeWavF32 (outDir + "/tract_noise_in.wav", white, (int) sr);
        std::printf ("white-in  rms=%.5f peak=%.5f nonFinite=%d\n", rmsOf (white), peakOf (white), res.nonFinite);
    }

    (void) wet;
    (void) aspOnly;

    return 0;
}

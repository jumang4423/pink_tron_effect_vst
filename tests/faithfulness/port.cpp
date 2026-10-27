// Same deterministic scenario as reference.mjs, executed by the C++ port.
// The engine halves the 2x-oversampled sum for unity gain, so the JS side is
// compared against 2 * port output (see run.sh).
#include "TractEngine.h"

#include <cstdio>

int main()
{
    const double sr = 44100.0;
    const int blocks = 24;
    const int bufferLength = 128;

    pink::TractEngine engine;
    engine.prepare (sr);
    engine.setBufferInterpolationStep (1.0 / (double) bufferLength);
    engine.setNoiseModulatorOverride (1.3); // same constant the JS harness passes

    // Deterministic input, identical generator to reference.mjs
    long long seed = 12345;
    auto rnd = [&seed]() {
        seed = (seed * 16807LL) % 2147483647LL;
        return (double) seed / 2147483647.0 * 2.0 - 1.0;
    };

    double input[bufferLength];

    for (int i = 0; i < bufferLength; ++i)
        input[i] = rnd() * 0.3;

    double seconds = 0.0;

    for (int block = 0; block < blocks; ++block)
    {
        pink::TractEngine::Params p;
        p.loudness = 1.0;
        p.aspiration = 0.0;
        p.tongueIndex = 12.9 + block * 0.4;
        p.tongueDiameter = 2.43 + 0.03 * block;
        p.tractLength = block < 8 ? 44.0 : block < 16 ? 30.0 : 60.0;

        p.constrictions = pink::ConstrictionSet {};

        if (block >= 4)
            p.constrictions.addSlot (0, pink::Constriction { 20.0 + block * 0.5, 1.4 - block * 0.1 > 0.45 ? 1.4 - block * 0.1 : 0.45 });

        if (block >= 12)
            p.constrictions.addSlot (1, pink::Constriction { 30.0, 2.5 });

        engine.setParams (p);

        for (int i = 0; i < bufferLength; ++i)
        {
            const double out = engine.processSample (input[i], 0.0, seconds, (double) i / (double) bufferLength);
            std::printf ("%.17g\n", out);
            seconds += 1.0 / sr;
        }

        engine.update (seconds);
    }

    return 0;
}

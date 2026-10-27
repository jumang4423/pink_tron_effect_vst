// Minimal JUCE host: loads the built plugin (VST3 or AU), renders a test tone
// through it and reports NaN / level statistics. Writes a WAV for inspection.
//
//   ./PinkTronHost "/path/to/Pink Tron.vst3" out.wav
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_processors/format/juce_AudioPluginFormatManagerHelpers.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <cmath>
#include <cstdio>

namespace
{
struct Stats
{
    double rms = 0.0;
    double peak = 0.0;
    int nonFinite = 0;
    int clipped = 0;
};

Stats analyse (const juce::AudioBuffer<float>& buffer)
{
    Stats s;
    double sum = 0.0;
    juce::int64 count = 0;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* d = buffer.getReadPointer (ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const double v = (double) d[i];

            if (! std::isfinite (v))
            {
                ++s.nonFinite;
                continue;
            }

            s.peak = std::max (s.peak, std::abs (v));
            s.clipped += (std::abs (v) > 1.0) ? 1 : 0;
            sum += v * v;
            ++count;
        }
    }

    s.rms = count > 0 ? std::sqrt (sum / (double) count) : 0.0;
    return s;
}

void windowReport (const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    const int win = (int) (sampleRate * 0.1);

    for (int ch = 0; ch < juce::jmin (1, buffer.getNumChannels()); ++ch)
    {
        std::printf ("  ch%d:", ch);

        for (int start = 0; start + win <= buffer.getNumSamples(); start += win)
        {
            double p = 0.0;

            for (int i = 0; i < win; ++i)
                p = std::max (p, (double) std::abs (buffer.getSample (ch, start + i)));

            std::printf (" %.2f", p);
        }

        std::printf ("\n");
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::String pluginPath = argc > 1 ? juce::String (argv[1]) : juce::String();
    const juce::String wavPath = argc > 2 ? juce::String (argv[2]) : juce::String();

    if (pluginPath.isEmpty())
    {
        std::printf ("usage: PinkTronHost <plugin> [out.wav]\n");
        return 1;
    }

    juce::AudioPluginFormatManager formatManager;
    juce::addDefaultFormatsToManager (formatManager);

    juce::OwnedArray<juce::PluginDescription> found;

    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (pluginPath))
            format->findAllTypesForFile (found, pluginPath);

    if (found.isEmpty())
    {
        std::printf ("no plugin types found in %s\n", pluginPath.toRawUTF8());
        return 1;
    }

    const auto* description = found.getFirst();
    std::printf ("found: %s (%s) by %s\n",
                 description->name.toRawUTF8(),
                 description->pluginFormatName.toRawUTF8(),
                 description->manufacturerName.toRawUTF8());

    juce::AudioPluginFormat* format = nullptr;

    for (auto* f : formatManager.getFormats())
        if (f->getName() == description->pluginFormatName)
            format = f;

    if (format == nullptr)
    {
        std::printf ("format not available\n");
        return 1;
    }

    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance (
        format->createInstanceFromDescription (*description, 48000.0, 512, error));

    if (instance == nullptr)
    {
        std::printf ("instantiation failed: %s\n", error.toRawUTF8());
        return 1;
    }

    instance->enableAllBuses();
    const double sampleRate = 48000.0;
    const int blockSize = 512;
    instance->setRateAndBufferSizeDetails (sampleRate, blockSize);
    instance->prepareToPlay (sampleRate, blockSize);

    std::printf ("instance: %d in / %d out channels, %d parameters, latency %d samples\n",
                 instance->getTotalNumInputChannels(),
                 instance->getTotalNumOutputChannels(),
                 (int) instance->getParameters().size(),
                 instance->getLatencySamples());

    // List the parameters (proves the whole parameter set is exposed).
    for (auto* p : instance->getParameters())
        std::printf ("  param: %-28s %.4f\n", p->getName (64).toRawUTF8(), p->getValue());

    auto setParam = [&instance] (const juce::String& name, float normalised) {
        for (auto* p : instance->getParameters())
            if (p->getName (64).equalsIgnoreCase (name))
                p->setValue (normalised);
    };

    juce::String shotPath;
    bool stress = false;

    // Optional overrides: name=value pairs (normalised 0..1). "shot=path.png"
    // snapshots the editor after rendering.
    for (int i = 3; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        const auto name = arg.upToFirstOccurrenceOf ("=", false, false);
        const auto value = arg.fromFirstOccurrenceOf ("=", false, false).getFloatValue();

        if (name.equalsIgnoreCase ("shot"))
        {
            shotPath = arg.fromFirstOccurrenceOf ("=", false, false);
            continue;
        }

        if (name.equalsIgnoreCase ("stress"))
        {
            stress = value > 0.5f;
            continue;
        }

        bool foundParam = false;

        for (auto* p : instance->getParameters())
        {
            if (p->getName (64).equalsIgnoreCase (name))
            {
                // hosted parameters are addressed in normalised 0..1 units
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
                foundParam = true;
            }
        }

        std::printf ("set %s = %.3f (%s)\n", name.toRawUTF8(), value, foundParam ? "ok" : "NOT FOUND");
    }

    const int totalSamples = (int) (sampleRate * 2.0);
    juce::AudioBuffer<float> buffer (juce::jmax (2, instance->getTotalNumOutputChannels()), blockSize);
    juce::AudioBuffer<float> recorded (buffer.getNumChannels(), totalSamples);
    recorded.clear();

    double phase = 0.0;
    int written = 0;
    Stats worst;

    for (int pos = 0; pos < totalSamples; pos += blockSize)
    {
        const int n = juce::jmin (blockSize, totalSamples - pos);
        buffer.setSize (buffer.getNumChannels(), n, false, false, true);
        buffer.clear();

        for (int i = 0; i < n; ++i)
        {
            phase += 220.0 / sampleRate;
            if (phase >= 1.0)
                phase -= 1.0;

            const float saw = (float) (0.22 * (2.0 * phase - 1.0));

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample (ch, i, saw);
        }

        if (stress)
        {
            // hammer every parameter with random values each block
            juce::Random rng (pos);

            for (auto* p : instance->getParameters())
                p->setValue (rng.nextFloat());
        }

        juce::MidiBuffer midi;
        instance->processBlock (buffer, midi);

        const auto s = analyse (buffer);
        worst.nonFinite += s.nonFinite;
        worst.peak = std::max (worst.peak, s.peak);
        worst.clipped += s.clipped;

        for (int ch = 0; ch < recorded.getNumChannels(); ++ch)
            recorded.copyFrom (ch, pos, buffer, ch, 0, n);

        written += n;
    }

    windowReport (recorded, sampleRate);
    const auto s = analyse (recorded);
    std::printf ("rendered %d samples: rms=%.4f peak=%.4f nonFinite=%d samplesOver1.0=%d\n",
                 written, s.rms, s.peak, worst.nonFinite, worst.clipped);

    if (! wavPath.isEmpty())
    {
        juce::File out (wavPath);
        out.deleteFile();

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());

        if (stream != nullptr)
        {
            std::unique_ptr<juce::AudioFormatWriter> writer (
                wav.createWriterFor (stream.get(), sampleRate, (unsigned int) recorded.getNumChannels(), 32, {}, 0));

            if (writer != nullptr)
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (recorded, 0, recorded.getNumSamples());
                std::printf ("wrote %s\n", wavPath.toRawUTF8());
            }
        }
    }

    if (shotPath.isNotEmpty())
    {
        std::printf ("hasEditor=%d\n", (int) instance->hasEditor());
        if (auto* editor = instance->createEditorIfNeeded())
        {
            editor->setSize (editor->getWidth(), editor->getHeight());
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.5f);

            juce::File out (shotPath);
            out.deleteFile();
            std::unique_ptr<juce::FileOutputStream> stream (out.createOutputStream());

            if (stream != nullptr)
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (image, *stream);
                std::printf ("wrote screenshot %s (%dx%d)\n", shotPath.toRawUTF8(), image.getWidth(), image.getHeight());
            }
        }
    }

    instance->releaseResources();
    instance.reset();

    return (s.nonFinite == 0) ? 0 : 2;
}

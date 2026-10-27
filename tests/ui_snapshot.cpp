// Tiny GUI app: instantiates the editor directly and writes a PNG snapshot.
// Used to eyeball the UI without opening a DAW.
//
//   ./PinkTronUI /tmp/editor.png
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

class SnapshotApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "PinkTronUI"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String& commandLine) override
    {
        outputPath = commandLine.trim().unquoted();
        if (outputPath.isEmpty())
            outputPath = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("pinktron_editor.png")
                             .getFullPathName();

        processor = std::make_unique<PinkTronAudioProcessor>();
        processor->setRateAndBufferSizeDetails (48000.0, 512);
        processor->prepareToPlay (48000.0, 512);

        // feed it a moment of audio so the tract display has a shape
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 8; ++block)
        {
            buffer.clear();

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float v = (float) (0.2 * std::sin (juce::MathConstants<double>::twoPi * 220.0
                                                         * (double) (block * 512 + i) / 48000.0));
                buffer.setSample (0, i, v);
                buffer.setSample (1, i, v);
            }

            processor->processBlock (buffer, midi);
        }

        editor = std::make_unique<PinkTronAudioProcessorEditor> (*processor);
        editor->setSize (760, 656);
        editor->setVisible (true);

        juce::Timer::callAfterDelay (700, [this] {
            editor->setSize (760, 656);
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.5f);
            juce::File out (outputPath);
            out.deleteFile();

            if (auto stream = std::unique_ptr<juce::FileOutputStream> (out.createOutputStream()))
            {
                juce::PNGImageFormat png;
                png.writeImageToStream (image, *stream);
            }

            quit();
        });
    }

    void shutdown() override
    {
        window.reset();
        editor.reset();
        processor.reset();
    }

    void systemRequestedQuit() override { quit(); }
    void anotherInstanceStarted (const juce::String&) override {}

private:
    std::unique_ptr<PinkTronAudioProcessor> processor;
    std::unique_ptr<PinkTronAudioProcessorEditor> editor;
    std::unique_ptr<juce::DocumentWindow> window;
    juce::String outputPath;
};

START_JUCE_APPLICATION (SnapshotApp)

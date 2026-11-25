#pragma once

#include "OptoEngine.h"
#include "MakeupLaw.h"
#include <juce_audio_processors/juce_audio_processors.h>

// FastVocal Comp Basic: transparent optical-style leveler for vocals.
// Fixed sound: Limit mode, clean detector with natural attack, 5 ms lookahead.
// Controls: Compression (0..100), Output trim (on top of an automatic make-up linked to Compression), Bypass.
class OptoProcessor : public juce::AudioProcessor
{
public:
    OptoProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Bypass is the plugin's official bypass: the host's bypass button drives it, and it keeps the 5 ms delay.
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    static constexpr int limitMode = 1;

    // Largest gain reduction actually applied (dB, positive) since the editor last read it.
    float readAndResetReductionDb() { return reductionDb.exchange (0.0f); }
    unsigned int processedBlocks() const { return blockCounter.load(); }
    double autoMakeupDb() const;
    double totalOutputDb() const { return autoMakeupDb() + (double) output->load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    opto::Engine engine;
    juce::SmoothedValue<double> outputGain, bypassMix;
    std::atomic<float>* compression = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* bypass = nullptr;
    float lastCompression = -1.0f;
    std::atomic<float> reductionDb { 0.0f };
    std::atomic<unsigned int> blockCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OptoProcessor)
};

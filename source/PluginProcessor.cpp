#include "PluginProcessor.h"
#include "PluginEditor.h"

OptoProcessor::OptoProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "OPTO", createLayout())
{
    compression = apvts.getRawParameterValue ("compression");
    output = apvts.getRawParameterValue ("output");
    bypass = apvts.getRawParameterValue ("bypass");
}

juce::AudioProcessorValueTreeState::ParameterLayout OptoProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "compression", 1 }, "Compression", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output trim (dB)", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    return l;
}

bool OptoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

double OptoProcessor::autoMakeupDb() const
{
    return makeup::lookup ((double) compression->load(), limitMode);
}

void OptoProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    opto::Switches s;                                    // the fixed sound of FastVocal Comp
    s.symmetric = true;
    s.smoothLevel = false;                               // natural attack: the detector follows the held crests
    s.lfHold = true;
    s.flatSidechain = false;
    s.lookahead = true;
    s.fastAttack = false;
    s.gapHold = false;
    s.rest = false;
    engine.setSwitches (s);
    lastCompression = -1.0f;
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (totalOutputDb()));
    bypassMix.reset (sampleRate, 0.01);
    bypassMix.setCurrentAndTargetValue (bypass->load() > 0.5f ? 1.0 : 0.0);
    setLatencySamples (engine.lookaheadLengthSamples());
}

void OptoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0)
        return;

    const float c = compression->load();
    if (c != lastCompression)
    {
        engine.setAmount (c, limitMode);
        lastCompression = c;
    }
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (totalOutputDb()));
    bypassMix.setTargetValue (bypass->load() > 0.5f ? 1.0 : 0.0);

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    double maxGr = 0.0;
    for (int i = 0; i < n; ++i)
    {
        double l = L[i], r = R != nullptr ? R[i] : L[i];
        engine.processSample (l, r, outputGain.getNextValue(), 0);
        maxGr = std::max (maxGr, engine.lastReductionDb());
        const double dl = engine.lastDryLeft(), dr = engine.lastDryRight();
        const double m = bypassMix.getNextValue();       // 0 = processed, 1 = original (short crossfade, no click)
        L[i] = (float) (l + m * (dl - l));
        if (R != nullptr)
            R[i] = (float) (r + m * (dr - r));
    }

    float prev = reductionDb.load();
    while ((float) maxGr > prev && ! reductionDb.compare_exchange_weak (prev, (float) maxGr)) {}
    blockCounter.fetch_add (1);
}

juce::AudioProcessorEditor* OptoProcessor::createEditor()
{
    return new OptoEditor (*this);
}

void OptoProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OptoProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OptoProcessor();
}

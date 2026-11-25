#pragma once

// Basic interface: a gain-reduction meter on top of the standard JUCE parameter editor.

#include "PluginProcessor.h"

// Horizontal gain-reduction meter: 0 dB on the left, 30 dB on the right (square-root scale so that
// small reductions stay readable), numeric readout and a 1.5 s peak-hold mark.
class ReductionMeter : public juce::Component
{
public:
    void setValues (float currentDb, float holdDb)
    {
        current = currentDb;
        hold = holdDb;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (8.0f, 4.0f);
        auto text = area.removeFromLeft (150.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText ("Reduction  " + (current < 0.05f ? juce::String ("0.0") : juce::String (-current, 1)) + " dB", text, juce::Justification::centredLeft);

        auto scale = area.removeFromBottom (14.0f);
        auto bar = area.reduced (0.0f, 2.0f);
        g.setColour (juce::Colour (0xff2a2f36));
        g.fillRoundedRectangle (bar, 3.0f);

        const float w = bar.getWidth() * position (current);
        g.setColour (juce::Colour (0xffe0a030));
        g.fillRoundedRectangle (bar.withWidth (w), 3.0f);

        const float hx = bar.getX() + bar.getWidth() * position (hold);
        g.setColour (juce::Colours::white);
        g.fillRect (juce::Rectangle<float> (hx - 1.0f, bar.getY(), 2.0f, bar.getHeight()));

        g.setFont (juce::FontOptions (11.0f));
        g.setColour (juce::Colours::lightgrey);
        for (float db : { 0.0f, 1.0f, 3.0f, 6.0f, 10.0f, 20.0f, 30.0f })
        {
            const float x = scale.getX() + scale.getWidth() * position (db);
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (x - 12.0f, scale.getY(), 24.0f, scale.getHeight()),
                        juce::Justification::centred);
        }
    }

private:
    static float position (float db) { return std::sqrt (juce::jlimit (0.0f, 30.0f, db) / 30.0f); }
    float current = 0.0f, hold = 0.0f;
};

class OptoEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit OptoEditor (OptoProcessor& p) : AudioProcessorEditor (p), processor (p), generic (p)
    {
        addAndMakeVisible (meter);
        addAndMakeVisible (generic);
        setSize (juce::jmax (420, generic.getWidth()), generic.getHeight() + meterHeight);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1b1f24)); }

    void resized() override
    {
        auto r = getLocalBounds();
        meter.setBounds (r.removeFromTop (meterHeight));
        generic.setBounds (r);
    }

private:
    void timerCallback() override
    {
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const unsigned int count = processor.processedBlocks();
        if (count != lastCount)                       // new audio since last tick: take its largest reduction
        {
            lastCount = count;
            lastAudioTime = t;
            latest = processor.readAndResetReductionDb();
        }
        else if (t - lastAudioTime > 0.25)            // no audio for 250 ms (transport stopped): fall back to 0
        {
            latest = 0.0f;
        }
        // display ballistics: instant rise, 300 ms fall (display only)
        shown = latest >= shown ? latest : shown + (latest - shown) * (1.0f - std::exp (-1.0f / (0.3f * 30.0f)));
        if (latest >= holdValue || t - holdTime > 1.5)
        {
            holdValue = latest;
            holdTime = t;
        }
        meter.setValues (shown, holdValue);
    }

    static constexpr int meterHeight = 60;
    OptoProcessor& processor;
    juce::GenericAudioProcessorEditor generic;
    ReductionMeter meter;
    float shown = 0.0f, holdValue = 0.0f, latest = 0.0f;
    double holdTime = 0.0, lastAudioTime = 0.0;
    unsigned int lastCount = 0;
};

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class HatsOffEditor : public juce::AudioProcessorEditor,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit HatsOffEditor (HatsOffProcessor&);
    ~HatsOffEditor() override = default;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { fileHover = true; }
    void fileDragExit (const juce::StringArray&) override { fileHover = false; }
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    void timerCallback() override { repaint(); }

    enum Kind { None, BtnRun, BtnSeq, BtnGen, BtnClear, BtnUndo, BtnRNotes, BtnRVel, BtnRAll,
                BtnDrag, BtnDragLen, BtnEmbed, BtnArrange, BtnMidiOut, BtnChoke, Wave, StepBar, StepRoll, Slider };
    struct Target { Kind kind = None; int index = -1; };

    struct SliderDef
    {
        const char* id; const char* label;
        float lo, hi, def, step, wheel, origin;
        juce::Colour colour;
        juce::Rectangle<float> rect;
    };

    void layout();
    Target findTarget (juce::Point<float>) const;
    juce::Rectangle<float> buttonRect (Kind) const;
    int numSteps() const;
    juce::Rectangle<float> stepRect (int) const;
    juce::Rectangle<float> rollRect (int) const;
    int stepAtX (float x) const;
    juce::String formatValue (int sliderIndex, float v) const;
    void setSliderValue (int sliderIndex, float v);
    void startMidiDrag();

    void drawPanel (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, juce::Colour dot) const;
    void drawPill (juce::Graphics&, juce::Rectangle<float>, const juce::String& text, juce::Colour fill, juce::Colour textCol, float fontSize = 13.0f) const;
    void drawSlider (juce::Graphics&, int index) const;

    HatsOffProcessor& proc;

    juce::Rectangle<float> pTitle, pTransport, pPattern, pRandom, pClip, pSample, pSteps, pSound, pGroove, pGen, pOut;
    juce::Rectangle<float> bRun, bSeq, bGen, bClear, bUndo, bRN, bRV, bRA, bDrag, bDragLen, bEmbed, bArrange, bMidiOut, bChoke, waveRect;
    float sx = 0, sy = 0, sw = 0, sbh = 0, numY = 0, rollY = 0;
    static constexpr float rollH = 16.0f;
    std::array<SliderDef, 10> sliders;

    Target hover, active;
    float dragStartX = 0, dragStartVal = 0;
    // step editing: 1 = paint on, 2 = velocity drag (pending), 3 = erase
    int stepMode = 0, dragStep = -1, dragStartY = 0, dragStartVel = 0;
    bool stepMoved = false, midiDragging = false, fileHover = false;
    juce::Point<float> mousePos { -1.0f, -1.0f };
    juce::String message;
    double messageTime = -100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HatsOffEditor)
};

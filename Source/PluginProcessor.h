#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

class HatsOffProcessor : public juce::AudioProcessor
{
public:
    static constexpr int MaxSteps = 32;
    static constexpr int NumPeaks = 512;

    HatsOffProcessor();
    ~HatsOffProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HatsOff"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- parameters ----
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float param (const char* id) const;
    void setParam (const char* id, float plainValue);   // message thread

    // ---- pattern (edited on the message thread, read by the audio thread) ----
    std::array<std::atomic<int>, MaxSteps> stepOn {}, stepVel {}, stepRoll {};
    void setStep (int s, bool isOn, int velocity, int roll);
    void generate();
    void randomizeNotes();
    void randomizeVelocity();
    void randomizeAll();
    void clearPattern();
    void undo();
    bool hasUndo() const { return undoAvailable; }
    static double stepBeatsFor (int rateIndex);
    int stepClass (int step) const;     // 0 = on the beat, 1 = off-beat 8th, 2 = other

    // ---- sample ----
    struct Sample
    {
        juce::AudioBuffer<float> audio;
        double rate = 44100.0;
        std::array<float, NumPeaks> peaks {};
        float peakMax = 1.0f;
    };
    bool loadFile (const juce::File& file, juce::String& error);
    std::shared_ptr<const Sample> getSample() const { return std::atomic_load (&sample); }
    juce::String getSampleName() const;
    bool isSampleMissing() const { return missing.load(); }
    double getLoadErrorTime() const { return loadErrorTime.load(); }

    // ---- display / control from the editor ----
    std::atomic<int> displayStep { -1 };
    std::atomic<bool> displayPlaying { false };
    std::atomic<float> voicePosition { -1.0f };   // 0..1 of the latest voice, -1 = silent
    std::atomic<bool> previewRequest { false };

    // ---- MIDI export ----
    juce::File writeMidiFile();
    double patternBeats() const;

private:
    // pattern helpers (message thread)
    struct PatternCopy { std::array<int, MaxSteps> on {}, vel {}, roll {}; };
    PatternCopy copyPattern() const;
    void applyPattern (const PatternCopy&);
    void pushUndo();
    void generateInto (PatternCopy&);
    void randomizeNotesInto (PatternCopy&);
    void randomizeVelocityInto (PatternCopy&);
    int makeVelocity (int cls);
    PatternCopy undoPattern;
    bool undoAvailable = false;
    juce::Random rng;

    // sample storage: old samples are kept alive so the audio thread never frees memory
    std::shared_ptr<const Sample> sample;
    std::vector<std::shared_ptr<const Sample>> retired;
    juce::AudioFormatManager formats;
    juce::String sampleName, samplePath;
    mutable juce::CriticalSection nameLock;
    std::atomic<bool> missing { false };
    std::atomic<double> loadErrorTime { -100.0 };
    std::shared_ptr<const Sample> makeSample (juce::AudioBuffer<float>&& audio, double rate) const;
    bool readFile (const juce::File& f, juce::AudioBuffer<float>& dest, double& rate, juce::String& error);
    juce::String encodeSample() const;
    bool decodeSample (const juce::String& base64);

    // playback
    struct Voice
    {
        bool active = false, fading = false;
        double pos = 0.0, inc = 1.0;
        float amp = 0.0f, fade = 1.0f;
    };
    std::array<Voice, 8> voices;
    int lastVoice = 0, voiceRR = 0;
    void startVoice (const Sample& s, int velocity);

    struct Hit { int offset = 0; int velocity = 0; double length = 0.0; bool generated = false; };
    std::array<Hit, 512> hits;
    int numHits = 0;
    void addHit (int offset, int velocity, double length, bool generated);

    juce::MidiBuffer outMidi;
    double sr = 44100.0;
    double freePos = 0.0, prevB1 = -1000.0;
    bool wasRun = false, wasTransportPlaying = false;
    bool pendingOff = false;
    double pendingOffset = 0.0;
    int pendingNote = 42;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HatsOffProcessor)
};

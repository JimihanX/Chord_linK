#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChordEngine.h"
#include "Synth.h"

#ifndef CL_INSTRUMENT
 #define CL_INSTRUMENT 1
#endif

#define CL_VERSION_STRING "v1.0 (plugin)"

// 화면 → 오디오 스레드로 보내는 명령
struct ClCommand
{
    enum Type { NoteOn, NoteOff, HoldOn, HoldOff, ToggleEdit, ResetEdit, VoiceKey, SetCV, Panic };
    Type type = NoteOn;
    int a = 0, b = 0;
    char kind = 0;
};

// 화면 표시용 스냅샷 (타이머에서 복사)
struct ClSnapshot
{
    juce::String chordName, voicingName, notesText;
    std::vector<int> outNotes;
    bool anyHeld = false, v2 = false, face = false, custom = false, flat = false;
    cl::Formula current, effective;
    int faceLevel = 5, voiceKeyIndex = 0;
    float emotion = 0.0f;
    std::array<juce::String, cl::NUM_SLOTS> slotLabels;
    std::array<bool, cl::NUM_SLOTS> slotActive {};
    std::array<juce::String, cl::NUM_CV> cv;
    std::array<bool, 128> down {};
    int flashSlot = -1;
};

class ChordLinkProcessor : public juce::AudioProcessor
{
public:
    ChordLinkProcessor();
    ~ChordLinkProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return CL_INSTRUMENT == 0; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // 화면에서 사용
    void pushCommand (const ClCommand& c);
    void setCustomVoicingText (int index, const juce::String& text);
    ClSnapshot snapshot();

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // 테스트용: 엔진에 직접 접근
    cl::Engine& getEngineForTest() { return engine; }

private:
    static BusesProperties makeBuses();
    void syncParams (std::vector<cl::MidiOut>& out);
    void drainCommands (std::vector<cl::MidiOut>& out);

    cl::Engine engine;
    juce::SpinLock engineLock;

    juce::AbstractFifo fifo { 512 };
    std::array<ClCommand, 512> cmdBuf;
    juce::SpinLock fifoWriteLock;

    std::array<juce::String, cl::NUM_CV> pendingCV;
    std::array<bool, cl::NUM_CV> pendingCVFlag {};
    juce::SpinLock cvLock;

    std::atomic<float> ccEmotion { -2.0f }; // CC20 으로 들어온 값 (-2 = 없음)
    float lastEmotionParam = 0.0f;

    std::atomic<int> soundType { cl::SND_PIANO };
    juce::Synthesiser synth;
    juce::MidiBuffer outMidi;
    std::vector<cl::MidiOut> tmp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordLinkProcessor)
};

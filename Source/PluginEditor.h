#pragma once
#include "PluginProcessor.h"

// 누르는 동안 동작하는 버튼 (코드 칩·슬롯·보이싱)
class ChipButton : public juce::Component
{
public:
    juce::String text, sub;
    juce::Colour base { juce::Colours::white }, accent { juce::Colours::orange }, ink { juce::Colours::black };
    bool active = false, dashed = false, enabledLook = true;
    std::function<void()> onDown, onUp;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (enabledLook && onDown) onDown(); pressed = true; }
    void mouseUp (const juce::MouseEvent&) override { if (pressed && onUp) onUp(); pressed = false; }
private:
    bool pressed = false;
};

// 건반 그림 (입력용 C1~B4 / 출력 미리보기 C2~C7)
class KeyboardView : public juce::Component
{
public:
    KeyboardView (int lowNote, int highNote, bool isInteractive) : lo (lowNote), hi (highNote), interactive (isInteractive) {}
    std::function<void (int)> onNoteDown, onNoteUp;
    std::function<juce::Colour (int, bool)> keyColour;    // (노트, 흑건 여부)
    std::function<juce::String (int)> keyLabel;
    std::function<juce::Colour (int)> labelColour;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    juce::Rectangle<float> keyRect (int note) const;
    int noteAt (juce::Point<float> p) const;
    int whiteCount() const;
    int lo, hi;
    bool interactive;
    int pressedNote = -1;
};

class ChordLinkEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ChordLinkEditor (ChordLinkProcessor&);
    ~ChordLinkEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refresh();
    void layoutVoicing (juce::Rectangle<int> r);
    void layoutChord (juce::Rectangle<int> r);
    juce::Colour zoneKeyColour (int note, bool black) const;
    juce::String zoneKeyLabel (int note) const;

    ChordLinkProcessor& proc;
    ClSnapshot snap;
    juce::Rectangle<int> voicePanel, notePanel, chordPanel, kbArea, outArea;

    juce::ToggleButton v2Btn, faceBtn, customBtn, flatBtn, vlBtn;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BA> v2Att, faceAtt, customAtt, flatAtt, vlAtt;
    juce::TextButton panicBtn;
#if CL_INSTRUMENT
    juce::ComboBox soundBox;
    juce::Slider volSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> soundAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volAtt;
#endif
    juce::Slider emoSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> emoAtt;

    juce::OwnedArray<ChipButton> voiceChips, cvChips, qChips, sChips, tChips, slotChips;
    ChipButton resetChip;
    juce::TextEditor cvEdit;
    juce::TextButton cvSave;
    juce::Label cvErr;
    int cvEditIndex = 0;

    KeyboardView inKb { 24, 71, true }, outKb { 36, 96, false };
    std::array<juce::Label, 2> dummy;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChordLinkEditor)
};

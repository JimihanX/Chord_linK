#include "PluginEditor.h"

namespace
{
juce::String K (const char* s) { return juce::String::fromUTF8 (s); }
const juce::Colour BG (0xff0b1220), SHELL (0xff0e1726), INKD (0xff0f172a);
const juce::Colour VOICE (0xff12906b), VOICE_BG (0xffe4f2dc), VOICE_INK (0xff0f3d2e), VCUST (0xffa7f3d0), GUITAR (0xffb45309);
const juce::Colour NOTE (0xff2f6feb), NOTE_BG (0xffddeafb), DISP (0xff0f2635);
const juce::Colour CHORD (0xffea580c), CHORD_BG (0xfffcebdd), CHORD_INK (0xff431407), CUSTOM (0xfffdba74);
const juce::Colour MUTED (0xff94a3b8);

bool isBlack (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }

void drawHeader (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c, const juce::String& title, const juce::String& sub)
{
    g.setColour (c);
    g.fillRoundedRectangle (r.toFloat().withHeight (44.0f + 10.0f), 10.0f);
    g.fillRect (r.withTop (r.getY() + 34).withHeight (10));
    g.setColour (juce::Colours::white);
    g.setFont (font (19.0f, true));
    g.drawText (title, r.getX() + 12, r.getY() + 3, 200, 24, juce::Justification::centredLeft);
    g.setFont (font (11.5f));
    g.drawText (sub, r.getX() + 12, r.getY() + 24, 220, 16, juce::Justification::centredLeft);
}
} // namespace

// ================================================================ ChipButton
void ChipButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float alpha = enabledLook ? 1.0f : 0.35f;
    g.setColour ((active ? accent : base).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour ((active ? accent : accent.withAlpha (0.45f)).withMultipliedAlpha (alpha));
    if (dashed && ! active)
    {
        juce::Path p; p.addRoundedRectangle (r, 6.0f);
        juce::Path d; const float dl[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (d, p, dl, 2);
        g.fillPath (d);
    }
    else g.drawRoundedRectangle (r, 6.0f, 1.0f);
    g.setColour ((active ? juce::Colours::white : ink).withMultipliedAlpha (alpha));
    if (sub.isEmpty())
    {
        g.setFont (font (juce::jmin (14.0f, r.getHeight() * 0.55f), true));
        g.drawFittedText (text, r.toNearestInt().reduced (3, 0), juce::Justification::centred, 1, 0.7f);
    }
    else
    {
        auto top = r.removeFromTop (r.getHeight() * 0.58f);
        g.setFont (font (juce::jmin (13.5f, top.getHeight() * 0.8f), true));
        g.drawFittedText (text, top.toNearestInt().reduced (2, 0), juce::Justification::centredBottom, 1, 0.7f);
        g.setFont (font (9.5f));
        g.drawFittedText (sub, r.toNearestInt().reduced (2, 0), juce::Justification::centredTop, 1, 0.7f);
    }
}

// ================================================================ KeyboardView
int KeyboardView::whiteCount() const
{
    int c = 0;
    for (int n = lo; n <= hi; ++n) if (! isBlack (n)) ++c;
    return c;
}
// 흰건반·흑건 위치를 한 함수에서 계산 (흑건은 두 흰건반 경계 정중앙)
juce::Rectangle<float> KeyboardView::keyRect (int note) const
{
    const float ww = (float) getWidth() / (float) whiteCount();
    const float h = (float) getHeight() - 16.0f;
    int wi = 0;
    for (int n = lo; n < note; ++n) if (! isBlack (n)) ++wi;
    if (! isBlack (note)) return { wi * ww, 0.0f, ww, h };
    const float bw = ww * 0.6f;
    return { wi * ww - bw * 0.5f, 0.0f, bw, h * 0.62f };
}
int KeyboardView::noteAt (juce::Point<float> p) const
{
    for (int n = lo; n <= hi; ++n) if (isBlack (n) && keyRect (n).contains (p)) return n;
    for (int n = lo; n <= hi; ++n) if (! isBlack (n) && keyRect (n).contains (p)) return n;
    return -1;
}
void KeyboardView::paint (juce::Graphics& g)
{
    for (int pass = 0; pass < 2; ++pass)
        for (int n = lo; n <= hi; ++n)
        {
            const bool b = isBlack (n);
            if ((pass == 0) == b) continue;
            auto r = keyRect (n);
            g.setColour (keyColour ? keyColour (n, b) : (b ? INKD : juce::Colours::white));
            if (b) g.fillRoundedRectangle (r, 2.5f);
            else { g.fillRect (r.reduced (0.5f, 0.0f)); g.setColour (juce::Colour (0xff7b8288)); g.drawRect (r, 0.8f); }
            if (keyLabel)
            {
                auto txt = keyLabel (n);
                if (txt.isNotEmpty())
                {
                    g.setColour (labelColour ? labelColour (n) : juce::Colours::black);
                    g.setFont (font (b ? 8.5f : 10.0f, true));
                    g.drawFittedText (txt, r.toNearestInt().withTrimmedTop ((int) (r.getHeight() - 18)), juce::Justification::centred, 1, 0.6f);
                }
            }
        }
    g.setColour (juce::Colour (0xffcfe3f0));
    g.setFont (font (11.0f, true));
    for (int n = lo; n <= hi; ++n)
        if (n % 12 == 0)
        {
            auto r = keyRect (n);
            g.drawText ("C" + juce::String (n / 12 - 1), (int) r.getX() + 3, getHeight() - 15, 40, 14, juce::Justification::centredLeft);
        }
}
void KeyboardView::mouseDown (const juce::MouseEvent& e)
{
    if (! interactive) return;
    pressedNote = noteAt (e.position);
    if (pressedNote >= 0 && onNoteDown) onNoteDown (pressedNote);
}
void KeyboardView::mouseUp (const juce::MouseEvent&)
{
    if (pressedNote >= 0 && onNoteUp) onNoteUp (pressedNote);
    pressedNote = -1;
}

// ================================================================ Editor
ChordLinkEditor::ChordLinkEditor (ChordLinkProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    auto setupToggle = [this] (juce::ToggleButton& b, const juce::String& text, const char* id, std::unique_ptr<BA>& att) {
        b.setButtonText (text);
        b.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        b.setColour (juce::ToggleButton::tickColourId, juce::Colours::white);
        b.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::white.withAlpha (0.7f));
        addAndMakeVisible (b);
        att = std::make_unique<BA> (proc.apvts, id, b);
    };
    setupToggle (v2Btn, K ("보이싱 2"), "v2", v2Att);
    setupToggle (faceBtn, K ("얼굴인식 모드"), "face", faceAtt);
    setupToggle (customBtn, K ("커스텀"), "custom", customAtt);
    setupToggle (flatBtn, K ("플랫(♭) 표기"), "flat", flatAtt);
    setupToggle (vlBtn, K ("보이스 리딩"), "vl", vlAtt);
    // 얼굴인식과 커스텀은 동시에 켜지지 않음
    faceBtn.onClick = [this] { if (faceBtn.getToggleState()) if (auto* q = proc.apvts.getParameter ("custom")) q->setValueNotifyingHost (0.0f); };
    customBtn.onClick = [this] { if (customBtn.getToggleState()) if (auto* q = proc.apvts.getParameter ("face")) q->setValueNotifyingHost (0.0f); };

    panicBtn.setButtonText (K ("모든 소리 끄기"));
    panicBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff7f1d1d));
    panicBtn.onClick = [this] { proc.pushCommand ({ ClCommand::Panic, 0, 0, 0 }); };
    addAndMakeVisible (panicBtn);

#if CL_INSTRUMENT
    soundBox.addItemList ({ K ("1 피아노"), K ("2 사인파"), K ("3 쏘우파"), K ("4 스퀘어") }, 1);
    addAndMakeVisible (soundBox);
    soundAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "sound", soundBox);
    volSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (volSlider);
    volAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "volume", volSlider);
#endif
    emoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    emoSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    emoSlider.setColour (juce::Slider::thumbColourId, CHORD);
    emoSlider.setColour (juce::Slider::trackColourId, CUSTOM);
    addChildComponent (emoSlider);
    emoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "emotion", emoSlider);

    // 보이싱 칩 (최대 9개) + 커스텀 보이싱 (6개)
    for (int i = 0; i < 9; ++i)
    {
        auto* c = voiceChips.add (new ChipButton());
        c->base = juce::Colours::white; c->accent = VOICE; c->ink = VOICE_INK;
        c->onDown = [this, i] { proc.pushCommand ({ ClCommand::VoiceKey, i, 0, 0 }); };
        addAndMakeVisible (c);
    }
    for (int i = 0; i < cl::NUM_CV; ++i)
    {
        auto* c = cvChips.add (new ChipButton());
        c->base = VOICE_BG; c->accent = VOICE; c->ink = VOICE_INK; c->dashed = true;
        c->onDown = [this, i] {
            const int n = (int) cl::presetList (snap.v2).size();
            cvEditIndex = i;
            cvEdit.setText (snap.cv[(size_t) i], false);
            cvErr.setText ({}, juce::dontSendNotification);
            proc.pushCommand ({ ClCommand::VoiceKey, n + i, 0, 0 });
        };
        addAndMakeVisible (c);
    }
    cvEdit.setFont (font (13.0f));
    addAndMakeVisible (cvEdit);
    cvSave.setButtonText (K ("저장"));
    cvSave.setColour (juce::TextButton::buttonColourId, VOICE);
    cvSave.onClick = [this] {
        std::vector<std::string> tk;
        std::string err;
        const auto txt = cvEdit.getText().toStdString();
        if (! cl::parseCustomVoicing (txt, tk, err)) { cvErr.setText (juce::String::fromUTF8 (err.c_str()), juce::dontSendNotification); return; }
        std::string joined;
        for (size_t i = 0; i < tk.size(); ++i) joined += (i ? "-" : "") + tk[i];
        cvErr.setText ({}, juce::dontSendNotification);
        proc.setCustomVoicingText (cvEditIndex, juce::String::fromUTF8 (joined.c_str()));
    };
    addAndMakeVisible (cvSave);
    cvErr.setColour (juce::Label::textColourId, juce::Colour (0xffb91c1c));
    cvErr.setFont (font (11.0f));
    addAndMakeVisible (cvErr);

    // 코드 칩: 누르는 동안만 적용 (커스텀 지정 모드에서는 고정 선택)
    auto makeChord = [this] (juce::OwnedArray<ChipButton>& arr, char kind, int value, const juce::String& txt, int key) {
        auto* c = arr.add (new ChipButton());
        c->text = txt;
        c->sub = key >= 0 ? juce::String (cl::noteName (key, false)) : K (kind == 'q' ? "기본" : "화면");
        c->base = juce::Colours::white; c->accent = CHORD; c->ink = CHORD_INK;
        const int id = 1000 + (int) kind * 16 + value;
        c->onDown = [this, kind, value, id] {
            if (snap.custom) proc.pushCommand ({ ClCommand::ToggleEdit, 0, value, kind });
            else proc.pushCommand ({ ClCommand::HoldOn, id, value, kind });
        };
        c->onUp = [this, id] { if (! snap.custom) proc.pushCommand ({ ClCommand::HoldOff, id, 0, 0 }); };
        addAndMakeVisible (c);
    };
    auto keyOf = [] (char k, int v) {
        for (size_t i = 0; i < cl::CHORD_KEYS.size(); ++i)
            if (cl::CHORD_KEYS[i].kind == k && cl::CHORD_KEYS[i].value == v) return cl::CHORD_LO + (int) i;
        return -1;
    };
    for (int q = 0; q < cl::NUM_Q; ++q) makeChord (qChips, 'q', q, cl::qualityName (q), q == cl::MAJ ? -1 : keyOf ('q', q));
    makeChord (sChips, 's', cl::S_7, "7", keyOf ('s', cl::S_7));
    makeChord (sChips, 's', cl::S_M7, "M7", keyOf ('s', cl::S_M7));
    for (int t = 0; t < cl::NUM_T; ++t) makeChord (tChips, 't', t, cl::tensionName (t), keyOf ('t', t));
    for (int i = 0; i < cl::NUM_SLOTS; ++i)
    {
        auto* c = slotChips.add (new ChipButton());
        c->sub = cl::noteName (cl::SLOT_LO + i, false);
        c->base = juce::Colours::white; c->accent = CHORD; c->ink = CHORD_INK;
        const int id = 2000 + i;
        c->onDown = [this, i, id] {
            if (snap.custom) proc.pushCommand ({ ClCommand::ToggleEdit, 0, i, 'l' });
            else proc.pushCommand ({ ClCommand::HoldOn, id, i, 'l' });
        };
        c->onUp = [this, id] { if (! snap.custom) proc.pushCommand ({ ClCommand::HoldOff, id, 0, 0 }); };
        addAndMakeVisible (c);
    }
    resetChip.text = K ("코드 초기화");
    resetChip.base = juce::Colours::white; resetChip.accent = CHORD; resetChip.ink = CHORD_INK;
    resetChip.onDown = [this] { proc.pushCommand ({ ClCommand::ResetEdit, 0, 0, 0 }); };
    addChildComponent (resetChip);

    // 건반
    inKb.keyColour = [this] (int n, bool b) { return zoneKeyColour (n, b); };
    inKb.keyLabel = [this] (int n) { return zoneKeyLabel (n); };
    inKb.labelColour = [] (int n) { return isBlack (n) ? juce::Colour (0xffe2e8f0) : juce::Colour (0xff334155); };
    inKb.onNoteDown = [this] (int n) { proc.pushCommand ({ ClCommand::NoteOn, n, 0, 0 }); };
    inKb.onNoteUp = [this] (int n) { proc.pushCommand ({ ClCommand::NoteOff, n, 0, 0 }); };
    addAndMakeVisible (inKb);
    outKb.keyColour = [this] (int n, bool b) {
        const bool on = std::find (snap.outNotes.begin(), snap.outNotes.end(), n) != snap.outNotes.end() && snap.anyHeld;
        if (on) return b ? juce::Colour (0xff15803d) : juce::Colour (0xff4ade80);
        return b ? juce::Colour (0xff1b1f23) : juce::Colour (0xfff6f6f3);
    };
    addAndMakeVisible (outKb);

    setSize (1000, 660);
    snap = proc.snapshot();
    refresh();
    startTimerHz (30);
}

ChordLinkEditor::~ChordLinkEditor() { stopTimer(); }

juce::Colour ChordLinkEditor::zoneKeyColour (int n, bool b) const
{
    const auto& eff = snap.current;
    juce::Colour w, k, on;
    bool active = false, dim = false;
    if (n <= cl::NOTE_HI) { w = juce::Colour (0xffd6e4fb); k = juce::Colour (0xff1e3a6e); on = NOTE; }
    else if (n <= cl::CHORD_HI)
    {
        w = juce::Colour (0xfff7c6a3); k = juce::Colour (0xff4a220f); on = CHORD;
        const auto a = cl::CHORD_KEYS[(size_t) (n - cl::CHORD_LO)];
        active = ! snap.face && (a.kind == 'q' ? eff.q == a.value : a.kind == 's' ? eff.s == a.value : eff.has (a.value));
        dim = snap.face;
    }
    else if (n <= cl::SLOT_HI)
    {
        w = juce::Colour (0xfffff0d2); k = juce::Colour (0xff7a4a12); on = juce::Colour (0xfff97316);
        active = ! snap.face && snap.slotActive[(size_t) (n - cl::SLOT_LO)];
        dim = snap.face;
    }
    else
    {
        const int np = (int) cl::presetList (snap.v2).size();
        const bool cust = (n - cl::VOICE_LO) >= np;
        w = cust ? juce::Colour (0xffb9f2db) : juce::Colour (0xffc9ebdb);
        k = cust ? juce::Colour (0xff0e6b52) : juce::Colour (0xff0b4d3a);
        on = cust ? juce::Colour (0xff34d399) : VOICE;
        active = (n - cl::VOICE_LO) == snap.voiceKeyIndex;
    }
    juce::Colour c = active ? on : (b ? k : w);
    if (snap.down[(size_t) n]) c = n <= cl::NOTE_HI ? juce::Colour (0xff38bdf8) : juce::Colour (0xfffacc15);
    return dim ? c.interpolatedWith (SHELL, 0.6f) : c;
}

juce::String ChordLinkEditor::zoneKeyLabel (int n) const
{
    if (n <= cl::NOTE_HI) return juce::String (cl::pcName (n, snap.flat));
    if (n <= cl::CHORD_HI)
    {
        const auto a = cl::CHORD_KEYS[(size_t) (n - cl::CHORD_LO)];
        return a.kind == 'q' ? cl::qualityName (a.value) : a.kind == 's' ? (a.value == cl::S_7 ? "7" : "M7") : cl::tensionName (a.value);
    }
    if (n <= cl::SLOT_HI) return juce::String (n - cl::SLOT_LO + 1);
    const auto& list = cl::presetList (snap.v2);
    const int i = n - cl::VOICE_LO;
    if (i < (int) list.size()) return cl::voicingShort (list[(size_t) i]);
    const int ci = i - (int) list.size();
    return ci < (snap.v2 ? 3 : 6) ? "V" + juce::String (ci + 1) : juce::String();
}

void ChordLinkEditor::timerCallback()
{
    snap = proc.snapshot();
    refresh();
}

void ChordLinkEditor::refresh()
{
    const auto& list = cl::presetList (snap.v2);
    for (int i = 0; i < voiceChips.size(); ++i)
    {
        auto* c = voiceChips[i];
        c->setVisible (i < (int) list.size());
        if (i < (int) list.size())
        {
            const auto id = list[(size_t) i];
            c->text = juce::String (cl::noteName (cl::VOICE_LO + i, snap.flat)) + "   " + cl::voicingName (id);
            c->accent = cl::voicingIsGuitar (id) ? GUITAR : VOICE;
            c->active = snap.voiceKeyIndex == i;
        }
    }
    const int nC = snap.v2 ? 3 : 6;
    for (int i = 0; i < cvChips.size(); ++i)
    {
        auto* c = cvChips[i];
        c->setVisible (i < nC);
        c->text = snap.cv[(size_t) i];
        c->sub = cl::noteName (cl::VOICE_LO + (int) list.size() + i, snap.flat);
        c->active = snap.voiceKeyIndex == (int) list.size() + i;
    }
    if (cvEditIndex >= nC) cvEditIndex = 0;
    if (! cvEdit.hasKeyboardFocus (true) && cvEdit.getText() != snap.cv[(size_t) cvEditIndex])
        cvEdit.setText (snap.cv[(size_t) cvEditIndex], false);

    const auto& f = snap.current;
    for (int q = 0; q < qChips.size(); ++q) { qChips[q]->active = f.q == q; qChips[q]->enabledLook = ! snap.face; }
    sChips[0]->active = f.s == cl::S_7; sChips[1]->active = f.s == cl::S_M7;
    for (int t = 0; t < tChips.size(); ++t) tChips[t]->active = f.has (t);
    for (int i = 0; i < slotChips.size(); ++i)
    {
        slotChips[i]->text = snap.slotLabels[(size_t) i];
        slotChips[i]->active = snap.slotActive[(size_t) i];
        slotChips[i]->dashed = snap.custom;
    }
    auto showChord = ! snap.face;
    for (auto* arr : { &qChips, &sChips, &tChips, &slotChips })
        for (auto* c : *arr) c->setVisible (showChord);
    resetChip.setVisible (snap.custom);
    emoSlider.setVisible (snap.face);
    layoutVoicing (voicePanel);
    layoutChord (chordPanel);
    repaint();
}

void ChordLinkEditor::paint (juce::Graphics& g)
{
    g.fillAll (BG);
    g.setColour (juce::Colours::white);
    g.setFont (font (24.0f, true));
    g.drawText ("Chord Link", 16, 10, 160, 30, juce::Justification::centredLeft);
    g.setColour (MUTED);
    g.setFont (font (11.0f));
    g.drawText (CL_INSTRUMENT ? K ("악기형 · MIDI 출력") : K ("MIDI FX"), 172, 18, 120, 16, juce::Justification::centredLeft);

    g.setColour (SHELL);
    g.fillRoundedRectangle (getLocalBounds().withTrimmedTop (52).reduced (8, 4).toFloat(), 14.0f);

    // 패널 배경·헤더
    g.setColour (VOICE_BG); g.fillRoundedRectangle (voicePanel.toFloat(), 10.0f);
    g.setColour (NOTE_BG);  g.fillRoundedRectangle (notePanel.toFloat(), 10.0f);
    g.setColour (CHORD_BG); g.fillRoundedRectangle (chordPanel.toFloat(), 10.0f);
    drawHeader (g, voicePanel, VOICE, snap.v2 ? "Voicing 2" : "Voicing", snap.v2 ? K ("6~9번 기타 보이싱 · C4~B4") : K ("보이싱 선택 · C4~B4"));
    drawHeader (g, notePanel, NOTE, "Note", K ("근음 입력 · C1~B1"));
    drawHeader (g, chordPanel, CHORD, "Chord", snap.face ? K ("표정으로 코드 결정") : snap.custom ? K ("커스텀 코드 지정") : K ("코드 종류 · C2~C3"));

    // 보이싱 패널 라벨
    {
        const auto& list = cl::presetList (snap.v2);
        auto r = cvChips[0]->getBounds();
        g.setColour (VOICE_INK);
        g.setFont (font (11.0f, true));
        g.drawText (K ("커스텀 보이싱 · ") + cl::noteName (cl::VOICE_LO + (int) list.size(), snap.flat) + "~B4  " + K ("(도수를 아래→위로)"),
                    voicePanel.getX() + 10, r.getY() - 16, voicePanel.getWidth() - 20, 14, juce::Justification::centredLeft);
    }

    // 최종 출력 코드
    {
        auto d = notePanel.withTrimmedTop (50).reduced (10);
        g.setColour (DISP);
        g.fillRoundedRectangle (d.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff9ccbea));
        g.setFont (font (12.0f));
        g.drawText (K ("최종 출력 코드"), d.getX() + 12, d.getY() + 8, 150, 16, juce::Justification::centredLeft);
        g.drawText (snap.anyHeld ? K ("● 연주 중") : K ("미리보기"), d.getRight() - 110, d.getY() + 8, 98, 16, juce::Justification::centredRight);
        g.setColour (juce::Colour (0xffe8f6ff).withAlpha (snap.anyHeld ? 1.0f : 0.5f));
        g.setFont (font (44.0f, true));
        g.drawFittedText (snap.chordName, d.getX() + 6, d.getY() + 60, d.getWidth() - 12, 60, juce::Justification::centred, 1, 0.5f);
        g.setColour (juce::Colour (0xff86efac));
        g.setFont (font (13.5f, true));
        g.drawFittedText (snap.voicingName, d.getX() + 6, d.getY() + 128, d.getWidth() - 12, 20, juce::Justification::centred, 1, 0.6f);
        if (snap.face || snap.custom)
        {
            g.setColour (CUSTOM);
            juce::String s;
            if (snap.face)
            {
                const int j = juce::roundToInt (juce::jmax (0.0f, snap.emotion) * 100.0f), sd = juce::roundToInt (juce::jmax (0.0f, -snap.emotion) * 100.0f);
                s = snap.emotion >= 0 ? K ("기쁨 ") + juce::String (j) + "%" : K ("슬픔 ") + juce::String (sd) + "%";
            }
            else s = K ("커스텀 지정 모드");
            g.drawText (s, d.getX(), d.getY() + 150, d.getWidth(), 18, juce::Justification::centred);
        }
        g.setColour (juce::Colour (0xffcfe3f0));
        g.setFont (font (12.0f));
        g.drawFittedText (snap.notesText, d.getX() + 6, d.getY() + 176, d.getWidth() - 12, 34, juce::Justification::centred, 2, 0.6f);
    }

    // 코드 패널 라벨
    if (! snap.face)
    {
        g.setColour (juce::Colour (0xff7c2d12));
        g.setFont (font (11.0f, true));
        auto lab = [&] (juce::Component& c, const juce::String& s, const juce::String& hint) {
            g.drawText (s, chordPanel.getX() + 10, c.getY() - 15, 150, 13, juce::Justification::centredLeft);
            g.setFont (font (10.0f));
            g.drawText (hint, chordPanel.getRight() - 170, c.getY() - 15, 160, 13, juce::Justification::centredRight);
            g.setFont (font (11.0f, true));
        };
        lab (*qChips[0], K ("코드 성질"), snap.custom ? K ("다시 누르면 maj") : K ("누르고 있는 동안만 적용"));
        lab (*sChips[0], "7th", {});
        lab (*tChips[0], K ("텐션"), snap.custom ? K ("여러 개 선택") : K ("여러 개 함께 누르기"));
        lab (*slotChips[0], K ("커스텀 코드 · C#3~B3"), snap.custom ? K ("누르면 지정") : K ("누르고 있는 동안 적용"));
    }
    else
    {
        auto r = chordPanel.withTrimmedTop (54).reduced (12, 4);
        g.setColour (CHORD_INK);
        g.setFont (font (12.0f, true));
        g.drawText (K ("슬픔"), r.getX(), r.getY(), 60, 16, juce::Justification::centredLeft);
        g.drawText (K ("기쁨"), r.getRight() - 60, r.getY(), 60, 16, juce::Justification::centredRight);
        g.setFont (font (11.0f));
        g.drawFittedText (K ("얼굴인식 앱이 MIDI CC20 으로 표정 값을 보내거나, 위 슬라이더로 직접 조절합니다."),
                          r.getX(), emoSlider.getBottom() + 4, r.getWidth(), 30, juce::Justification::centredLeft, 2);
        int y = emoSlider.getBottom() + 44;
        for (int part = 0; part < 2; ++part)
        {
            g.setFont (font (11.5f, true));
            g.setColour (juce::Colour (0xff7c2d12));
            g.drawText (part == 0 ? K ("밝은 코드 ↑ 기쁨") : K ("어두운 코드 ↓ 슬픔"), r.getX(), y, r.getWidth(), 16, juce::Justification::centredLeft);
            y += 18;
            int x = r.getX();
            for (int k = 0; k < 5; ++k)
            {
                const int idx = part == 0 ? 5 + k : 4 - k;
                const auto lbl = juce::String (cl::labelOrMaj (cl::FACE_LADDER[(size_t) idx]));
                const int w = (int) juce::GlyphArrangement::getStringWidth (font (11.5f, true), lbl) + 12;
                const bool on = idx == snap.faceLevel;
                g.setColour (on ? CHORD : juce::Colours::white);
                g.fillRoundedRectangle ((float) x, (float) y, (float) w, 20.0f, 5.0f);
                g.setColour (on ? juce::Colours::white : CHORD_INK);
                g.setFont (font (11.5f, true));
                g.drawText (lbl, x, y, w, 20, juce::Justification::centred);
                x += w + 4;
            }
            y += 30;
        }
    }

    // 구역 바
    {
        const float ww = (float) kbArea.getWidth() / 28.0f;
        const int np = (int) cl::presetList (snap.v2).size();
        const float vp = np == 9 ? 5.0f : 3.5f;
        struct Z { juce::String a, b; float w; juce::Colour bg, fg; };
        const Z zs[] = {
            { "Note", "C1 ~ B1", 7, NOTE, juce::Colours::white },
            { snap.face ? "Face mode" : "Chord key switch", snap.face ? K ("C2 ~ C3 · 꺼짐") : "C2 ~ C3", 8, CHORD, juce::Colours::white },
            { "Custom chord", "C#3 ~ B3", 6, CUSTOM, CHORD_INK },
            { snap.v2 ? "Voicing 2" : "Voicing", np == 9 ? "C4 ~ G#4" : "C4 ~ F4", vp, VOICE, juce::Colours::white },
            { "Custom", np == 9 ? "A4 ~ B4" : "F#4 ~ B4", 7.0f - vp, VCUST, VOICE_INK } };
        float x = (float) kbArea.getX();
        const int y = kbArea.getY() - 30;
        for (auto& z : zs)
        {
            const float w = z.w * ww;
            g.setColour (z.bg);
            g.fillRect (x, (float) y, w - 2.0f, 28.0f);
            g.setColour (z.fg);
            g.setFont (font (11.5f, true));
            g.drawFittedText (z.a + "  " + z.b, juce::Rectangle<float> (x + 6, (float) y, w - 12, 28).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
            x += w;
        }
    }
    g.setColour (MUTED);
    g.setFont (font (11.0f));
    g.drawText (K ("출력 미리보기 (C2 ~ C7)"), outArea.getX(), outArea.getY() - 16, 300, 14, juce::Justification::centredLeft);
    g.drawText (K ("근음 C1~B1 · 코드 키 C2~C3·커스텀 C#3~B3 은 누르는 동안만 · 보이싱 C4~B4 · 얼굴인식 CC20 · ") + CL_VERSION_STRING,
                16, getHeight() - 22, getWidth() - 32, 16, juce::Justification::centredLeft);
}

void ChordLinkEditor::layoutVoicing (juce::Rectangle<int> panel)
{
    if (panel.isEmpty()) return;
    auto r = panel.withTrimmedTop (50).reduced (8, 2);
    const int np = (int) cl::presetList (snap.v2).size();
    const int rh = np == 9 ? 21 : 27, gap = 3;
    for (int i = 0; i < voiceChips.size(); ++i)
        if (i < np) voiceChips[i]->setBounds (r.removeFromTop (rh)), r.removeFromTop (gap);
    r.removeFromTop (18);
    const int nC = snap.v2 ? 3 : 6;
    const int cw = (r.getWidth() - 8) / 3;
    for (int i = 0; i < nC; ++i)
        cvChips[i]->setBounds (r.getX() + (i % 3) * (cw + 4), r.getY() + (i / 3) * 32, cw, 29);
    r.removeFromTop (((nC + 2) / 3) * 32 + 2);
    auto row = r.removeFromTop (24);
    cvSave.setBounds (row.removeFromRight (52));
    row.removeFromRight (4);
    cvEdit.setBounds (row);
    cvErr.setBounds (r.removeFromTop (16));
}

void ChordLinkEditor::layoutChord (juce::Rectangle<int> panel)
{
    if (panel.isEmpty()) return;
    auto r = panel.withTrimmedTop (50).reduced (8, 2);
    if (snap.face)
    {
        emoSlider.setBounds (r.getX() + 4, r.getY() + 18, r.getWidth() - 8, 24);
        return;
    }
    auto row = [] (juce::Rectangle<int>& area, juce::OwnedArray<ChipButton>& chips, int count, int h) {
        area.removeFromTop (16);
        auto line = area.removeFromTop (h);
        const int w = (line.getWidth() - (count - 1) * 4) / count;
        for (int i = 0; i < chips.size(); ++i) chips[i]->setBounds (line.getX() + i * (w + 4), line.getY(), w, h);
        area.removeFromTop (6);
    };
    row (r, qChips, 6, 32);
    row (r, sChips, 6, 32);
    row (r, tChips, 7, 32);
    r.removeFromTop (16);
    const int cw = (r.getWidth() - 12) / 4;
    for (int i = 0; i < slotChips.size(); ++i)
        slotChips[i]->setBounds (r.getX() + (i % 4) * (cw + 4), r.getY() + (i / 4) * 30, cw, 27);
    resetChip.setBounds (panel.getRight() - 8 - 84, panel.getBottom() - 30, 84, 24);
}

void ChordLinkEditor::resized()
{
    auto top = getLocalBounds().removeFromTop (52).reduced (12, 10);
    panicBtn.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
#if CL_INSTRUMENT
    volSlider.setBounds (top.removeFromRight (90));
    top.removeFromRight (6);
    soundBox.setBounds (top.removeFromRight (110));
    top.removeFromRight (8);
#endif
    vlBtn.setBounds (top.removeFromRight (110));
    flatBtn.setBounds (top.removeFromRight (120));

    auto body = getLocalBounds().withTrimmedTop (60).reduced (18, 0);
    auto panels = body.removeFromTop (340);
    const int pw = (panels.getWidth() - 20) / 3;
    voicePanel = panels.removeFromLeft (pw);
    panels.removeFromLeft (10);
    notePanel = panels.removeFromLeft (pw);
    panels.removeFromLeft (10);
    chordPanel = panels;

    v2Btn.setBounds (voicePanel.getRight() - 100, voicePanel.getY() + 10, 94, 24);
    faceBtn.setBounds (chordPanel.getRight() - 130, chordPanel.getY() + 2, 124, 22);
    customBtn.setBounds (chordPanel.getRight() - 130, chordPanel.getY() + 23, 124, 22);

    body.removeFromTop (40);
    kbArea = body.removeFromTop (118);
    inKb.setBounds (kbArea);
    body.removeFromTop (22);
    outArea = body.removeFromTop (60);
    outKb.setBounds (outArea);
    layoutVoicing (voicePanel);
    layoutChord (chordPanel);
}

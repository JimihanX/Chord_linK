#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int EMOTION_CC = 20; // 얼굴인식 앱이 보내는 표정 값 (0=슬픔, 64=무표정, 127=기쁨)

} // namespace

juce::AudioProcessor::BusesProperties ChordLinkProcessor::makeBuses()
{
#if CL_INSTRUMENT
    return BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
#else
    return BusesProperties();
#endif
}

ChordLinkProcessor::ChordLinkProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
#if CL_INSTRUMENT
    for (int i = 0; i < 24; ++i) synth.addVoice (new cl::ClVoice (soundType));
    synth.addSound (new cl::ClSound());
#endif
}

juce::AudioProcessorValueTreeState::ParameterLayout ChordLinkProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "v2", 1 }, "Voicing 2", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "face", 1 }, "Face Mode", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "custom", 1 }, "Custom Mode", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "flat", 1 }, "Flat Names", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "vl", 1 }, "Voice Leading", true));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "emotion", 1 }, "Emotion", NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
#if CL_INSTRUMENT
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sound", 1 }, "Sound", StringArray { "Piano", "Sine", "Saw", "Square" }, 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", NormalisableRange<float> (0.0f, 1.0f), 0.8f));
#endif
    return l;
}

bool ChordLinkProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if CL_INSTRUMENT
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
#else
    juce::ignoreUnused (layouts);
    return true;
#endif
}

void ChordLinkProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    tmp.reserve (256);
}

void ChordLinkProcessor::pushCommand (const ClCommand& c)
{
    const juce::SpinLock::ScopedLockType sl (fifoWriteLock);
    auto w = fifo.write (1);
    if (w.blockSize1 > 0) cmdBuf[(size_t) w.startIndex1] = c;
    else if (w.blockSize2 > 0) cmdBuf[(size_t) w.startIndex2] = c;
}

void ChordLinkProcessor::setCustomVoicingText (int index, const juce::String& text)
{
    if (index < 0 || index >= cl::NUM_CV) return;
    {
        const juce::SpinLock::ScopedLockType sl (cvLock);
        pendingCV[(size_t) index] = text;
        pendingCVFlag[(size_t) index] = true;
    }
    pushCommand ({ ClCommand::SetCV, index, 0, 0 });
}

void ChordLinkProcessor::syncParams (std::vector<cl::MidiOut>& out)
{
    auto b = [this] (const char* id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };
    engine.setVoicing2 (b ("v2"), out);
    bool f = b ("face"), c = b ("custom");
    if (f && c) { if (engine.isFaceMode()) f = false; else c = false; } // 둘 다 켜지면 새로 켠 쪽 우선
    if (! f) engine.setFaceMode (false, out);
    if (! c) engine.setCustomMode (false, out);
    if (f) engine.setFaceMode (true, out);
    if (c) engine.setCustomMode (true, out);
    engine.setFlat (b ("flat"));
    if (engine.isVoiceLeading() != b ("vl")) engine.setVoiceLeading (b ("vl"));

    const float p = apvts.getRawParameterValue ("emotion")->load();
    const float cc = ccEmotion.exchange (-2.0f);
    if (cc > -1.5f) engine.setEmotion (cc, out);          // 얼굴인식 앱(CC20)
    else if (std::abs (p - lastEmotionParam) > 1.0e-4f) engine.setEmotion (p, out); // 화면 슬라이더·오토메이션
    lastEmotionParam = p;
#if CL_INSTRUMENT
    soundType.store ((int) apvts.getRawParameterValue ("sound")->load());
#endif
}

void ChordLinkProcessor::drainCommands (std::vector<cl::MidiOut>& out)
{
    auto handle = [&] (const ClCommand& c) {
        switch (c.type)
        {
            case ClCommand::NoteOn: engine.noteEvent (c.a, true, 100, out); break;
            case ClCommand::NoteOff: engine.noteEvent (c.a, false, 0, out); break;
            case ClCommand::HoldOn: engine.uiHoldOn (c.a, c.kind, c.b, out); break;
            case ClCommand::HoldOff: engine.uiHoldOff (c.a, out); break;
            case ClCommand::ToggleEdit: engine.uiToggleEdit (c.kind, c.b, out); break;
            case ClCommand::ResetEdit: engine.uiResetEdit (out); break;
            case ClCommand::VoiceKey: engine.selectVoiceKey (c.a, out); break;
            case ClCommand::Panic: engine.allNotesOff (out); break;
            case ClCommand::SetCV:
            {
                juce::String s;
                {
                    const juce::SpinLock::ScopedLockType sl (cvLock);
                    if (! pendingCVFlag[(size_t) c.a]) break;
                    s = pendingCV[(size_t) c.a];
                    pendingCVFlag[(size_t) c.a] = false;
                }
                engine.setCustomVoicingText (c.a, s.toStdString(), out);
                break;
            }
        }
    };
    auto r = fifo.read (fifo.getNumReady());
    for (int i = 0; i < r.blockSize1; ++i) handle (cmdBuf[(size_t) (r.startIndex1 + i)]);
    for (int i = 0; i < r.blockSize2; ++i) handle (cmdBuf[(size_t) (r.startIndex2 + i)]);
}

void ChordLinkProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    outMidi.clear();
    {
        const juce::SpinLock::ScopedLockType sl (engineLock);
        auto flush = [this] (int pos) {
            for (auto& m : tmp)
                outMidi.addEvent (m.on ? juce::MidiMessage::noteOn (1, m.note, (juce::uint8) juce::jlimit (1, 127, m.vel))
                                       : juce::MidiMessage::noteOff (1, m.note), pos);
            tmp.clear();
        };
        tmp.clear();
        syncParams (tmp);
        drainCommands (tmp);
        flush (0);

        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const int pos = meta.samplePosition;
            if (msg.isNoteOn()) engine.noteEvent (msg.getNoteNumber(), true, msg.getVelocity(), tmp);
            else if (msg.isNoteOff()) engine.noteEvent (msg.getNoteNumber(), false, 0, tmp);
            else if (msg.isSustainPedalOn()) engine.setSustain (true, tmp);
            else if (msg.isSustainPedalOff()) engine.setSustain (false, tmp);
            else if (msg.isController() && msg.getControllerNumber() == EMOTION_CC)
                engine.setEmotion ((float) msg.getControllerValue() / 63.5f - 1.0f, tmp);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff (tmp);
            else outMidi.addEvent (msg, pos); // 피치벤드, 모듈레이션 등은 그대로 통과
            flush (pos);
        }
    }
    midi.swapWith (outMidi);

#if CL_INSTRUMENT
    buffer.clear();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    buffer.applyGain (apvts.getRawParameterValue ("volume")->load());
#else
    buffer.clear();
#endif
}

ClSnapshot ChordLinkProcessor::snapshot()
{
    ClSnapshot s;
    const juce::SpinLock::ScopedLockType sl (engineLock);
    const bool flat = engine.isFlat();
    s.chordName = juce::String::fromUTF8 (engine.chordName().c_str());
    s.voicingName = juce::String::fromUTF8 (engine.voicingDisplayName().c_str());
    s.anyHeld = engine.anyRootHeld();
    s.outNotes = engine.previewNotes();
    juce::StringArray names;
    for (int n : s.outNotes) names.add (cl::noteName (n, flat));
    s.notesText = names.joinIntoString ("  ");
    s.v2 = engine.isVoicing2();
    s.face = engine.isFaceMode();
    s.custom = engine.isCustomMode();
    s.flat = flat;
    s.current = engine.currentFormula();
    s.effective = engine.effectiveFormula();
    s.faceLevel = engine.getFaceLevel();
    s.emotion = engine.getEmotion();
    s.voiceKeyIndex = engine.voicePresetIndexOrCustom();
    for (int i = 0; i < cl::NUM_SLOTS; ++i)
    {
        const auto& f = engine.getSlots()[(size_t) i];
        s.slotLabels[(size_t) i] = cl::labelOrMaj (f);
        s.slotActive[(size_t) i] = s.custom ? (f == s.current) : engine.slotIsHeld (i);
    }
    for (int i = 0; i < cl::NUM_CV; ++i) s.cv[(size_t) i] = juce::String::fromUTF8 (engine.getCustomVoicings()[(size_t) i].c_str());
    for (int n = 0; n < 128; ++n) s.down[(size_t) n] = engine.isNoteDown (n);
    s.flashSlot = engine.flashSlot;
    return s;
}

void ChordLinkProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree eng ("ENGINE");
    {
        const juce::SpinLock::ScopedLockType sl (engineLock);
        for (int i = 0; i < cl::NUM_SLOTS; ++i)
        {
            const auto& f = engine.getSlots()[(size_t) i];
            juce::ValueTree t ("SLOT");
            t.setProperty ("q", f.q, nullptr).setProperty ("s", f.s, nullptr).setProperty ("t", (int) f.t, nullptr);
            eng.addChild (t, -1, nullptr);
        }
        for (int i = 0; i < cl::NUM_CV; ++i)
        {
            juce::ValueTree t ("CV");
            t.setProperty ("text", juce::String::fromUTF8 (engine.getCustomVoicings()[(size_t) i].c_str()), nullptr);
            eng.addChild (t, -1, nullptr);
        }
        eng.setProperty ("voiceCustom", engine.voiceIsCustom(), nullptr);
        eng.setProperty ("voiceIndex", engine.voiceIsCustom() ? engine.voiceCustomIndex() : (int) engine.voicePreset(), nullptr);
    }
    state.removeChild (state.getChildWithName ("ENGINE"), nullptr);
    state.addChild (eng, -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ChordLinkProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;
    auto eng = state.getChildWithName ("ENGINE");
    apvts.replaceState (state);
    if (! eng.isValid()) return;

    auto slots = cl::DEFAULT_SLOTS;
    std::array<std::string, cl::NUM_CV> cv;
    for (int i = 0; i < cl::NUM_CV; ++i) cv[(size_t) i] = cl::DEFAULT_CV[(size_t) i];
    int si = 0, ci = 0;
    for (auto child : eng)
    {
        if (child.hasType ("SLOT") && si < cl::NUM_SLOTS)
        {
            auto& f = slots[(size_t) si++];
            f.q = juce::jlimit (0, (int) cl::NUM_Q - 1, (int) child.getProperty ("q"));
            f.s = juce::jlimit (0, 2, (int) child.getProperty ("s"));
            f.t = (uint8_t) juce::jlimit (0, 127, (int) child.getProperty ("t"));
        }
        else if (child.hasType ("CV") && ci < cl::NUM_CV)
            cv[(size_t) ci++] = child.getProperty ("text").toString().toStdString();
    }
    const juce::SpinLock::ScopedLockType sl (engineLock);
    engine.restore (slots, cv, (bool) eng.getProperty ("voiceCustom"), (int) eng.getProperty ("voiceIndex"));
}

juce::AudioProcessorEditor* ChordLinkProcessor::createEditor() { return new ChordLinkEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChordLinkProcessor(); }

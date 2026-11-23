// 플러그인 화면을 PNG로 저장하는 확인용 프로그램 (개발용)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void setParam (juce::AudioProcessor& p, const char* id, float v)
{
    for (auto* prm : p.getParameters())
        if (auto* w = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (w->paramID == id) w->setValueNotifyingHost (w->convertTo0to1 (v));
}
static void midi (juce::AudioProcessor& p, std::initializer_list<juce::MidiMessage> msgs)
{
    juce::MidiBuffer mb; int pos = 0;
    for (auto& m : msgs) mb.addEvent (m, pos++);
    juce::AudioBuffer<float> a (2, 256);
    p.processBlock (a, mb);
}
static void shot (juce::AudioProcessor& p, const juce::String& file)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (ed->getWidth(), ed->getHeight());
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (file); f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    printf ("saved %s\n", file.toRawUTF8());
}
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::String dir = argc > 1 ? argv[1] : ".";
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    p->prepareToPlay (48000, 256);
    using M = juce::MidiMessage;
    midi (*p, { M::noteOn (1, 42, (juce::uint8) 100), M::noteOn (1, 44, (juce::uint8) 100), M::noteOn (1, 24, (juce::uint8) 100) });
    shot (*p, dir + "/shot_main.png");
    midi (*p, { M::noteOff (1, 42), M::noteOff (1, 44), M::noteOff (1, 24) });
    setParam (*p, "v2", 1.0f); midi (*p, {});
    midi (*p, { M::noteOn (1, 65, (juce::uint8) 100), M::noteOff (1, 65), M::noteOn (1, 26, (juce::uint8) 100) });
    setParam (*p, "flat", 1.0f); midi (*p, {});
    shot (*p, dir + "/shot_v2.png");
    midi (*p, { M::noteOff (1, 26) });
    setParam (*p, "v2", 0.0f); setParam (*p, "flat", 0.0f); setParam (*p, "face", 1.0f); setParam (*p, "emotion", 0.72f); midi (*p, {});
    midi (*p, { M::noteOn (1, 24, (juce::uint8) 100) });
    shot (*p, dir + "/shot_face.png");
    midi (*p, { M::noteOff (1, 24) });
    setParam (*p, "face", 0.0f); setParam (*p, "custom", 1.0f); midi (*p, {});
    shot (*p, dir + "/shot_custom.png");
    return 0;
}

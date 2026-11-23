// 악기형(Chord Link)에 들어 있는 간단한 내장 신스: 피아노 / 사인파 / 쏘우파 / 스퀘어
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace cl
{
enum SoundType { SND_PIANO = 0, SND_SINE, SND_SAW, SND_SQUARE };

struct ClSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class ClVoice : public juce::SynthesiserVoice
{
public:
    explicit ClVoice (std::atomic<int>& soundRef) : sound (soundRef) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<ClSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        type = sound.load();
        freq = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        vel = velocity;
        sr = getSampleRate();
        for (auto& p : phase) p = 0.0;
        t = 0.0;
        release = false;
        relGain = 1.0;
        env = 0.0;
        // 피아노: 높은 음일수록 빨리 줄어듦
        tau0 = juce::jlimit (0.7, 7.0, 5.0 * std::sqrt (110.0 / freq));
        partials = juce::jlimit (3, 10, (int) (9000.0 / freq));
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) release = true;
        else { clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;
        const double dt = 1.0 / sr;
        const double relCoef = std::exp (-dt / (type == SND_PIANO ? 0.09 : 0.06));
        for (int i = 0; i < num; ++i)
        {
            double s = 0.0;
            if (type == SND_PIANO) s = piano();
            else s = basic();
            // 어택 (클릭 방지)
            env = std::min (1.0, env + dt / 0.004);
            if (release) relGain *= relCoef;
            const float v = (float) (s * env * relGain);
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample (ch, start + i, v);
            t += dt;
            if (release && relGain < 0.0005) { clearCurrentNote(); break; }
            if (type == SND_PIANO && t > tau0 * 6.0) { clearCurrentNote(); break; }
        }
    }

private:
    static double polyBlep (double ph, double inc)
    {
        if (ph < inc) { double x = ph / inc; return x + x - x * x - 1.0; }
        if (ph > 1.0 - inc) { double x = (ph - 1.0) / inc; return x * x + x + x + 1.0; }
        return 0.0;
    }

    double basic()
    {
        const double inc = freq / sr;
        double& ph = phase[0];
        double s = 0.0;
        if (type == SND_SINE) s = std::sin (juce::MathConstants<double>::twoPi * ph) * 0.16;
        else if (type == SND_SAW) s = ((2.0 * ph - 1.0) - polyBlep (ph, inc)) * 0.06;
        else
        {
            double sq = ph < 0.5 ? 1.0 : -1.0;
            sq += polyBlep (ph, inc);
            sq -= polyBlep (std::fmod (ph + 0.5, 1.0), inc);
            s = sq * 0.05;
        }
        ph += inc;
        if (ph >= 1.0) ph -= 1.0;
        // 살짝 줄어드는 서스테인
        const double sus = 0.7 + 0.3 * std::exp (-t / 0.25);
        return s * sus * (0.35 + 0.65 * vel);
    }

    double piano()
    {
        const double B = 0.00008 + 0.0004 * (freq / 1000.0);
        double s = 0.0;
        for (int k = 1; k <= partials; ++k)
        {
            const double fk = freq * k * std::sqrt (1.0 + B * k * k);
            if (fk > sr * 0.45) break;
            const double a = 1.0 / std::pow ((double) k, 1.25) * (k == 2 ? 1.15 : 1.0);
            const double tau = tau0 / (1.0 + 0.6 * (k - 1));
            double& ph = phase[(size_t) (k - 1)];
            s += a * std::exp (-t / tau) * std::sin (juce::MathConstants<double>::twoPi * ph);
            ph += fk / sr;
            if (ph >= 1.0) ph -= 1.0;
        }
        // 해머 느낌의 초기 감쇠
        const double body = 0.45 + 0.55 * std::exp (-t / 0.28);
        const double peak = 0.13 * vel * std::pow (std::min (1.0, 262.0 / freq), 0.15);
        return s * body * peak;
    }

    std::atomic<int>& sound;
    int type = SND_PIANO;
    double freq = 440.0, sr = 44100.0, t = 0.0, env = 0.0, relGain = 1.0, tau0 = 2.0;
    float vel = 0.8f;
    int partials = 8;
    bool release = false;
    std::array<double, 12> phase {};
};
} // namespace cl

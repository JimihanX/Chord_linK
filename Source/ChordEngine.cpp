#include "ChordEngine.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <numeric>
#include <set>

namespace cl
{
namespace
{
const int QB[NUM_Q][3] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 8 }, { 0, 3, 6 }, { 0, 5, 7 }, { 0, 2, 7 } };
const char* SYM[NUM_Q][3] = {
    { "", "7", "maj7" },          { "m", "m7", "m(maj7)" },       { "aug", "aug7", "aug(maj7)" },
    { "dim", "m7(b5)", "dim7" },  { "sus4", "7sus4", "maj7sus4" }, { "sus2", "7sus2", "maj7sus2" } };
const int TENS[NUM_T] = { 13, 14, 15, 17, 18, 21, 20 };
const char* TNAME[NUM_T] = { "b9", "9", "#9", "11", "#11", "13", "b13" };
const char* QNAME[NUM_Q] = { "maj", "min", "aug", "dim", "sus4", "sus2" };
const char* SHARP[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* FLAT[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

struct VInfo { const char* name; const char* shortName; bool guitar; };
const VInfo VINFO[NUM_V] = {
    { "Close Voicing", "Cl", false },  { "Open Voicing", "Op", false },   { "Drop Voicing", "Dr", false },
    { "Shell Voicing", "Sh", false },  { "Quartal Voicing", "Qu", false }, { "Cluster Voicing", "Cs", false },
    { "Drop 2 Voicing", "D2", true },  { "Drop 3 Voicing", "D3", true },  { "Spread Voicing", "Sp", true },
    { "Hybrid Voicing", "Hy", true } };

std::vector<int> uniqSort (std::vector<int> a)
{
    std::sort (a.begin(), a.end());
    a.erase (std::unique (a.begin(), a.end()), a.end());
    return a;
}
int mod12 (int v) { return ((v % 12) + 12) % 12; }
std::vector<int> foldPcs (const std::vector<int>& iv)
{
    std::vector<int> r;
    for (int v : iv) r.push_back (mod12 (v));
    return uniqSort (r);
}
bool contains (const std::vector<int>& a, int v) { return std::find (a.begin(), a.end(), v) != a.end(); }

std::vector<int> vClose (const Tones& T) { return foldPcs (T.iv); }
std::vector<int> vOpen (const Tones& T)
{
    auto c = vClose (T);
    if (c.size() >= 3) c[1] += 12;
    return uniqSort (c);
}
std::vector<int> vDrop (const Tones& T, int n)
{
    auto c = vClose (T);
    if (c.size() < 3) return c;
    int i = (int) c.size() - n;
    if (i < 1) i = (int) c.size() - 2;
    c[(size_t) i] -= 12;
    return uniqSort (c);
}
std::vector<int> vShell (const Tones& T)
{
    int top = T.sev >= 0 ? T.sev : (T.fifth >= 0 ? T.fifth : QB[T.q][2]);
    return uniqSort ({ 0, T.third, top });
}
std::vector<int> vQuartal (const Tones& T)
{
    int st = T.sev >= 0 ? T.sev : ((T.q == MAJ || T.q == AUG) ? T.third : QB[T.q][2]);
    return uniqSort ({ 0, st, st + 5, st + 10 });
}
std::vector<int> vCluster (const Tones& T)
{
    auto p = foldPcs (T.iv);
    std::vector<int> best;
    int bestSpan = 1 << 20;
    for (size_t i = 0; i < p.size(); ++i)
    {
        std::vector<int> r (p.begin() + (long) i, p.end());
        for (size_t j = 0; j < i; ++j) r.push_back (p[j] + 12);
        int sp = r.back() - r.front();
        if (sp < bestSpan) { bestSpan = sp; best = r; }
    }
    return best;
}
std::vector<int> vSpread (const Tones& T)
{
    std::vector<int> order { 0 };
    if (T.fifth >= 0) order.push_back (mod12 (T.fifth));
    order.push_back (mod12 (T.third));
    if (T.sev >= 0) order.push_back (mod12 (T.sev));
    for (int t : T.ts) order.push_back (mod12 (t));
    auto folded = foldPcs (T.iv);
    std::vector<int> pcs;
    for (int p : order)
        if (! contains (pcs, p) && contains (folded, p)) pcs.push_back (p);
    const int off[] = { 0, 0, 12, 12, 24, 24, 36, 36 };
    std::vector<int> out;
    int prev = -99;
    for (size_t i = 0; i < pcs.size(); ++i)
    {
        int v = pcs[i] + off[std::min<size_t> (i, 7)];
        while (v <= prev) v += 12;
        out.push_back (v);
        prev = v;
    }
    return out;
}
std::vector<int> vHybrid (const Tones& T)
{
    std::vector<int> r { 0 };
    if (T.fifth >= 0) r.push_back (mod12 (T.fifth));
    for (int p : foldPcs (T.iv))
        if (p != 0 && ! (T.fifth >= 0 && p == mod12 (T.fifth))) r.push_back (p + 12);
    return uniqSort (r);
}

std::string upper (std::string s)
{
    for (auto& c : s) c = (char) std::toupper ((unsigned char) c);
    return s;
}
const std::vector<std::string> DEG = { "R", "1", "3", "5", "7", "6", "9", "11", "13", "b9", "#9", "#11", "b13", "b5", "#5" };

int degSemi (const std::string& t, const Tones& T)
{
    if (t == "R" || t == "1") return 0;
    if (t == "3") return T.third;
    if (t == "5") return QB[T.q][2];
    if (t == "7") return T.sev >= 0 ? T.sev : (T.q == MAJ ? 11 : 10);
    if (t == "6") return 9;
    if (t == "9") return contains (T.ts, 13) ? 13 : contains (T.ts, 15) ? 15 : 14;
    if (t == "11") return contains (T.ts, 18) ? 18 : 17;
    if (t == "13") return contains (T.ts, 20) ? 20 : 21;
    for (int i = 0; i < NUM_T; ++i)
        if (t == TNAME[i]) return TENS[i];
    return t == "b5" ? 6 : 8;
}
} // namespace

// 코드 키 스위치 C2~C3 (13개). b13 은 화면 전용
const std::array<ChordKeyAct, 13> CHORD_KEYS = { { { 'q', MIN }, { 'q', AUG }, { 'q', DIM }, { 'q', SUS4 }, { 'q', SUS2 },
                                                    { 's', S_7 }, { 's', S_M7 },
                                                    { 't', T_b9 }, { 't', T_9 }, { 't', T_s9 }, { 't', T_11 }, { 't', T_s11 }, { 't', T_13 } } };

static Formula F (int q, int s, std::initializer_list<int> ts)
{
    Formula f; f.q = q; f.s = s;
    for (int t : ts) f.t = (uint8_t) (f.t | (1u << t));
    return f;
}
const std::array<Formula, NUM_FACE> FACE_LADDER = { { F (DIM, S_M7, {}), F (DIM, S_7, {}), F (MIN, S_7, { T_b9 }), F (MIN, S_7, {}), F (MIN, S_NONE, {}),
                                                     F (SUS2, S_NONE, {}), F (MAJ, S_NONE, {}), F (MAJ, S_M7, {}), F (MAJ, S_M7, { T_9 }), F (MAJ, S_M7, { T_9, T_13 }) } };
const std::array<Formula, NUM_SLOTS> DEFAULT_SLOTS = { { F (MAJ, S_M7, {}), F (MIN, S_7, {}), F (MAJ, S_7, {}), F (DIM, S_7, {}),
                                                         F (DIM, S_M7, {}), F (SUS4, S_7, {}), F (MAJ, S_NONE, { T_9 }), F (MAJ, S_M7, { T_9 }),
                                                         F (MIN, S_7, { T_9 }), F (MAJ, S_7, { T_b9 }), F (MAJ, S_7, { T_s9 }) } };
const std::array<const char*, NUM_CV> DEFAULT_CV = { { "R-5-9-3", "R-3-7-9", "R-7-3-5", "R-5-7-3", "3-7-9-5", "7-3-5-13" } };

std::string label (const Formula& f)
{
    std::string s = SYM[f.q][f.s];
    std::string ts;
    for (int i = 0; i < NUM_T; ++i)
        if (f.has (i)) ts += (ts.empty() ? "" : ",") + std::string (TNAME[i]);
    if (! ts.empty()) s += f.s != S_NONE ? "(" + ts + ")" : "(add" + ts + ")";
    return s;
}
std::string labelOrMaj (const Formula& f) { auto s = label (f); return s.empty() ? "maj" : s; }

Tones tones (const Formula& f)
{
    Tones T;
    T.q = f.q;
    const int* base = QB[f.q];
    T.third = base[1];
    const int fifth = base[2];
    std::vector<int> vals;
    std::set<int> pcs;
    auto add = [&] (int v) { if (pcs.insert (mod12 (v)).second) vals.push_back (v); };
    for (int i = 0; i < 3; ++i) add (base[i]);
    if (f.s == S_7) T.sev = 10;
    else if (f.s == S_M7) T.sev = f.q == DIM ? 9 : 11;
    if (T.sev >= 0) add (T.sev);
    for (int i = 0; i < NUM_T; ++i)
        if (f.has (i)) { T.ts.push_back (TENS[i]); add (TENS[i]); }
    std::sort (vals.begin(), vals.end());
    if (vals.size() >= 5 && f.q != AUG && f.q != DIM && f.q != SUS4 && f.q != SUS2)
        vals.erase (std::remove (vals.begin(), vals.end(), fifth), vals.end());
    T.iv = vals;
    T.fifth = contains (vals, fifth) ? fifth : -1;
    return T;
}

std::vector<int> voicingRel (VoicingId id, const Tones& T)
{
    switch (id)
    {
        case V_CLOSE: return vClose (T);
        case V_OPEN: return vOpen (T);
        case V_DROP: case V_G_DROP2: return vDrop (T, 2);
        case V_SHELL: return vShell (T);
        case V_QUARTAL: return vQuartal (T);
        case V_CLUSTER: return vCluster (T);
        case V_G_DROP3: return vDrop (T, 3);
        case V_G_SPREAD: return vSpread (T);
        case V_G_HYBRID: return vHybrid (T);
        default: return vClose (T);
    }
}

bool parseCustomVoicing (const std::string& s, std::vector<std::string>& tk, std::string& err)
{
    tk.clear();
    std::string cur;
    auto flush = [&] {
        if (! cur.empty())
        {
            auto u = upper (cur);
            for (auto& c : u) if (c == 'B') { c = 'b'; break; } // JS: replace 첫 'B' → 'b'
            tk.push_back (u);
            cur.clear();
        }
    };
    for (char c : s)
    {
        if (std::isspace ((unsigned char) c)) continue;
        if (c == '-' || c == ',') flush();
        else cur += c;
    }
    flush();
    if (tk.empty()) { err = "도수를 입력하세요 (예: R-5-9-3)"; return false; }
    for (auto& t : tk)
        if (std::find (DEG.begin(), DEG.end(), t) == DEG.end())
        { err = "'" + t + "' 는 쓸 수 없는 도수입니다. 사용 가능: R 3 5 7 6 9 11 13 b9 #9 #11 b13"; return false; }
    if (tk.size() > 7) { err = "최대 7음까지 가능합니다"; return false; }
    return true;
}

std::vector<int> customVoicingRel (const Tones& T, const std::string& s)
{
    std::vector<std::string> tk;
    std::string err;
    if (! parseCustomVoicing (s, tk, err)) return vClose (T);
    std::vector<int> out;
    int prev = -1;
    for (auto& t : tk)
    {
        int v = mod12 (degSemi (t, T));
        while (v <= prev) v += 12;
        out.push_back (v);
        prev = v;
    }
    return out;
}

int faceLevel (float e) { return std::max (0, std::min (9, (int) std::floor ((e + 1.0f) / 2.0f * 10.0f))); }
std::string pcName (int pc, bool flat) { return (flat ? FLAT : SHARP)[mod12 (pc)]; }
std::string noteName (int m, bool flat) { return pcName (m, flat) + std::to_string ((int) std::floor (m / 12.0) - 1); }
const char* voicingName (VoicingId id) { return VINFO[id].name; }
const char* voicingShort (VoicingId id) { return VINFO[id].shortName; }
bool voicingIsGuitar (VoicingId id) { return VINFO[id].guitar; }
const char* tensionName (int t) { return TNAME[t]; }
const char* qualityName (int q) { return QNAME[q]; }
const std::vector<VoicingId>& presetList (bool v2)
{
    static const std::vector<VoicingId> p1 { V_CLOSE, V_OPEN, V_DROP, V_SHELL, V_QUARTAL, V_CLUSTER };
    static const std::vector<VoicingId> p2 { V_CLOSE, V_OPEN, V_DROP, V_SHELL, V_QUARTAL, V_G_DROP2, V_G_DROP3, V_G_SPREAD, V_G_HYBRID };
    return v2 ? p2 : p1;
}

// ================================================================ Engine
Engine::Engine()
{
    slots = DEFAULT_SLOTS;
    for (int i = 0; i < NUM_CV; ++i) cv[(size_t) i] = DEFAULT_CV[(size_t) i];
    lvl = faceLevel (0.0f);
}

void Engine::restore (const std::array<Formula, NUM_SLOTS>& s, const std::array<std::string, NUM_CV>& c, bool vc, int idx)
{
    slots = s;
    cv = c;
    voiceCustom = vc;
    if (vc) voiceCustomIdx = std::max (0, std::min (NUM_CV - 1, idx));
    else voiceId = (VoicingId) std::max (0, std::min ((int) NUM_V - 1, idx));
}

Formula Engine::heldFormula (Formula f) const
{
    for (auto& h : hk)
        if (h.slot >= 0) f = slots[(size_t) h.slot];
    for (auto& h : hk)
    {
        if (h.slot >= 0) continue;
        if (h.kind == 'q') f.q = h.value;
        else if (h.kind == 's') f.s = h.value;
        else if (h.kind == 't') f.t = (uint8_t) (f.t | (1u << h.value));
    }
    return f;
}
Formula Engine::currentFormula() const { return heldFormula (custom ? editF : Formula {}); }
Formula Engine::effectiveFormula() const { return face ? FACE_LADDER[(size_t) lvl] : currentFormula(); }
bool Engine::slotIsHeld (int i) const
{
    for (auto& h : hk) if (h.slot == i) return true;
    return false;
}

bool Engine::currentIsGuitar() const { return ! voiceCustom && voicingIsGuitar (voiceId); }
std::vector<int> Engine::upperRelFor (const Tones& T) const
{
    if (voiceCustom) return customVoicingRel (T, cv[(size_t) voiceCustomIdx]);
    return voicingRel (voiceId, T);
}
int Engine::voicePresetIndexOrCustom() const
{
    auto& list = presetList (v2);
    if (voiceCustom) return (int) list.size() + voiceCustomIdx;
    auto it = std::find (list.begin(), list.end(), voiceId);
    return it == list.end() ? 0 : (int) (it - list.begin());
}
std::string Engine::voicingDisplayName() const
{
    if (voiceCustom) return "Custom Voicing " + std::to_string (voiceCustomIdx + 1) + " - " + cv[(size_t) voiceCustomIdx];
    return std::string (voicingName (voiceId)) + (voicingIsGuitar (voiceId) ? " (Guitar)" : "");
}
std::string Engine::chordName() const { return pcName (lastRoot, flat) + label (effectiveFormula()); }

Engine::ChordResult Engine::buildChord (int pc, const Formula& f, bool useVL, const std::vector<int>& prev) const
{
    const Tones T = tones (f);
    const auto rel = upperRelFor (T);
    const int lo = currentIsGuitar() ? 40 : 48;
    std::vector<int> up;
    for (int v : rel) up.push_back (36 + pc + v);
    auto mn = [] (const std::vector<int>& a) { return *std::min_element (a.begin(), a.end()); };
    auto mx = [] (const std::vector<int>& a) { return *std::max_element (a.begin(), a.end()); };
    while (mn (up) < lo) for (auto& n : up) n += 12;
    while (mn (up) >= lo + 12) for (auto& n : up) n -= 12;
    if (useVL && ! prev.empty())
    {
        auto avg = [] (const std::vector<int>& a) { return std::accumulate (a.begin(), a.end(), 0.0) / (double) a.size(); };
        const double pa = avg (prev);
        auto best = up;
        double bd = std::abs (avg (up) - pa);
        for (int sh : { -12, 12 })
        {
            auto c = up;
            for (auto& n : c) n += sh;
            if (mn (c) >= 38 && mx (c) <= 96)
            {
                double d = std::abs (avg (c) - pa);
                if (d + 0.01 < bd) { bd = d; best = c; }
            }
        }
        up = best;
    }
    ChordResult r;
    r.up = up;
    r.notes = uniqSort (up);
    return r;
}

std::vector<int> Engine::previewNotes() const
{
    if (! held.empty()) return lastNotes;
    return buildChord (lastRoot, effectiveFormula(), false, {}).notes;
}

// ---------------------------------------------------------------- 출력 노트 관리
void Engine::noteOnOut (int n, int vel, std::vector<MidiOut>& out)
{
    if (n < 0 || n > 127) return;
    if (outCount[(size_t) n]++ == 0) out.push_back ({ true, n, vel });
}
void Engine::noteOffOut (int n, std::vector<MidiOut>& out)
{
    if (n < 0 || n > 127 || outCount[(size_t) n] == 0) return;
    if (--outCount[(size_t) n] == 0) out.push_back ({ false, n, 0 });
}
void Engine::endEntry (const Entry& e, std::vector<MidiOut>& out)
{
    for (int n : e.notes) noteOffOut (n, out);
}

void Engine::startRoot (int key, int vel, std::vector<MidiOut>& out)
{
    const int pc = key % 12;
    auto r = buildChord (pc, effectiveFormula(), voiceLeading, prevUpper);
    Entry e { pc, vel, r.notes };
    for (int n : e.notes) noteOnOut (n, vel, out);
    held[key] = e;
    prevUpper = r.up;
    lastRoot = pc;
    lastNotes = r.notes;
}
void Engine::releaseRoot (int key, std::vector<MidiOut>& out)
{
    auto it = held.find (key);
    if (it == held.end()) return;
    Entry e = it->second;
    held.erase (it);
    if (sustain) sustained.push_back (e);
    else endEntry (e, out);
}

// 상태가 바뀌면: 누르고 있는 근음마다 새 코드와 비교해서 달라진 음만 교체
void Engine::changed (std::vector<MidiOut>& out)
{
    const Formula f = effectiveFormula();
    for (auto& kv : held)
    {
        auto& e = kv.second;
        auto r = buildChord (e.pc, f, voiceLeading, prevUpper);
        for (int n : e.notes)
            if (! contains (r.notes, n)) noteOffOut (n, out);
        for (int n : r.notes)
            if (! contains (e.notes, n)) noteOnOut (n, e.vel, out);
        e.notes = r.notes;
        prevUpper = r.up;
        lastRoot = e.pc;
        lastNotes = r.notes;
    }
}

void Engine::holdOn (const Held& h, std::vector<MidiOut>& out)
{
    for (auto& x : hk) if (x.id == h.id) return;
    hk.push_back (h);
    changed (out);
}
void Engine::holdOff (int id, std::vector<MidiOut>& out)
{
    const auto n = hk.size();
    hk.erase (std::remove_if (hk.begin(), hk.end(), [id] (const Held& h) { return h.id == id; }), hk.end());
    if (hk.size() != n) changed (out);
}

void Engine::storeSlot (int i, std::vector<MidiOut>& out)
{
    slots[(size_t) i] = currentFormula();
    flashSlot = i;
    changed (out);
}

void Engine::noteEvent (int m, bool on, int vel, std::vector<MidiOut>& out)
{
    if (m < 0 || m > 127) return;
    down[(size_t) m] = on;
    if (m >= NOTE_LO && m <= NOTE_HI)
    {
        if (on) { if (held.find (m) == held.end()) startRoot (m, vel, out); }
        else releaseRoot (m, out);
        return;
    }
    if (m >= CHORD_LO && m <= SLOT_HI)
    {
        if (face) return; // 얼굴인식 모드에서는 코드 키 꺼짐
        if (custom)
        {   // 지정 모드: 코드 키는 고정 선택, 슬롯 키는 저장
            if (! on) return;
            if (m <= CHORD_HI) { auto a = CHORD_KEYS[(size_t) (m - CHORD_LO)]; uiToggleEdit (a.kind, a.value, out); }
            else storeSlot (m - SLOT_LO, out);
            return;
        }
        if (on)
        {
            Held h { m, -1, -1, 0, 0 };
            if (m <= CHORD_HI) { auto a = CHORD_KEYS[(size_t) (m - CHORD_LO)]; h.kind = a.kind; h.value = a.value; }
            else h.slot = m - SLOT_LO;
            holdOn (h, out);
        }
        else holdOff (m, out);
        return;
    }
    if (m >= VOICE_LO && m <= VOICE_HI)
    {
        if (on) selectVoiceKey (m - VOICE_LO, out);
        return;
    }
    // 구역 밖: 그대로 통과
    if (on)
    {
        auto it = plain.find (m);
        if (it != plain.end()) { endEntry (it->second, out); plain.erase (it); }
        plainSustained.erase (std::remove (plainSustained.begin(), plainSustained.end(), m), plainSustained.end());
        Entry e { m % 12, vel, { m } };
        noteOnOut (m, vel, out);
        plain[m] = e;
    }
    else
    {
        auto it = plain.find (m);
        if (it == plain.end()) return;
        if (sustain) plainSustained.push_back (m);
        else { endEntry (it->second, out); plain.erase (it); }
    }
}

void Engine::setSustain (bool on, std::vector<MidiOut>& out)
{
    sustain = on;
    if (on) return;
    for (auto& e : sustained) endEntry (e, out);
    sustained.clear();
    for (int m : plainSustained)
    {
        auto it = plain.find (m);
        if (it != plain.end()) { endEntry (it->second, out); plain.erase (it); }
    }
    plainSustained.clear();
}

void Engine::allNotesOff (std::vector<MidiOut>& out)
{
    held.clear(); sustained.clear(); plain.clear(); plainSustained.clear(); hk.clear();
    for (int n = 0; n < 128; ++n)
        if (outCount[(size_t) n] > 0) { outCount[(size_t) n] = 0; out.push_back ({ false, n, 0 }); }
    down.fill (false);
}

void Engine::uiHoldOn (int id, char kind, int value, std::vector<MidiOut>& out)
{
    if (face || custom) return;
    Held h { id, -1, -1, kind, value };
    if (kind == 'l') { h.slot = value; h.kind = 0; }
    holdOn (h, out);
}
void Engine::uiHoldOff (int id, std::vector<MidiOut>& out) { holdOff (id, out); }
void Engine::uiToggleEdit (char kind, int value, std::vector<MidiOut>& out)
{
    if (kind == 'q') editF.q = (editF.q == value && value != MAJ) ? MAJ : value;
    else if (kind == 's') editF.s = editF.s == value ? S_NONE : value;
    else if (kind == 't') editF.t = (uint8_t) (editF.t ^ (1u << value));
    else if (kind == 'l') { storeSlot (value, out); return; }
    changed (out);
}
void Engine::uiResetEdit (std::vector<MidiOut>& out) { editF = Formula {}; changed (out); }

void Engine::setVoicing2 (bool b, std::vector<MidiOut>& out)
{
    if (b == v2) return;
    v2 = b;
    auto& list = presetList (v2);
    if (! voiceCustom && std::find (list.begin(), list.end(), voiceId) == list.end()) voiceId = V_CLOSE;
    if (voiceCustom && voiceCustomIdx >= (v2 ? 3 : 6)) { voiceCustom = false; voiceId = V_CLOSE; }
    changed (out);
}
void Engine::setFaceMode (bool b, std::vector<MidiOut>& out)
{
    if (b == face) return;
    face = b;
    if (face) { custom = false; hk.clear(); }
    changed (out);
}
void Engine::setCustomMode (bool b, std::vector<MidiOut>& out)
{
    if (b == custom) return;
    hk.clear();
    custom = b;
    if (custom) face = false;
    changed (out);
}
void Engine::setEmotion (float e, std::vector<MidiOut>& out)
{
    emo = std::max (-1.0f, std::min (1.0f, e));
    const int l = faceLevel (emo);
    if (l != lvl) { lvl = l; if (face) changed (out); }
}
void Engine::selectVoiceKey (int i, std::vector<MidiOut>& out)
{
    auto& list = presetList (v2);
    if (i < 0) return;
    if (i < (int) list.size()) { voiceCustom = false; voiceId = list[(size_t) i]; }
    else
    {
        const int ci = i - (int) list.size();
        if (ci >= (v2 ? 3 : 6)) return;
        voiceCustom = true; voiceCustomIdx = ci;
    }
    changed (out);
}
void Engine::setCustomVoicingText (int i, const std::string& s, std::vector<MidiOut>& out)
{
    if (i < 0 || i >= NUM_CV) return;
    cv[(size_t) i] = s;
    voiceCustom = true;
    voiceCustomIdx = i;
    changed (out);
}
} // namespace cl

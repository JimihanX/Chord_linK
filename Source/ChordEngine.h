// Chord Link - 코드 엔진 (JUCE에 의존하지 않는 순수 C++)
// 웹 프로토타입 v1.05 의 엔진을 그대로 옮긴 것.
#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cl
{
// ---------------------------------------------------------------- 기본 정의
enum Quality { MAJ = 0, MIN, AUG, DIM, SUS4, SUS2, NUM_Q };
enum Seventh { S_NONE = 0, S_7, S_M7 };
enum Tension { T_b9 = 0, T_9, T_s9, T_11, T_s11, T_13, T_b13, NUM_T }; // 비트 순서 = 표기 순서

struct Formula
{
    int q = MAJ;
    int s = S_NONE;
    uint8_t t = 0; // Tension 비트 마스크
    bool operator== (const Formula& o) const { return q == o.q && s == o.s && t == o.t; }
    bool operator!= (const Formula& o) const { return ! (*this == o); }
    bool has (int ten) const { return (t >> ten) & 1; }
};

enum VoicingId { V_CLOSE = 0, V_OPEN, V_DROP, V_SHELL, V_QUARTAL, V_CLUSTER,
                 V_G_DROP2, V_G_DROP3, V_G_SPREAD, V_G_HYBRID, NUM_V };

struct Tones
{
    std::vector<int> iv;   // 근음 기준 반음 간격 (오름차순)
    int third = 4;
    int fifth = -1;        // 생략되면 -1
    int sev = -1;          // 없으면 -1
    std::vector<int> ts;   // 텐션 반음
    int q = MAJ;
};

// 건반 구역 (C4 = 60)
constexpr int NOTE_LO = 24, NOTE_HI = 35;     // C1~B1 근음
constexpr int CHORD_LO = 36, CHORD_HI = 48;   // C2~C3 코드 키 스위치
constexpr int SLOT_LO = 49, SLOT_HI = 59;     // C#3~B3 커스텀 코드
constexpr int VOICE_LO = 60, VOICE_HI = 71;   // C4~B4 보이싱
constexpr int NUM_SLOTS = 11, NUM_CV = 6, NUM_FACE = 10;

struct ChordKeyAct { char kind; int value; }; // kind: 'q','s','t'
extern const std::array<ChordKeyAct, 13> CHORD_KEYS;
extern const std::array<Formula, NUM_FACE> FACE_LADDER;
extern const std::array<Formula, NUM_SLOTS> DEFAULT_SLOTS;
extern const std::array<const char*, NUM_CV> DEFAULT_CV;

// ---------------------------------------------------------------- 순수 함수
std::string label (const Formula& f);          // 예: "maj7(9)", 메이저 트라이어드는 ""
std::string labelOrMaj (const Formula& f);
Tones tones (const Formula& f);
std::vector<int> voicingRel (VoicingId id, const Tones& T);
bool parseCustomVoicing (const std::string& s, std::vector<std::string>& tokens, std::string& err);
std::vector<int> customVoicingRel (const Tones& T, const std::string& s);
int faceLevel (float emo);                      // -1(슬픔)~+1(기쁨) → 0~9
std::string noteName (int midi, bool flat);     // 예: "C#4" / "Db4"
std::string pcName (int pc, bool flat);
const char* voicingName (VoicingId id);
const char* voicingShort (VoicingId id);
bool voicingIsGuitar (VoicingId id);
const std::vector<VoicingId>& presetList (bool v2);
const char* tensionName (int t);
const char* qualityName (int q);

// ---------------------------------------------------------------- 상태를 가진 엔진
struct MidiOut
{
    bool on;
    int note;
    int vel;
};

class Engine
{
public:
    Engine();

    // 입력 (오디오 스레드에서 호출). 결과 MIDI는 out 에 추가된다.
    void noteEvent (int note, bool on, int vel, std::vector<MidiOut>& out);
    void setSustain (bool on, std::vector<MidiOut>& out);
    void allNotesOff (std::vector<MidiOut>& out);

    // 화면 버튼 (누르고 있는 동안만 적용). id 는 화면 버튼마다 고유값
    void uiHoldOn (int id, char kind, int value, std::vector<MidiOut>& out);   // kind 'q','s','t','slot'(='l')
    void uiHoldOff (int id, std::vector<MidiOut>& out);
    void uiToggleEdit (char kind, int value, std::vector<MidiOut>& out);       // 커스텀 지정 모드에서 고정 선택
    void uiResetEdit (std::vector<MidiOut>& out);

    // 스위치·설정 (바뀌면 울리는 코드를 다시 계산)
    void setVoicing2 (bool b, std::vector<MidiOut>& out);
    void setFaceMode (bool b, std::vector<MidiOut>& out);
    void setCustomMode (bool b, std::vector<MidiOut>& out);
    void setEmotion (float e, std::vector<MidiOut>& out);
    void setVoiceLeading (bool b) { voiceLeading = b; prevUpper.clear(); }
    void setFlat (bool b) { flat = b; }
    void selectVoiceKey (int index, std::vector<MidiOut>& out);               // 0~11 (C4~B4)
    void setCustomVoicingText (int i, const std::string& s, std::vector<MidiOut>& out);
    void setSlot (int i, const Formula& f) { if (i >= 0 && i < NUM_SLOTS) slots[(size_t) i] = f; }

    // 조회
    bool isVoicing2() const { return v2; }
    bool isFaceMode() const { return face; }
    bool isCustomMode() const { return custom; }
    bool isFlat() const { return flat; }
    bool isVoiceLeading() const { return voiceLeading; }
    float getEmotion() const { return emo; }
    int getFaceLevel() const { return lvl; }
    Formula currentFormula() const;   // 화면에 보이는 조합 (얼굴인식 제외)
    Formula effectiveFormula() const; // 실제로 쓰이는 조합
    bool voiceIsCustom() const { return voiceCustom; }
    int voicePresetIndexOrCustom() const; // 0~11 키 인덱스
    VoicingId voicePreset() const { return voiceId; }
    int voiceCustomIndex() const { return voiceCustomIdx; }
    std::string voicingDisplayName() const;
    const std::array<Formula, NUM_SLOTS>& getSlots() const { return slots; }
    const std::array<std::string, NUM_CV>& getCustomVoicings() const { return cv; }
    bool slotIsHeld (int i) const;
    int lastRootPc() const { return lastRoot; }
    bool anyRootHeld() const { return ! held.empty(); }
    const std::vector<int>& lastChordNotes() const { return lastNotes; }
    std::string chordName() const;  // 예: "Cmaj7(9)"
    std::vector<int> previewNotes() const;
    bool isNoteDown (int note) const { return note >= 0 && note < 128 && down[(size_t) note]; }
    bool isSustain() const { return sustain; }
    int flashSlot = -1; // 마지막으로 저장한 슬롯 (화면 표시용)

    // 테스트·저장용: 상태 직접 설정
    void restore (const std::array<Formula, NUM_SLOTS>& s, const std::array<std::string, NUM_CV>& c,
                  bool voiceCustomSel, int voicePresetOrCustomIdx);

    // 순수 계산 (테스트용 공개)
    struct ChordResult { std::vector<int> notes, up; };
    ChordResult buildChord (int pc, const Formula& f, bool useVL, const std::vector<int>& prev) const;

private:
    struct Held { int id; int slot; int act; char kind; int value; };
    struct Entry { int pc; int vel; std::vector<int> notes; };

    Formula heldFormula (Formula base) const;
    void changed (std::vector<MidiOut>& out);
    void startRoot (int key, int vel, std::vector<MidiOut>& out);
    void releaseRoot (int key, std::vector<MidiOut>& out);
    void endEntry (const Entry& e, std::vector<MidiOut>& out);
    void noteOnOut (int n, int vel, std::vector<MidiOut>& out);
    void noteOffOut (int n, std::vector<MidiOut>& out);
    void holdOn (const Held& h, std::vector<MidiOut>& out);
    void holdOff (int id, std::vector<MidiOut>& out);
    void storeSlot (int i, std::vector<MidiOut>& out);
    std::vector<int> upperRelFor (const Tones& T) const;
    bool currentIsGuitar() const;

    bool v2 = false, face = false, custom = false, flat = false, voiceLeading = true, sustain = false;
    float emo = 0.0f;
    int lvl = 5;
    Formula editF;
    std::array<Formula, NUM_SLOTS> slots;
    std::array<std::string, NUM_CV> cv;
    bool voiceCustom = false;
    VoicingId voiceId = V_CLOSE;
    int voiceCustomIdx = 0;

    std::vector<Held> hk;                 // 누르고 있는 코드 키·슬롯 (누른 순서)
    std::map<int, Entry> held;            // 누르고 있는 근음 → 울리는 노트
    std::vector<Entry> sustained;         // 페달로 유지 중
    std::map<int, Entry> plain;           // 구역 밖 일반 노트
    std::vector<int> plainSustained;
    std::array<int, 128> outCount {};     // 출력 노트 겹침 횟수
    std::array<bool, 128> down {};
    std::vector<int> prevUpper;
    int lastRoot = 0;
    std::vector<int> lastNotes;
};
} // namespace cl

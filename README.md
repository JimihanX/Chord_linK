# Chord Link 플러그인 v1.0

웹 프로토타입 v1.05의 코드 엔진을 C++(JUCE)로 옮긴 플러그인입니다.
한 번 빌드하면 **AU · VST3 · CLAP · Standalone(단독 앱)** 이 함께 만들어집니다.

## 두 가지 플러그인

| 이름 | 종류 | 쓰는 곳 |
| --- | --- | --- |
| **Chord Link** | 악기형. 내장 소리(피아노·사인파·쏘우파·스퀘어) + MIDI 출력 | Ableton Live 12, Logic 악기 슬롯 |
| **Chord Link MIDI FX** | MIDI 이펙트. 소리 없이 코드 MIDI만 출력 | Logic 의 MIDI FX 슬롯 |

건반 구역과 동작은 웹 버전 v1.05와 같습니다.

- 근음 C1~B1 (가운데 도 = C4)
- 코드 키 스위치 C2~C3, 커스텀 코드 C#3~B3: **누르고 있는 동안만** 적용
- 보이싱 C4~B4: 한 번 누르면 선택 유지
- 구역 밖 건반은 그대로 통과
- 서스테인 페달(CC64) 지원
- 얼굴인식 모드: 화면 슬라이더 또는 **MIDI CC20** (0 = 슬픔, 64 = 무표정, 127 = 기쁨)
- 커스텀 코드·커스텀 보이싱·스위치 상태는 DAW 프로젝트에 함께 저장됨

---

## 방법 A. GitHub 자동 빌드 (개발 도구 설치 없이)

1. GitHub에 새 저장소를 만들고 이 폴더 전체를 올립니다.
2. 저장소의 **Actions** 탭을 열면 `Build plugins` 가 자동으로 실행됩니다. Mac 기준 약 20~30분 걸립니다.
3. 끝나면 실행 기록을 눌러 **Artifacts** 에서 `ChordLink-mac` 을 내려받습니다.
4. 압축을 풀고 아래 위치로 복사합니다.

| 파일 | 복사할 위치 |
| --- | --- |
| `AU/*.component` | `~/Library/Audio/Plug-Ins/Components/` |
| `VST3/*.vst3` | `~/Library/Audio/Plug-Ins/VST3/` |
| `CLAP/*.clap` | `~/Library/Audio/Plug-Ins/CLAP/` |

5. 인터넷에서 받은 파일이라 macOS가 막을 수 있습니다. **터미널**에서 한 번 실행하세요.

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Chord Link"*.component
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Chord Link"*.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/CLAP/"Chord Link"*.clap
```

## 방법 B. 내 Mac에서 직접 빌드

1. **Xcode** 를 설치합니다 (App Store). 설치 후 한 번 실행해서 추가 구성요소 설치를 마칩니다.
2. **CMake** 를 설치합니다: [cmake.org/download](https://cmake.org/download/) 에서 macOS 버전을 받거나, Homebrew가 있으면 `brew install cmake`.
3. 터미널에서 이 폴더로 이동한 뒤 실행합니다.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 8
```

처음에는 JUCE를 자동으로 내려받기 때문에 시간이 걸립니다. 빌드가 끝나면 AU·VST3가 `~/Library/Audio/Plug-Ins/` 에 **자동으로 복사**됩니다.
CLAP은 `build/ChordLink_artefacts/Release/CLAP/` 에서 직접 복사하세요.

4. Logic이 AU를 인식하는지 확인합니다.

```bash
auval -a | grep -i "chord link"
```

---

## DAW에서 쓰기

### Logic Pro

- **소리까지 바로**: 소프트웨어 악기 트랙 → 악기 슬롯 → AU Instruments → Chord Link → **Chord Link**
- **다른 악기 소리로**: 원하는 악기를 넣은 트랙 → **MIDI FX** 슬롯 → Audio Units → Chord Link → **Chord Link MIDI FX**
- 플러그인이 안 보이면: Logic Pro → 설정 → 플러그인 관리자 → 다시 검사

### Ableton Live 12

Live는 MIDI 이펙트형 플러그인을 지원하지 않으므로 **악기형 Chord Link** 를 사용합니다.

- **설정**: 설정 → Plug-Ins 에서 *Use Audio Units v2* 또는 *Use VST3 Plug-In System Folders* 를 켜고 Rescan
- **소리까지 바로**: MIDI 트랙에 Chord Link 를 넣으면 내장 소리가 납니다.
- **다른 악기 소리로**:
  1. 트랙 1에 Chord Link 를 넣고 내장 음량을 0으로 내립니다.
  2. 트랙 2에 원하는 악기를 넣습니다.
  3. 트랙 2의 **MIDI From** 을 "1-MIDI" 로, 그 아래 칸을 **Chord Link** 로 선택합니다.
  4. Monitor 를 **In** 으로 둡니다.
- 위 연결은 실제 Live 12에서 한 번 확인해 주세요. 플러그인 MIDI 출력을 받는 방식은 Live 버전과 플러그인 형식(AU/VST3)에 따라 다를 수 있습니다.

> CLAP은 Bitwig, Reaper, FL Studio 등에서 쓸 수 있습니다. Logic과 Live는 CLAP을 지원하지 않습니다.

---

## 폴더 구성

```
Source/ChordEngine.*      코드 엔진 (웹 v1.05와 같은 규칙, JUCE와 무관한 순수 C++)
Source/PluginProcessor.*  MIDI 처리, 상태 저장, 내장 신스 연결
Source/PluginEditor.*     플러그인 화면
Source/Synth.h            내장 소리 (피아노 / 사인파 / 쏘우파 / 스퀘어)
.github/workflows/        GitHub 자동 빌드 설정
tests/editor_shot.cpp     개발용 화면 캡처 테스트 (-DCL_BUILD_TESTS=ON)
```

## 알아둘 점

- **배포**: 다른 사람 Mac에 배포하려면 Apple 개발자 계정으로 코드 서명과 공증을 해야 경고 없이 열립니다. 팀 내부 테스트는 위의 `xattr` 명령으로 충분합니다.
- **라이선스**: JUCE는 개인·소규모 무료 조건(또는 AGPL)을 따릅니다. 판매할 계획이 생기면 JUCE·VST3 라이선스를 다시 확인하세요.
- **얼굴인식**: 카메라 인식은 플러그인 안에 넣지 않았습니다. 별도 앱이 표정 값을 MIDI CC20 으로 보내는 구조입니다. 앱 연결 전에는 화면 슬라이더로 시연할 수 있습니다.

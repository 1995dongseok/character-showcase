# 아티스트 빠른 시작 (CharacterShowcase)

Unreal을 처음 여는 캐릭터 아티스트를 위한 순서다. 위에서부터 한 단계씩 하고, 각 단계의 **확인 방법**이 맞을 때만 다음으로 넘어간다.
필드 하나하나의 의미·설계 규칙·검증 기록은 [CHARACTER_VIEWER_SETUP.md](CHARACTER_VIEWER_SETUP.md)(개발/인계 문서)에 있다. FBX 규칙은 [FBX_IMPORT_GUIDE.md](FBX_IMPORT_GUIDE.md).

`Tools\*.bat`은 전부 더블클릭으로 실행하며 끝나면 창이 "계속하려면 아무 키나…"에서 멈춘다(결과를 읽고 닫으면 된다). 명령 창에서 `--check`를 붙이면 실행할 명령만 보여 주고 아무것도 시작하지 않는다.

## 0. 설치 (한 번만)

1. **Epic Games Launcher** 설치 → 로그인 → 왼쪽 **Unreal Engine** → 위쪽 **라이브러리** → 엔진 버전 옆 **+** → 버전 **5.6.1** 선택 → **설치**. 기본 위치 `C:\Program Files\Epic Games\UE_5.6`을 권장한다(다른 곳이면 `.bat` 실행 전에 `set UE_ROOT=<설치 폴더>`). 용량이 수십 GB라 오래 걸린다.
2. **Visual Studio 2022 Community** 설치 → Visual Studio Installer의 워크로드에서 **"C++를 사용한 게임 개발"(Game development with C++)** 체크. 개발 PC 기준 MSVC 14.38 + Windows SDK 10.0.22621이 깔려 있다. 코드를 고치지 않아도 프로젝트 C++를 한 번 빌드해야 하므로 필요하다.
3. **Git for Windows** 설치(Git LFS 포함). 명령 창에서 한 번:
   ```
   git lfs install
   ```
4. 저장소 받기(경로에 한글·공백이 없는 폴더 권장):
   ```
   git clone https://github.com/1995dongseok/character-showcase
   ```

**확인 방법**
- `git lfs version`이 버전을 출력한다.
- `C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe`가 있다.
- 받은 폴더의 `Content\Portfolio\Maps\LV_Portfolio.umap` 크기가 **수십 KB**다(약 130바이트면 LFS가 안 받아진 것 → 7단계 "Git LFS").
- `Tools\OpenEditor.bat --check`(명령 창)가 `Content : ok`를 출력한다.

## 1. 프로젝트 열기

1. `Tools\BuildEditor.bat` 더블클릭 → C++ 빌드. 끝에 `[OK] Build succeeded.`가 나와야 한다. 개발 PC(i5-9600K, 6코어)에서 C++ 일부 재빌드는 15~95초였다. 막 받은 저장소의 전체 빌드는 측정하지 않았다(수 분 예상).
2. `Tools\OpenEditor.bat` 더블클릭 → Editor가 뜬다. **첫 실행은 셰이더 컴파일 때문에 오래 걸린다**(오른쪽 아래 "Compiling Shaders" 진행 표시, 수십 분이 걸릴 수 있다 — 개발 PC에서 측정하지 않음). 두 번째부터 빨라진다.
   - BuildEditor를 건너뛰면 Editor가 "The following modules are missing or built with a different engine version … Would you like to rebuild them now?"라고 묻는다 → **Yes**.

**확인 방법**: Editor 창에 레벨 `LV_Portfolio`가 자동으로 열리고, 어두운 배경 앞 회색 바닥 위에 흰색 마네킹(Manny)이 서 있다. 아래 **Content Browser**에 `Portfolio`, `PlayDemo`, `Characters` 폴더가 보인다.

## 2. 뷰어 보기

- **창으로 실행**: `Tools\RunViewer.bat` → 1920×1080 창(작은 화면은 명령 창에서 `Tools\RunViewer.bat --720p`).
- **Editor 안에서**: 툴바의 **Play**(▶). PIE에서는 Esc가 Play 종료 키라 촬영 취소로 쓰지 않는다.
- 플레이 데모: `Tools\RunPlayDemo.bat`(W/A/S/D·Shift·마우스, 키 표는 [README](../README.md#조작-키)).

![뷰어 화면](Images/viewer_ui_1280x720.png)

**확인 방법**: 왼쪽에 Manny, 오른쪽에 패널이 보인다 — 맨 위 이름 `Manny (placeholder)`, 그 아래 **CHARACTER**(Manny / Tutorial Mannequin / Skeletal Cube), **VIEW**(Face / Upper Body / Full Body), **DISPLAY**(Turntable, Reset Camera, Clean View, Inspection, Wireframe, Screenshot (F12), Turntable Shots (Shift+F12)), 설명 글, **ANIMATION**(Idle / Walk / Jog / Pose), **EXPRESSION**(Neutral). 패널 위에서 휠을 굴리면 아래 목록이 더 나온다. 지금 선택된 항목에는 `▸` 표시가 붙는다. 왼쪽 드래그로 캐릭터가 돌아가면 성공.

![플레이 데모](Images/playdemo_run.png)

## 3. 내 캐릭터 등록

상세는 [CHARACTER_VIEWER_SETUP.md 2절](CHARACTER_VIEWER_SETUP.md#2-아티스트-작업-절차). C++/Blueprint 코드는 고치지 않는다. 필드 이름은 Editor **Details** 패널에 보이는 이름 그대로다.
**가장 쉬운 길**: Content Browser에서 `Content/Portfolio/Data/DA_Character_Manny` 우클릭 → **Duplicate** → `DA_<캐릭터>`로 이름 변경 → 아래 ②의 Skeletal Mesh만 바꾸고 나머지를 고친다(카메라 구도·애니메이션 행이 이미 180 cm 인체 기준으로 채워져 있다).

- [ ] ① **Import**: Content Browser에서 `Content/Portfolio/Characters/<캐릭터>` 폴더를 만들고 FBX를 끌어다 놓는다([FBX_IMPORT_GUIDE.md](FBX_IMPORT_GUIDE.md)). Physics Asset이 함께 만들어졌는지 확인.
- [ ] ② **Data Asset**: `Content/Portfolio/Data`에서 우클릭 → **Miscellaneous → Data Asset** → 클래스 **CharacterProfileData** → `DA_<캐릭터>`. `Character` 카테고리: **Display Name**, **Description**, **Skeletal Mesh**.
- [ ] ③ **Animation**: **Animations** 배열에 행 추가 — **Id**(예: `Idle`), **Display Name**, **Sequence**(같은 Skeleton의 애니메이션), **Loop**, **Is Pose** + **Pose Time**. 시작 동작은 **Default Animation Id**(또는 AnimBP를 쓸 때 **Default Anim Class**).
- [ ] ④ **Expression**: **Expressions** 행 — **Id**, **Display Name**, **Morphs**(**Morph Name**, **Weight**). Neutral은 Morphs를 비운 행.
- [ ] ⑤ **Material Variant**: **Material Variants** 행 — **Id**, **Display Name**, **Slots**(**Slot Name** 또는 **Slot Index**, **Material**). Id `Default`에 Slots를 비운 행 = 원래 재질로 돌아가는 버튼.
- [ ] ⑥ **Camera**: **Camera Presets** 행(**Id**, **Display Name**, **Framing**: **Target Offset**, **Distance**, **FOV**, **Min/Max Distance**, **Min/Max Pitch**) — Face / Upper Body / Full Body. **Default Preset Id** = R 키가 돌아갈 구도. **Max Distance는 2,500보다 작게**(배경 구 반지름).
- [ ] ⑦ **Parts**: **Parts** 행 — **Id**, **Display Name**, **Part Type**, **Description**, **Bone Names**, **Material Slot Names**(선택), **Triangle Count / Material Name / Texture Resolution**(선택 메모).
- [ ] ⑧ **GameMode**: `Content/Portfolio/Blueprints/BP_CharacterViewerGameMode` 더블클릭 → 위쪽 **Class Defaults** → `Viewer` 카테고리 → **Profile Library**의 **+** 로 `DA_<캐릭터>` 추가. 처음부터 내 캐릭터로 시작하려면 **Default Profile**도 바꾼다 → **Compile** → **Save**.
- [ ] ⑨ **레벨**: `Content/Portfolio/Maps/LV_Portfolio`의 Outliner에서 `PortfolioCharacter` 선택 → Details `Character` → **Profile**을 Default Profile과 같은 에셋으로. **File → Save All**(Ctrl+Shift+S) → **Play**.

**가장 흔한 실수 두 가지**
1. **Bone Names가 Physics Asset의 본 이름과 다르다** → 그 파츠는 클릭해도 선택되지 않는다. 메시를 열어(더블클릭) **Skeleton Tree**의 이름을 대소문자까지 그대로 복사한다. 메시에 **Physics Asset이 없으면 파츠 클릭 자체가 안 된다**(Skeletal Mesh 에디터 → Asset Details → Physics → **Physics Asset** 칸 확인).
2. **Profile Library에 추가하지 않았다** → 패널 CHARACTER 목록에 내 캐릭터 버튼이 없다(⑧).

**확인 방법**: Play 또는 RunViewer에서 CHARACTER 목록에 내 캐릭터 이름이 보이고, 누르면 캐릭터가 바뀌며 이름/설명/ANIMATION 목록이 내 값으로 바뀐다. I를 켜고 몸통을 클릭하면 INSPECTION에 파츠 이름이 나오고 그 부위가 마젠타로 표시된다(슬롯이 맞으면 슬롯 전체, 아니면 관절 위치에 작은 구).

## 4. 검증

`Tools\ValidateProfiles.bat` 더블클릭 → 창 없이 프로젝트를 읽어 모든 `CharacterProfileData`를 검사하고(`Scripts/ValidateProfiles.py`, 별도 작업으로 추가 중 — 아직 없으면 "not present yet"만 출력하고 끝난다), 마지막에 요약을 보여 준다:

```
[ValidateProfiles] DA_Character_Manny: E=0 W=0 I=3
[ValidateProfiles] DA_<캐릭터>: E=0 W=2 I=1
```

**확인 방법**: 내 에셋 줄의 **E=0**(오류 없음). W(경고)는 읽어 보고 의도한 것이면 둔다. 전체 로그는 `Saved\Logs\ValidateProfiles.log`. (위 숫자는 형식 예시이며 실제 값은 스크립트 결과를 본다.)

## 5. 촬영

| 키 | 결과 |
| --- | --- |
| **F12** | UI 없이 뷰포트 × 2 해상도 1장 → `Saved/Screenshots/Portfolio/<프로필>_<구도>_<yyyyMMdd-HHmmss>.png`. 패널 아래에 `Saved: …`가 3초 뜬다 |
| **Shift+F12** | 캐릭터를 10°씩 돌려 36장 → `Saved/Screenshots/Portfolio/Turntable_<프로필>_<시각>/frame_000.png … frame_035.png`. 진행률 `Capturing 12/36` |
| **Esc** | 촬영 취소(이미 찍은 장은 남는다) |

- 찍기 전에 구도(VIEW)·애니메이션(Pose 추천)·Variant를 고르고, 강조를 빼려면 I를 끈다. 패키지 실행 파일에서는 `<압축 푼 폴더>\CharacterShowcase\Saved\Screenshots\Portfolio\`에 저장된다.
- 프레임 → 영상/GIF(ffmpeg 별도 설치, 턴테이블 폴더에서):

```powershell
ffmpeg -framerate 12 -i frame_%03d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -pix_fmt yuv420p -crf 18 turntable.mp4
ffmpeg -framerate 12 -i frame_%03d.png -vf "scale=720:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse" -loop 0 turntable.gif
```

![Clean View 화면 — F12 결과도 이렇게 UI 없이 저장된다](Images/viewer_clean_1280x720.png)

**확인 방법**: 탐색기에서 위 폴더에 PNG가 생겼고, 열어 보면 오른쪽 패널이 없다. 턴테이블은 `frame_035.png`까지 36장.

## 6. 패키지 (보낼 실행 파일 만들기)

`Tools\PackageViewer.bat` 더블클릭 → 빌드·쿡·패키지 후 압축:
- 실행 폴더: `Saved\Packaged\Windows\CharacterShowcase.exe`
- 보낼 파일: `Saved\Packaged\CharacterShowcase-Win64-<yyyyMMdd>.zip`(디버그 파일 `*.pdb` 제외). 이미 패키지가 있고 압축만 다시 하려면 명령 창에서 `Tools\PackageViewer.bat --zip-only`.
- 뷰어 레벨만 들어간다(플레이 데모는 아직 패키지 대상이 아님). 받는 사람은 압축을 풀고 `CharacterShowcase-Win64-<날짜>\CharacterShowcase.exe`를 더블클릭한다. Google Drive/WeTransfer 등으로 링크를 보낸다(메일 첨부 용량을 넘는다).

**확인 방법**: 창 끝에 `[OK] …zip (… bytes)`. zip을 다른 폴더에 풀어 exe를 실행해 Manny가 뜨는지 본다. 다른 PC에서 실행해 본 기록은 아직 없다.

## 7. 문제 해결

| 증상 | 원인 | 해결 |
| --- | --- | --- |
| 화면이 검다 / 레벨이 비어 있다 | 에셋이 LFS 포인터, 또는 다른 레벨이 열림 | `.bat` 출력의 `Content :` 확인 → 아래 "Git LFS". Editor에서는 `Content/Portfolio/Maps/LV_Portfolio`를 연다 |
| 캐릭터가 안 보이거나 거대/아주 작다 | 가져오기 스케일(cm) 문제 | 빈 레벨(**File → New Level → Basic**, 저장하지 않음)에 내 메시와 `SKM_Manny_Simple`(약 180 cm)을 나란히 끌어다 놓고 키를 비교한다. 다르면 FBX_IMPORT_GUIDE 1절대로 다시 내보낸다. 카메라 프리셋의 Distance/Target Offset도 확인 |
| 파츠 클릭이 안 된다 | Physics Asset 없음, 또는 Bone Names 불일치 | 3단계 "흔한 실수 1". Inspection(I)이 켜져 있는지, 새로 놓은 배경 소품이 **NoCollision**인지도 확인 |
| CHARACTER에 내 캐릭터 버튼이 없다 | Profile Library에 미등록 | 3단계 ⑧ → Compile·Save |
| 표정 버튼을 눌러도 그대로 | 메시에 Morph Target이 없음 / 이름 불일치 | FBX 가져오기에서 **Import Morph Targets** 켜기, Morph Name을 메시 에디터의 **Morph Target Preview** 이름과 똑같이 |
| Wireframe이 꽉 찬 면처럼 보인다 | 삼각형이 많아 선이 겹침 | Face 구도로 가까이 가서 본다(정상 동작) |
| `Preparing Shaders (N)`, 강조/와이어가 회색 체커 | 첫 실행 셰이더 컴파일 중 | 기다린다. Editor에서 한 번 Play해 두면 줄어든다. 패키지에서는 나오지 않는다 |
| Editor가 에셋을 못 읽음, `.uasset`이 약 130바이트 | **Git LFS** 파일을 안 받음 | `git lfs install` → 저장소 폴더에서 `git lfs pull` |
| `.bat`이 `editor module missing` | C++ 미빌드 | `Tools\BuildEditor.bat` |
| 느리다(수 FPS) | 내장 그래픽 | 정상(개발 PC UHD 630에서 약 5 FPS). `--720p`로 실행, 키는 조금 길게 누른다 |

![첫 실행 셰이더 컴파일 중 — 왼쪽 위 Preparing Shaders](Images/troubleshoot_preparing_shaders.png)

# Character Portfolio Viewer — 아티스트 인계 문서

이 프로젝트는 UE 5.6.1 기반 3D 캐릭터 포트폴리오 뷰어다. 캐릭터가 중심이며 C++/Blueprint 기능은 관찰과 촬영을 돕는다.
이 문서는 **현재 상태를 앞에, 과거 기록을 뒤에** 둔다. 0~5절만 읽으면 아티스트 작업에 충분하다. 6절은 참고용 개발 이력이다.

## 0. 현재 상태 요약 (2026-10-01)

- **구현 완료(코드)**: P0(카메라/입력/GameMode/기본 UI), P1(카메라 프리셋, Turntable, Animation/Pose, Morph 표정, Slot 재질, Clean View), P2(Inspection 파츠 선택, 선택 강조, Wireframe) 전부 C++로 구현되어 있다. 아티스트는 **C++를 수정하지 않고** 새 캐릭터를 등록할 수 있다(2절).
- **에셋 자동 생성**: `Scripts/CreatePortfolioAssets.py`가 없는 에셋만 만들고, 이미 있는 에셋은 절대 덮어쓰지 않는다(읽기 전용 검증만 함, `[keep] ... OK/DIFFERS`). `Scripts/CreateViewerWidgetLayout.py`는 `WBP_CharacterViewer`에 디자이너 트리가 없을 때만 최소 트리를 만든다. **일상적인 캐릭터 등록(2절)에는 두 스크립트 모두 다시 실행할 필요가 없다** — Editor GUI에서 Data Asset/Blueprint/Level을 직접 편집하면 된다(1절 "언제 스크립트를 실행하지 않는가" 참고).
- **검증된 것(숫자 있음, 4절 표)**: Editor 빌드 0오류/0경고, Game 빌드 0오류/0경고, Editor Automation 6/6 통과(2026-09-29), Win64 Development 패키지 빌드/실행 스모크 통과, Win64 Shipping 패키지 빌드 성공 + 프로세스 정상 기동/종료 확인(자동화 테스트 미포함), 두 Python 스크립트의 "기존 자산 보존" 동작을 해시 비교로 검증.
- **2026-09-30 추가 검증(4절 표)**: `-game` 합성 포인터 스모크가 활성 전면 창에서 1/1 통과(이전 실패 3건 해소). Shipping 패키지에 OS 수준 실제 마우스/키보드 입력(SendInput)을 넣어 드래그 Orbit, 휠 Zoom, R, Space와 드래그 정지, H, I/파츠 클릭/빈 공간 해제, W, 패널 위 휠·드래그 차단, 포커스 상실 복귀를 화면 캡처 25장으로 확인. 증거는 `Docs/Evidence/2026-09-30-shipping-real-input/`.
- **2026-10-01 추가(4절 표, 6.11절)**: 파츠 단위 강조, 메시 수치 실측 API, 스켈레톤 호환 검사 완화, 실제 ProjectID. Editor 빌드 0오류/0경고, Game 빌드 0오류/0경고, Editor Automation **11/11 통과**(신규 4개 포함), `CreatePortfolioAssets.py` 2회 실행(1차 `M_ViewerPartHighlight` 생성 + 기존 7개 `[keep] OK`, 2차 `[keep] OK` ×8). `-game` 스모크는 이 변경 후 아직 실행하지 않았다.
- **2026-10-01 패널 레이아웃·촬영 패스(6.12절)**: 패널 폭이 뷰포트의 24%(300~460 Slate 단위)로 바뀌고 DPI 곡선(720p = 0.8)을 프로젝트에 설정해 720p에서 버튼 글자가 잘리던 문제를 고쳤다. F12(고해상도 스크린샷)·Shift+F12(36장 턴테이블)·Esc(취소) 촬영 기능과 패널 상태 줄(`StatusText`)을 추가했다(1.7절). Editor 빌드/Game 빌드 0/0, Editor Automation 10/10. `-game` 시각 확인은 대기(4.1절).
- **2026-10-01 INSPECTION 실측 수치·Shaded Wireframe·설명 줄 맞춤(6.14절)**: Inspection을 켜면 INSPECTION 섹션에 실측 메시 요약(`Triangles 92,178 · Verts 48,705 · Bones 89 · Slots 2 · LODs 3 · Morphs 0`, Skeleton/Physics), 슬롯별 줄, 선택 파츠의 `Measured:`(슬롯 실측) 또는 `Authored:`(메모)와 `Highlight:` 방식이 표시된다. Wireframe(W)은 기본적으로 **음영 위에 반투명(불투명도 0.35) 청록 선을 겹치는 Overlay**(`M_WireframeOverlay`)로 바뀌어 92k 삼각형 Manny에서도 표면이 선 아래로 보이도록 했다(예전 "슬롯 전체 교체"는 선택 옵션으로 남음). 본 마커는 깊이 테스트를 끈 반투명 마젠타(`M_ViewerBoneMarker`, 지름 12 cm)로 그려 머리·목처럼 메시 안쪽 관절에서도 보이게 했고, 새 파츠를 선택하면 패널 목록이 자동으로 끝까지 스크롤되어 선택 파츠 정보가 보인다. 설명 박스는 DPI 배율마다 정확히 6줄 높이(1080p 126 / 720p 120 Slate 단위)로 맞춰 7번째 줄이 반만 보이지 않는다. Editor 빌드/Game 빌드 0/0, Editor Automation **16/16**, 생성 스크립트 `[keep] OK` ×15. 화면 확인은 대기(4.1절).
- **미검증/대기(4.1절)**: 사람이 손으로 직접 조작한 확인(자동 입력 재생과 구분), 패널 버튼 클릭 자체의 실제 입력 확인(키보드 경로로만 확인), 표정(Expression)의 실제 시각 검증(현재 캐릭터에 Morph Target이 없음), 사람이 만든 디자이너 WBP 레이아웃에서의 hover 동작, 파츠 단위 강조의 실제 화면 확인(`-game`/패키지, 2026-10-01 구현분), Shaded Wireframe·INSPECTION 실측 표시·설명 6줄 맞춤의 화면 확인(6.14절, `CharacterShowcase.Game.ViewerCapture`의 Inspect/Wireframe 캡처), 패널 레이아웃·F12/Shift+F12 촬영의 `-game` 시각 확인(6.12절).
- **파츠 강조는 파츠 단위로 표시된다(2026-10-01, 6.11절).** 우선순위: ⓐ Part의 **Material Slot Names**가 메시 슬롯과 일치하면 그 슬롯만 마젠타 불투명 재질(`M_ViewerPartHighlight`)로 바뀐다. ⓑ 아니면 **Bone Names**의 본과 그 직계 자식 본에 작은 마젠타 구체 마커가 붙는다(예: 팔 → 어깨·팔꿈치·손목). ⓒ 둘 다 해당 없으면 예전처럼 메시 전체에 반투명 마젠타 Overlay가 덮인다(선택이 안 보이는 경우가 없도록). 현재 placeholder(`TutorialTPP`)는 Material Slot이 1개라 ⓑ 본 마커로 표시된다.
- **placeholder 데이터 주의**: `DA_Character_Manny`가 참조하는 `SKM_Manny_Simple`(파츠별 삼각형 9,206~25,680, 6.13절의 측정 방법), `DA_Character`가 참조하는 `TutorialTPP`(6,118 삼각형, Material Slot 1개, 텍스처 0개), `DA_Character_Cube`가 참조하는 `SkeletalCube`(12 삼각형)는 전부 UE 엔진/템플릿이 기본 제공하는 에셋이다. **이 수치는 실제 캐릭터 정보가 아니며**, 실제 아트가 들어오면 각 Part의 `Triangle Count`/`Material Name`/`Texture Resolution`을 그 아트 기준으로 다시 측정해 입력해야 한다.
- **2026-10-01 검증(4절 표)**: `CreatePortfolioAssets.py` 신규 5개 생성 + 재실행 `[keep] OK` ×12, `-game` 스모크 2회 — 커서가 게임 창 밖이라 합성 포인터 전제 조건 1건으로 Fail, 나머지 assertion 오류 0(Manny Torso 클릭 포함), 화면 측정으로 바닥 중간 회색·가슴 클리핑 0 확인. 비쿠킹 실행이라 Manny의 강조/Wireframe 셰이더는 화면에 아직 안 나왔다(4.1절).
- **기본 프로필 = `DA_Character_Manny` (2026-10-01)**: 엔진 3인칭 템플릿 마네킹 `SKM_Manny_Simple`(본 89개, LOD0 92,178 삼각형, Material Slot 2개 `M_HeadLegs`/`M_Torso`, 텍스처 1024²·Torso 노멀만 4096², Physics Asset `PA_Mannequin`, Morph 0)을 쓰는 placeholder 프로필이다. 시작 시 Idle(`MM_Idle`)로 서 있고 Full Body 구도로 보인다. `ProfileLibrary` 순서는 Manny → Tutorial Mannequin(`DA_Character`) → Skeletal Cube(`DA_Character_Cube`)이며, 기존 두 프로필은 변경 없이 남아 있다(2절 ⑧).
- **스튜디오 룩 (2026-10-01)**: `LV_Portfolio`는 Key/Fill/Rim 3점 조명(전부 Directional, Movable), 고정 노출 `StudioPostProcess`(Manual, 보정 0), 어두운 그라데이션 배경 구(`MI_StudioBackdrop`), 18% 중간 회색 바닥(`MI_StudioFloor`, 멀어질수록 배경으로 페이드)으로 바뀌었다. 배경/바닥은 NoCollision이라 파츠 클릭을 가로채지 않는다. 값과 조정 위치는 2절 ⑨, 변경 이력은 6.13절.
- **프로필 검증 도구 (2026-10-01, 2.10절)**: `Scripts/ValidateProfiles.py`가 모든 `CharacterProfileData`를 메시와 대조해 틀린 본/슬롯/Morph 이름, 잘못된 카메라 범위, 없는 기본 애니메이션, 호환 안 되는 Skeleton, Physics Asset 누락 등을 필드 위치와 고치는 방법까지 한국어로 출력한다(Error가 있으면 종료 코드 ≠ 0). 현재 3개 프로필: Manny·Tutorial **E=0 W=0 I=0**, Cube **E=0 W=2**(Physics Asset 없음 → 파츠 클릭 불가, 높이 25 cm 스케일 확인).
- **재생 컨트롤·LOD 표시·배경 프리셋 (2026-10-01, 1.7절, 6.18절)**: 패널 ANIMATION 아래 **PLAYBACK**(P 일시정지/재개, `[`/`]` 한 프레임 이동, `-`/`=` 속도 0.25 단위 0.1~2.0배, `0` = 1.0배, 시간 줄 `0.10 s / 7.57 s · frame 3 / 227`), DISPLAY에 **`LOD: Auto (L)`**(Auto → LOD0 → LOD1 → LOD2, INSPECTION 삼각형 수가 표시 LOD 기준으로 바뀜 — Manny LOD0 92,178 / LOD1 26,998 / LOD2 12,998)와 **`Backdrop: Studio (B)`**(Studio → Black → White → Mid Grey 실루엣 확인, 런타임 Dynamic MI만 써서 레벨/에셋 무수정). Editor 빌드/Game 빌드 0/0, Editor Automation **24/24**(신규 4개). 화면 확인은 대기(4.1절).
- 어디를 보면 되는지: 실행 명령 → 1절, 캐릭터 등록 절차 → 2절, 책임 분리 규칙 → 3절, 검증 수치 전체 → 4절, 남은 위험 → 5절, 과거 실패/원인 분석 상세 기록 → 6절.

## 1. 실행 방법

**처음이면 [README.md](../README.md) → [ARTIST_QUICKSTART.md](ARTIST_QUICKSTART.md) 순서로 본다**(FBX 규칙은 [FBX_IMPORT_GUIDE.md](FBX_IMPORT_GUIDE.md)). 아래 명령은 전부 `Tools\*.bat`으로 감싸 두었다 — 더블클릭으로 실행하고, `--check`를 붙이면 실행할 명령만 출력한 뒤 종료 코드 0으로 끝난다(아무것도 시작하지 않음). 엔진 경로는 기본 `C:\Program Files\Epic Games\UE_5.6`, 환경 변수 `UE_ROOT`로 바꿀 수 있다. 이 PC는 Launcher가 5.6을 레지스트리에 등록하지 않아 `.uproject` 더블클릭/우클릭 메뉴가 동작하지 않을 수 있으므로 .bat 사용을 권장한다. 공통 처리(경로·엔진 탐지, C++ 모듈 DLL·LFS 포인터 검사)는 `Tools\_Common.bat`에 있다.

### 1.1 Editor에서 열기

**`Tools\OpenEditor.bat`**(C++ 모듈이 아직 없으면 먼저 **`Tools\BuildEditor.bat`**, 1.4절). 직접 실행하려면 `CharacterShowcase.uproject`를 더블클릭하거나 `UnrealEditor.exe`에 직접 넘긴다(설치 경로: `C:\Program Files\Epic Games\UE_5.6`).

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" `
    "C:\Users\WINCARD1\Downloads\develop\project\character-showcase\CharacterShowcase.uproject"
```

`EditorStartupMap`이 `LV_Portfolio`로 설정되어 있어 Editor가 이 레벨을 자동으로 연다.

### 1.2 PIE (Play In Editor)

`Tools\OpenEditor.bat`으로 연 Editor에서 툴바 **Play**. Editor 툴바의 **Play** 버튼으로 사람이 직접 확인한 기록은 아직 없다(4.1절 — 지금까지의 실행 검증은 전부 `-game`/패키지 프로세스와 Editor Automation으로 이루어졌다). 아티스트는 캐릭터를 등록한 뒤 Play로 직접 눌러 확인하는 것을 권장한다.

### 1.3 `-game` 커맨드 (렌더링 실확인, NullRHI 아님)

**`Tools\RunViewer.bat`**(`/Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=1920 -ResY=1080`, `--720p`면 1280×720)과 **`Tools\RunPlayDemo.bat`**(같은 인자로 `/Game/PlayDemo/Maps/LV_PlayDemo`)이 이 명령을 감싼다. 둘 다 `-log -unattended -nosplash`는 붙이지 않는다(아티스트용). 아래는 스모크/테스트용 원래 명령이다.

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" `
    "C:\Users\WINCARD1\Downloads\develop\project\character-showcase\CharacterShowcase.uproject" `
    /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=1280 -ResY=720 -log -unattended -nosplash
```

**주의**: 자동화 테스트가 포함된 스모크(`CharacterShowcase.Game.ViewerSmoke`)의 합성 마우스/키보드 단계는 **게임 창이 활성·비가려짐 포그라운드 창일 때만** 정확히 동작한다. 엔진 동작상 앱이 비활성 상태면 Slate 자체 커서가 hover를 지우고, 창이 배경으로 밀려 있으면 Win32가 마우스 캡처를 거부한다. 최소화/숨김 창으로 실행하지 말 것. 6.10절에 기록된 원인으로 스크린샷/합성 입력이 모두 실패한다. 스모크는 `FSlateApplication::SetCursorPos`로 실제 OS 커서를 움직이므로 실행 중(약 12분) PC를 조작하지 않는다. 2026-09-30 이 조건에서 1/1 통과했다.

실제 입력 재생 검증(Shipping 등 자동화 테스트가 없는 빌드): `Docs/Evidence/2026-09-30-shipping-real-input/ViewerInputDriver.ps1`이 user32 `SendInput`으로 실제 마우스/키보드 이벤트를 보내고 단계마다 창을 캡처한다. 게임 창이 전면 창임을 확인한 뒤에만 입력을 보내며, 아니면 중단한다. 실행 중 PC를 조작하지 않는다. 이 PC는 약 5 FPS라 키 입력은 400 ms 이상 누르고 드래그는 한 단계 250 ms 간격으로 보내야 엔진이 인식한다(60 ms 탭은 프레임 사이에 묻힘).

### 1.4 빌드/테스트 명령

Editor 빌드는 **`Tools\BuildEditor.bat`**(`Build.bat CharacterShowcaseEditor Win64 Development -Project=... -WaitMutex`, 실행 중인 Editor의 Live Coding이 있으면 막히므로 Editor를 닫고 실행). 프로젝트 파일 생성·Game 빌드·Automation은 .bat이 없다 — 아래 명령을 쓴다.

```powershell
$ueRoot = 'C:\Program Files\Epic Games\UE_5.6'
$proj = 'C:\Users\WINCARD1\Downloads\develop\project\character-showcase\CharacterShowcase.uproject'

# 프로젝트 파일 생성
& "$ueRoot\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles "-project=$proj" -game -engine

# Editor 빌드
& "$ueRoot\Engine\Build\BatchFiles\Build.bat" CharacterShowcaseEditor Win64 Development "-Project=$proj" -WaitMutex

# Game(-game/패키지용) 빌드
& "$ueRoot\Engine\Build\BatchFiles\Build.bat" CharacterShowcase Win64 Development "-Project=$proj" -WaitMutex
```

Automation 테스트(`CharacterShowcase.Profile.NullSafety`, `CharacterShowcase.Viewer.*` — 2026-10-01부터 `Viewer.PanelLayout`/`Viewer.CaptureNaming`/`Viewer.CaptureSequence` 포함, `CharacterShowcase.Game.*`는 NullRHI에서 실행되지 않음):

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $proj -unattended -nop4 -nosound -NullRHI `
    '-ExecCmds=Automation RunTests CharacterShowcase' '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Saved\Automation"
```

### 1.5 패키지 명령 (Development / Shipping)

**`Tools\PackageViewer.bat`**이 아래 Development 명령(같은 플래그)을 실행한 뒤 `Saved\Packaged\Windows`를 `*.pdb`와 `CharacterShowcase-Win64-Shipping.*`를 빼고 복사해 `Saved\Packaged\CharacterShowcase-Win64-<yyyyMMdd>.zip`(PowerShell `Compress-Archive`, zip 안 최상위 폴더 `CharacterShowcase-Win64-<yyyyMMdd>`)으로 압축한다. `--zip-only`는 패키징을 건너뛰고 압축만 한다. Shipping용 .bat은 없다. `MapsToCook`이 `LV_Portfolio`뿐이라 플레이 데모 맵은 패키지에 들어가지 않는다.

```powershell
& "$ueRoot\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$proj" `
    -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive `
    -archivedirectory="C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Saved\Packaged" `
    -unattended -noP4 -utf8output
```

Shipping은 `-clientconfig=Shipping`으로 동일하게 실행한다. Shipping 빌드는 `WITH_DEV_AUTOMATION_TESTS`가 꺼져 있어 자동화 테스트로 검증할 수 없다 — 배포 전 사람이 직접 화면을 확인해야 한다(4.1절).

### 1.6 두 Python 스크립트 — 언제 실행하고, 언제 실행하지 않는가

아래 두 생성 스크립트는 일부러 .bat으로 감싸지 않았다(일상 작업에서 실행할 일이 없음). 프로필 검사 스크립트 `Scripts/ValidateProfiles.py`(별도 작업으로 추가)는 **`Tools\ValidateProfiles.bat`**이 `UnrealEditor-Cmd ... -ExecutePythonScript=<abs>\Scripts\ValidateProfiles.py -NullRHI -unattended -nosplash -nop4 -abslog=Saved\Logs\ValidateProfiles.log`로 실행하고, 끝나면 로그에서 `[ValidateProfiles]` 줄(`<asset>: E=0 W=.. I=..`)만 다시 보여 준다. 스크립트가 없으면 "not present yet"만 출력하고 종료 코드 0.

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$proj" `
    "-ExecutePythonScript=C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Scripts\CreatePortfolioAssets.py" `
    -unattended -nosplash -nop4 -log
```

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$proj" `
    "-ExecutePythonScript=C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Scripts\CreateViewerWidgetLayout.py" `
    -unattended -nosplash -nop4 -log
```

- **`CreatePortfolioAssets.py`**는 `DA_Character`, `DA_Character_Cube`, `DA_Character_Manny`, `BP_CharacterViewerGameMode`, `WBP_CharacterViewer`, `LV_Portfolio`, `M_Wireframe`, `M_ViewerPartHighlight`, `M_ViewerHighlight`, `M_StudioBackdrop`, `MI_StudioBackdrop`, `M_StudioFloor`, `MI_StudioFloor` **12개가 존재하지 않을 때만** 새로 만든다(2026-10-01에 뒤의 5개 추가). 이미 있으면 절대 덮어쓰지 않고 `[keep] <경로> OK` 또는 `[keep] <경로> DIFFERS: <내용>` 한 줄만 출력한다.
- **일상적인 캐릭터 등록(2절)에는 이 스크립트를 다시 실행할 필요가 없다.** 새 캐릭터는 새 `CharacterProfileData` Data Asset을 Editor GUI로 직접 만들고 `ProfileLibrary`에 추가하면 된다 — 스크립트는 "프로젝트 최초 세팅 / 필수 에셋 12개 중 일부가 삭제되어 없어졌을 때"에만 쓴다. `LV_Portfolio`가 없어서 새로 만들 때는 스튜디오 조명/배경/PostProcess(`apply_studio_setup()`)까지 함께 만든다.
- 스크립트가 만든 에셋을 최신 생성 로직으로 다시 만들고 싶을 때만, 그 에셋을 Editor에서 직접 삭제한 뒤 재실행한다(스크립트가 "없는 에셋"으로 인식해 새로 만든다). 기존 값을 스크립트로 되돌리는 용도로 쓰지 않는다.
- **`CreateViewerWidgetLayout.py`**는 `WBP_CharacterViewer`(`widget_tree.root_widget`)가 비어 있을 때만 기본 트리(필수 7개 + 선택 `StatusText`)를 만든다. 이미 트리가 있으면(사람이 디자이너에서 편집했거나 이전에 생성됐으면) `[keep] ... not modified.`만 출력하고 아무것도 바꾸지 않는다. **디자이너에서 스타일을 다듬은 뒤에는 이 스크립트를 다시 실행해도 안전하지만 실행할 이유가 없다.**
- 기본 레이아웃을 최신 생성 로직으로 다시 만들려면 `WBP_CharacterViewer`를 삭제하고 이 스크립트만 다시 실행한다(2026-10-01부터 에셋이 없으면 빈 WBP(부모 `CharacterViewerWidget`)부터 만든다 — `CreatePortfolioAssets.py`를 함께 돌릴 필요 없음). 트리는 런타임 C++ 폴백과 같은 함수(`UCharacterViewerWidget::BuildDefaultLayoutTree()`)로 만들어지므로 두 경로의 모양이 같다.

### 1.7 조작 키 · 촬영(포트폴리오 캡처)

`Tools\RunViewer.bat`/PIE에서 쓰는 키다. 뷰어 키와 플레이 데모 키를 한 표로 모은 아티스트용 요약은 [README.md](../README.md#조작-키), 촬영 순서는 [ARTIST_QUICKSTART.md](ARTIST_QUICKSTART.md) 5단계.

| 키 | 동작 | 패널 버튼 |
| --- | --- | --- |
| 왼쪽 드래그 | Orbit (패널 위에서 시작한 드래그는 무시) | — |
| 휠 | Zoom (패널 위에서는 패널 스크롤) | — |
| R | 카메라 Reset (Default Preset) | Reset Camera (R) |
| Space | Turntable On/Off | Turntable: On/Off (Space) |
| H | Clean View (UI·강조·커서 숨김) | Clean View (H) |
| I | Inspection On/Off | Inspection: On/Off (I) |
| W | Wireframe On/Off | Wireframe: On/Off (W) |
| **F12** | 고해상도 스크린샷 1장 | Screenshot (F12) |
| **Shift+F12** | 턴테이블 연속 촬영(36장) | Turntable Shots (Shift+F12) |
| **Esc** | 진행 중인 촬영 취소 | — |
| **P** | 애니메이션 일시정지/재개 (Pose 항목은 항상 정지) | Pause (P) / Resume (P) — PLAYBACK |
| **[** / **]** | 한 프레임 뒤로/앞으로 (자동 일시정지, 끝에서 처음으로 순환) | ◀ ([) / ▶ (]) — PLAYBACK |
| **-** / **=** | 재생 속도 0.25씩 감소/증가 (0.1~2.0배, 숫자패드 -/+ 도 동작) | Slower (-) / Faster (=) — PLAYBACK |
| **0** | 재생 속도 1.0배로 | Rate 1.00 (0) — PLAYBACK |
| **L** | 표시 LOD 순환 Auto → LOD0 → LOD1 → … → Auto | LOD: Auto (L) |
| **B** | 배경/바닥 색 순환 Studio → Black → White → Mid Grey | Backdrop: Studio (B) |

키는 전부 `ACharacterViewerController`의 런타임 폴백 Enhanced Input(`EnsureFallbackInputAssets`)으로 매핑된다. 기존 키는 그대로이고 F12/Shift+F12/Esc만 추가됐다(`IA_ViewerScreenshot`, `IA_ViewerTurntableCapture`(F12 + Shift 코드(chord) 트리거), `IA_ViewerCaptureShift`, `IA_ViewerCancelCapture`). 2026-10-01(6.18절)에 P / [ / ] / - / = / 0 / L / B가 추가됐다(`IA_ViewerToggleAnimationPause`, `IA_ViewerStepAnimationBack`/`Forward`, `IA_ViewerAnimationRateDown`/`Up`/`Reset`, `IA_ViewerCycleLOD`, `IA_ViewerCycleBackdrop`, 기존 키와 겹치지 않음).

**재생 컨트롤·LOD·배경 프리셋 (6.18절)**

- **PLAYBACK**(패널 ANIMATION 아래): 특정 프레임의 포즈·변형을 멈춰 놓고 보는 용도. 시간 줄 `0.45 s / 1.20 s · frame 14 / 36`(현재 시간 / 길이 · 현재 프레임 / 마지막 프레임 번호, 시퀀스의 샘플링 프레임레이트 기준)이 프레임이 바뀔 때만 갱신된다. 일시정지와 속도는 다른 애니메이션을 골라도 유지되고(일시정지 중이면 새 애니메이션은 0초에서 멈춘 채 시작), 캐릭터(프로필)를 바꾸면 재생·1.0배로 돌아간다. Pose 항목은 항상 정지 상태라 Pause 버튼이 비활성이고, 프레임 이동은 된다. 프로필이 Animation Blueprint(`DefaultAnimClass`)로 움직이면 버튼이 전부 비활성이고 시간 줄은 `Animation Blueprint drives this mesh`.
- **LOD**: 기본 `Auto`는 엔진이 화면 크기로 LOD를 고른다. L을 누르면 그 LOD를 강제로 표시하고(`APortfolioCharacterActor::SetForcedLOD`, 엔진 규칙 1 = LOD0), INSPECTION의 `Triangles`/`Verts`와 슬롯별 삼각형 수가 **그 LOD 기준**으로 바뀐다(Auto일 때는 LOD0 기준). 캐릭터를 바꾸면 Auto로 돌아간다.
- **Backdrop**: 실루엣 확인용. `Black`(0), `White`(0.8 Linear — 1.0이면 고정 노출에서 흰색이 날아간다), `Mid Grey`(0.18)는 배경 구(`MI_StudioBackdrop`의 `TopColor`/`BottomColor`)와 바닥(`MI_StudioFloor`의 `BaseColor`/`EdgeColor`)을 같은 단색으로 바꾸고, `Studio`는 레벨에 저작된 원래 값으로 되돌린다. 조명·노출은 바꾸지 않는다. 런타임 Dynamic Material Instance만 쓰므로 **레벨/MI 에셋은 수정되지 않는다**. Clean View와 캐릭터 전환 뒤에도 유지된다. 배경/바닥 Actor는 재질(또는 그 부모)이 `MI_StudioBackdrop`/`MI_StudioFloor`인 Static Mesh로 찾는다(쿡된 빌드에는 Outliner 라벨이 없으므로; Editor에서는 라벨 `StudioBackdrop`/`PlatformCylinder`도 인정). 없으면 아무것도 바꾸지 않고 로그 1줄만 남긴다.

**촬영 방법과 결과 위치**

- **F12**: 현재 화면을 **UI 없이**, 선택 강조는 화면 그대로 둔 채 뷰포트 크기 × `ScreenshotResolutionMultiplier`(기본 2, `ACharacterViewerController`의 `Capture` 카테고리 — Editor에서 바꾸려면 이 클래스의 Blueprint 자식을 만들어 GameMode의 Player Controller Class로 지정)로 저장한다. 파일: `Saved/Screenshots/Portfolio/<프로필 에셋 이름>_<프리셋 Id>_<yyyyMMdd-HHmmss>.png` (예: `DA_Character_Full_20261001-142530.png`). 패널 아래 상태 줄에 `Saved: <상대 경로>`가 약 3초 표시된다.
- **Shift+F12**: 캐릭터 Actor를 10°씩 돌리며 36장(`TurntableStepDegrees`)을 찍는다. 촬영 중에는 Turntable이 멈추고 카메라 Orbit/Zoom/R/Space는 무시되며, 끝나거나 취소되면 원래 회전·Turntable 상태로 돌아간다. 프레임마다 `CaptureSettleFrames`(기본 4) 틱을 기다린 뒤 1장씩 저장한다. 상태 줄에 `Capturing 12/36` 형태로 진행률이 보이고, **Esc**로 취소할 수 있다(이미 저장된 프레임은 남는다). 결과: `Saved/Screenshots/Portfolio/Turntable_<프로필>_<타임스탬프>/frame_000.png` … `frame_035.png`.
- 이미지는 Slate가 패널을 그리기 전의 씬 뷰포트를 읽는 엔진 스크린샷 경로(`FScreenshotRequest::RequestScreenshot(..., bShowUI=false)`, 배율 > 1이면 `GetHighResScreenshotConfig().SetResolution`)로 저장되므로 패널은 이미지에 들어가지 않는다. 요청 후 `CaptureTimeoutSeconds`(6초) 안에 파일이 생기지 않으면 그 프레임만 스모크 테스트와 같은 `FSlateApplication::TakeScreenshot`(그 순간만 패널 숨김, 뷰포트 크기)으로 대신 저장하고 로그에 `falling back`을 남긴다.
- 패키지 빌드에서는 `Saved`가 패키지 폴더 아래(`<패키지>/CharacterShowcase/Saved/Screenshots/Portfolio/`)에 생긴다.

**프레임 → 영상/GIF (ffmpeg, 별도 설치)** — 턴테이블 폴더에서:

```powershell
# MP4 (12fps = 3초에 한 바퀴, 짝수 해상도 보정)
ffmpeg -framerate 12 -i frame_%03d.png -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -pix_fmt yuv420p -crf 18 turntable.mp4
# GIF (가로 720px, 팔레트 생성으로 색 손실 최소화, 무한 반복)
ffmpeg -framerate 12 -i frame_%03d.png -vf "scale=720:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse" -loop 0 turntable.gif
```

**레이아웃/촬영 `-game` 확인용 테스트** (`CharacterShowcase.Game.ViewerCapture`, 약 30~60초, 포인터 합성 없음): 해상도별 창 캡처(UI/Clean View)를 `Saved/Screenshots/ViewerCapture/ViewerCapture_<UI|Clean>_<W>x<H>.png`로 남기고, F12 경로 파일 생성·크기, Shift+F12 2프레임 저장 후 취소 시 회전/Turntable 복원을 확인한다. 1280×720과 1920×1080에서 각각 실행한다.

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor.exe" $proj /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=1280 -ResY=720 `
    -log -unattended -nosplash '-ExecCmds=Automation RunTests CharacterShowcase.Game.ViewerCapture' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Saved\Automation\ViewerCapture720"
```

### 1.8 일괄 촬영·턴테이블 영상

**`Tools\CaptureAll.bat`** — 포트폴리오 이미지 한 벌을 한 번에 찍는다. 뷰어를 `-game`(1920×1080 창, `--720p`면 1280×720)으로 띄워 콘솔 명령 `Viewer.CaptureAll ... quit=1`을 실행하고, 다 찍으면 창이 저절로 닫힌 뒤 새 `Batch_*` 폴더와 PNG 개수, 로그(`Saved\Logs\CaptureAll.log`)의 `[CharacterViewerBatch]` 줄을 보여 준다. 새 파일이 0장이면 종료 코드 1.

| 옵션 | 콘솔 인자 | 내용 |
| --- | --- | --- |
| (기본) | `profile=current` | 시작 프로필(GameMode Default Profile)만 |
| `--all` | `profile=all` | GameMode `ProfileLibrary`의 모든 프로필(목록 순서, 프로필마다 전환 후 대기) |
| `--expressions` | `expressions=1` | Expression이 2개 이상인 프로필은 표정마다 한 벌 더(Neutral 하나뿐이면 변화 없음) |
| `--poses` | `poses=1` | `Is Pose` 애니메이션마다 1장 추가(기본 구도 `DefaultPresetId` + 첫 Variant) |

- **무엇을 찍나**: 프로필마다 **카메라 프리셋 × Material Variant**(프리셋/Variant가 없으면 각각 `Default` 1개) 조합을 F12와 똑같이 UI 없이, 뷰포트 × `ScreenshotResolutionMultiplier`(기본 2)로 1장씩. Skeletal Mesh가 없는 프로필은 건너뛴다. Manny는 3 × 2 = **6장**(`--poses`면 7장).
- **결과**: `Saved/Screenshots/Portfolio/Batch_<프로필>_<yyyyMMdd-HHmmss>/<프로필>_<프리셋>_<Variant>[_<표정>|_<포즈>].png` — 예: `DA_Character_Manny_Face_Default.png`, `DA_Character_Manny_Full_Grid.png`, `DA_Character_Manny_Full_Default_Pose.png`. 시각은 폴더 이름에만 있어 같은 조합은 언제나 같은 파일 이름이다(포트폴리오 문서에 링크하기 쉽다).
- **뷰어 안에서 직접**: `RunViewer.bat`/PIE의 콘솔(`~` 키)에 `Viewer.CaptureAll`(지금 프로필), `Viewer.CaptureAll profile=DA_Character_Cube`, `Viewer.CaptureAll profile=all poses=1` 등. 상태 줄에 `Batch 3/12`, 끝나면 `Batch saved 6/6 -> Saved/Screenshots/Portfolio/Batch_...`. **Esc**는 일괄 촬영 전체를 취소한다(이미 저장된 파일은 남음). 끝나거나 취소되면 원래 프로필·구도·Variant·표정·애니메이션·Turntable 상태로 돌아간다(손으로 돌려 둔 Orbit 각도는 저장되지 않고, 선택돼 있던 프리셋 구도로 돌아간다).
- **한 장의 순서**: 프로필 표시 → 1초 + 셰이더 컴파일 대기(비쿠킹 첫 실행의 `Preparing Shaders`, 최대 300초) → 선택 적용 → `CaptureSettleFrames`(4) 틱 + 카메라 보간 끝(최대 5초) → 텍스처 스트리밍(최대 2초) → F12와 같은 저장 경로(6초 안에 안 되면 Slate 캡처 폴백). Turntable은 촬영 동안 멈춘다.

**`Tools\MakeTurntableVideo.bat [폴더] [--gif] [--fps 12]`** — Shift+F12 결과를 영상으로 만든다. 폴더를 주지 않으면 `Saved\Screenshots\Portfolio\Turntable_*` 중 가장 최근 폴더. 같은 폴더에 `turntable.mp4`(H.264, yuv420p, CRF 18, 홀수 해상도는 짝수로 보정)를, `--gif`면 가로 640px `turntable.gif`(팔레트 2단계, 무한 반복)를 만든다. ffmpeg는 **PATH → `%LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg*` → `Toolsfmpegfmpeg.exe`** 순서로 찾고, 없으면 설치 명령 `winget install --id Gyan.FFmpeg --scope user --accept-package-agreements --accept-source-agreements`(관리자 권한 불필요)을 출력하고 종료 코드 2로 끝난다. UE 설치와 무관하게 동작한다. Shift+F12가 끝나면 상태 줄에도 `- video: Tools\MakeTurntableVideo.bat`이 붙는다.

## 2. 아티스트 작업 절차

아래 절차는 전부 Editor GUI로 수행하며 C++/Blueprint 코드 수정이 필요 없다. Data Asset의 필드명은 Editor Details 패널에 보이는 표시 이름(예: `TargetOffset` → **Target Offset**) 그대로 적었다.

### ① Mesh/Texture/Material Import

Content Browser에서 `Portfolio/Characters/<캐릭터 이름>` 폴더(없으면 새로 만든다)에 최종 Skeletal Mesh와 필요한 Texture/Material을 Import한다. 원본 제작 파일(.ztl/.ma/.mb/PSD 등)은 이 프로젝트에 들여오지 않는다.

### ② CharacterProfileData 생성

Content Browser 우클릭 → **Miscellaneous → Data Asset** → Pick Data Asset Class에서 **CharacterProfileData** 선택 → `Portfolio/Data`에 저장(예: `DA_<캐릭터>`).

`Character` 카테고리에서 지정:
- **Display Name**, **Description** (멀티라인)
- **Skeletal Mesh** — Import한 캐릭터 메시

### ③ Animation/Pose 등록

`Animation` 카테고리의 **Animations** 배열(항목 타입 `FViewerAnimationEntry`)에 행을 추가한다. 각 행:
- **Id**(안정적인 키, 예: `Idle`), **Display Name**, **Sequence**(같은 Skeleton의 Animation Sequence), **Loop**(체크 시 반복), **Is Pose**(체크 시 재생하지 않고 **Pose Time** 초 지점에 고정)

캐릭터의 기본 재생 상태는 **Default Anim Class**(AnimBP를 쓸 경우) 또는 **Default Animation Id**(위 Animations의 Id 하나, AnimBP를 안 쓸 경우)로 지정한다.

### ④ Morph 이름·가중치로 Expression 등록

`Expression` 카테고리의 **Expressions** 배열(`FViewerExpression`)에 행을 추가한다. 각 행: **Id**, **Display Name**, **Morphs**(`FViewerMorphWeight` 배열 — **Morph Name**, **Weight**).

Neutral 표정은 **Morphs를 빈 배열로 둔 행**으로 등록한다. 메시에 없는 Morph 이름을 넣지 않는다(런타임에 무시됨). **Mesh에 실제 Morph Target이 없으면 이 섹션의 시각적 결과는 검증할 수 없다** — 지금 등록된 두 placeholder 프로필 모두 Morph가 없는 메시라 Expression은 스키마/무충돌만 확인됐고 화면상 표정 변화는 미검증으로 남긴다(0절, 6.9절).

### ⑤ 슬롯별 Material Variant 등록

`Appearance` 카테고리의 **Material Variants** 배열(`FViewerMaterialVariant`)에 행을 추가한다. 각 행: **Id**, **Display Name**, **Slots**(`FViewerMaterialSlotOverride` 배열 — **Slot Name** 또는 **Slot Index**, **Material**).

바꾸지 않을 Slot은 그 Variant의 Slots에서 비워 두면 원래(기본) 재질이 유지된다(부분 override). "Default"라는 Id로 override가 전혀 없는 행을 하나 두면 원래 재질로 되돌리는 버튼이 된다.

### ⑥ Face/Upper/Full 구도 조정

`Camera` 카테고리:
- **Default Framing**(`FViewerCameraFraming`) — CameraPresets가 비어 있을 때의 기본 전신 구도. 필드: **Target Offset**, **Distance**, **FOV**, **Min Distance**, **Max Distance**, **Min Pitch**, **Max Pitch**
- **Camera Presets** 배열(`FViewerCameraPreset`) — 각 행 **Id**, **Display Name**, **Framing**(위와 같은 필드). Face/Upper Body/Full Body 등 원하는 만큼 등록한다.
- **Default Preset Id** — Reset(R)이 복귀할 프리셋의 Id. 비어 있거나 못 찾으면 Default Framing으로 대체된다.

### ⑦ 본·충돌 기반 파츠 매핑과 기술정보 등록

`Part` 카테고리의 **Parts** 배열(`FViewerPartInfo`)에 행을 추가한다. 각 행: **Id**, **Display Name**, **Part Type**, **Description**, **Bone Names**(배열), **Component Tag**, **Material Slot Names**(배열, 선택), **Triangle Count**, **Material Name**, **Texture Resolution**(마지막 셋은 선택 메모).

- 현재 구조(단일 `SkeletalMeshComponent`)에서는 **Bone Names**로 파츠를 식별한다. 클릭 지점의 `BoneName`이 어느 Part의 Bone Names와도 정확히 일치하지 않으면 부모 본을 최대 10단계까지 걸어 올라가며 다시 찾는다(예: 손가락 본 → `hand_l` → "왼팔"). 여기 적는 본 이름은 **메시의 Physics Asset에 실제로 존재하는 본 이름과 정확히 같아야** 하며, 파츠 클릭이 되려면 **메시에 Physics Asset이 할당되어 있어야 한다**(Physics Asset이 없으면 그 캐릭터의 파츠는 클릭되지 않는다 — 지금의 `DA_Character_Cube`가 이 경우다).
- 파츠가 별도 Component(예: Face/Hair/Jacket이 각각 다른 SkeletalMeshComponent)로 구성된 캐릭터라면 **Component Tag**로 식별 방식을 바꿀 수 있다(스키마는 이미 지원, 현재 placeholder는 미사용).
- **Material Slot Names**: 캐릭터의 파츠가 별도 Material Slot/섹션으로 나뉘어 있으면(예: Body/Head/Hair/Jacket) 그 파츠를 이루는 슬롯 이름을 Skeletal Mesh 에디터에 보이는 이름 그대로 적는다. 하나라도 메시 슬롯과 일치하면 ① 선택 시 정확히 그 슬롯들만 마젠타로 바뀌고 ② 그 파츠의 삼각형 수/재질 이름/텍스처 요약을 Viewer가 직접 측정한다. 슬롯이 1개뿐인 메시(지금의 `TutorialTPP`)는 비워 둔다 — 그러면 Bone Names의 본 위치에 구체 마커가 표시된다.
- **선택 강조 우선순위**: Material Slot Names(일치하는 슬롯만 교체) → Bone Names(본과 직계 자식 본에 지름 12cm 반투명 마젠타 구체, 메시를 통과해 보임, 충돌/그림자 없음) → 둘 다 없으면 메시 전체 반투명 Overlay. Wireframe(기본 = 음영 위 반투명 청록 선 Overlay, 6.14절)이 켜져 있어도 선택된 슬롯의 마젠타와 본 마커는 선 아래에 그대로 보이고, Wireframe/Variant를 바꾸거나 선택을 풀면 모든 슬롯이 정확히 원래 상태로 돌아온다. 단 메시의 Overlay 슬롯은 하나뿐이라 **Wireframe이 켜진 동안에는 "메시 전체 반투명 Overlay" 강조 대신 Wireframe 선이 표시**되고(Custom Depth만 유지, 로그 1줄), INSPECTION의 `Highlight:` 줄에 그 사실이 적힌다. Clean View(H) 중에는 강조가 전부 숨겨진다(Wireframe은 유지).
- **Triangle Count / Material Name / Texture Resolution은 선택 메모(authored)다.** Material Slot Names가 메시 슬롯과 일치하는 파츠는 측정값(`GetPartMeasuredStats`)이 이 메모보다 우선한다. 지금의 placeholder(`TutorialTPP`)는 Material Slot이 1개뿐이라 6개 Part 모두 메시 전체 수치(6,118 삼각형)를 메모로 공유한다 — **이 숫자를 실제 캐릭터 스펙으로 착각하지 않는다.** INSPECTION 패널은 Inspection을 켜면 항상 메시 전체 실측값(삼각형/정점/본/슬롯/LOD/Morph, Skeleton/Physics Asset)과 슬롯별 실측 줄을 보여 주고, 선택 파츠는 슬롯이 일치하면 `Measured: …`(실측), 아니면 이 메모를 `Authored: …`로 구분해 표시한다(6.14절).
- 본 마커는 전용 재질 `M_ViewerBoneMarker`(반투명 0.85, Unlit, **Disable Depth Test**)로 그려 메시 안쪽에 있는 관절(머리·목·척추)도 몸을 통과해 보인다(6.14절 — 처음의 불투명 구체는 Manny의 머리/목 안에 완전히 묻혀 화면에 안 보였다). 재질은 `PortfolioCharacterActor`의 **Bone Marker Material**, 크기는 **Bone Marker Diameter**(기본 12cm)로 조정한다. 마커와 함께 예전의 메시 전체 틴트도 원하면 **Whole Mesh Tint With Bone Markers**를 켠다(기본 꺼짐).

### ⑧ Default Profile / Profile Library 등록

Content Browser에서 `BP_CharacterViewerGameMode`(부모 클래스 `ACharacterViewerGameMode`)를 연다 → **Class Defaults** → `Viewer` 카테고리:
- **Default Profile** — 시작 시 적용할 `CharacterProfileData`
- **Profile Library** — 런타임 CHARACTER 섹션에 노출할 프로필 목록(배열). 여기에 에셋을 추가/교체하는 것만으로 새 캐릭터가 코드 수정 없이 선택 목록에 나타난다.
- **Viewer Widget Class** — 보통 `WBP_CharacterViewer`

현재 값(2026-10-01): **Default Profile = `DA_Character_Manny`**, **Profile Library = [`DA_Character_Manny`, `DA_Character`, `DA_Character_Cube`]**(CHARACTER 섹션 버튼 순서와 같다). 레벨의 `PortfolioCharacter` Actor의 **Profile**도 `DA_Character_Manny`다(⑨) — 두 곳을 같은 프로필로 맞춰 둔다.

### ⑨ 조명·배경·UI 조정과 실행 확인

- 레벨 `LV_Portfolio`에서 `PortfolioCharacterActor`를 배치하고 `Character` 카테고리의 **Profile**에 새 `CharacterProfileData`를 지정한다. 씬에는 이 Actor가 정확히 1개 있어야 한다(0개/2개 이상이면 Controller가 입력을 안전하게 비활성화한다).
- World Settings의 **GameMode Override**(또는 `Config/DefaultEngine.ini`의 `GlobalDefaultGameMode`)가 `BP_CharacterViewerGameMode`를 가리키는지 확인한다.
- **조명·노출·배경(스튜디오 룩, 2026-10-01)**: 레벨 `LV_Portfolio`의 아래 Actor를 World Outliner 라벨로 찾아 Details 패널에서 직접 조정한다. 전부 Movable이라 라이트매스 빌드가 필요 없다. 카메라는 +X 방향을 보고(yaw 0), 캐릭터는 카메라(-X)를 향한다.

  | Actor (Outliner 라벨) | 종류 | 현재 값 | 조정 포인트 |
  | --- | --- | --- | --- |
  | `KeyLight` | Directional Light | Rotation Pitch -40 / Yaw 30(카메라 앞-왼쪽 위), Intensity **2.7 lux**, Light Color (255, 244, 229) 따뜻한 흰색, Cast Shadows 켬, Atmosphere Sun Light 켬(유일), Forward Shading Priority 1 | 주광. 그림자를 드리우는 유일한 라이트 |
  | `FillLight` | Directional Light | Pitch -15 / Yaw -50(카메라 앞-오른쪽, 낮게), **0.9 lux**, (222, 232, 255) 차가운 흰색, Cast Shadows 끔, Atmosphere Sun Light 끔 | 그림자 쪽을 받쳐 주는 보조광 |
  | `RimLight` | Directional Light | Pitch -35 / Yaw -150(캐릭터 뒤-오른쪽 위), **1.8 lux**, (255, 255, 255), Cast Shadows 끔, Atmosphere Sun Light 끔 | 어두운 배경에서 실루엣을 분리하는 역광 |
  | `AmbientSkyLight` | Sky Light | Source Type = Specified Cubemap(`DaylightAmbientCubemap`), Intensity **0.36**(이전 1.0) | 전체 환경광/반사. 올리면 그림자가 옅어지고 대비가 줄어든다 |
  | `StudioPostProcess` | Post Process Volume | **Infinite Extent (Unbound) 켬**, Exposure: Metering Mode **Manual**, Apply Physical Camera Exposure **끔**, Exposure Compensation **0.0**(노출 배율 1.0), Bloom Intensity **0.15**, Vignette Intensity **0.2** | 노출이 고정이므로 화면 전체 밝기는 여기 Exposure Compensation(+1 = 2배) 또는 라이트 lux로 조정 |
  | `StudioBackdrop` | Static Mesh Actor | `/Engine/BasicShapes/Sphere`, Scale 50(반지름 25 m, 캐릭터 중심), Material `MI_StudioBackdrop`, Cast Shadow **끔**, Collision Preset **NoCollision** | 배경색은 MI에서만 바꾼다(Unlit이라 라이트 영향 없음) |
  | `PlatformCylinder` | Static Mesh Actor | `/Engine/BasicShapes/Cylinder`, Location Z -10, Scale (51, 51, 0.2)(윗면 Z 0, 반지름 25.5 m — 가장자리가 배경 구 밖이라 보이지 않음), Material `MI_StudioFloor`, Collision Preset **NoCollision** | 바닥 색/광택은 MI에서 바꾼다 |

  Material Instance 파라미터(`Content/Portfolio/Materials`, MI를 더블클릭해 Details의 `Studio` 그룹에서 바꾼다 — 머티리얼 그래프는 건드리지 않는다):

  | MI | 파라미터 | 현재 값 | 의미 |
  | --- | --- | --- | --- |
  | `MI_StudioBackdrop` | `BottomColor` / `TopColor` | (0.060, 0.063, 0.068) / (0.006, 0.0065, 0.008) Linear | 지평선(바닥 높이) 색 / 위쪽 색. Unlit 값이 그대로 화면 밝기가 된다 |
  | | `GradientBottomZ` / `GradientHeight` | 0 / 1200 cm | 월드 Z 0에서 BottomColor, Z 1200 이상에서 TopColor |
  | | `Brightness` | 1.0 | 배경 전체 밝기 배율 |
  | `MI_StudioFloor` | `BaseColor` / `EdgeColor` | 0.18 회색 / (0.035, 0.035, 0.038) | 캐릭터 주변 바닥 알베도(18% 중간 회색) / 먼 바닥 알베도 |
  | | `FadeStartRadius` / `FadeEndRadius` | 350 / 2400 cm | 바닥 Actor 중심에서 이 거리 사이에 BaseColor→EdgeColor, Specular→0으로 페이드(지평선 경계를 부드럽게) |
  | | `Roughness` / `Specular` | 0.7 / 0.3 | 바닥 광택 |

  - **밝기 기준**: 이 값에서 18% 회색 바닥이 화면에서 중간 회색(sRGB 약 117)으로, Manny의 흰 플라스틱 가슴이 클리핑 없이 보이도록 맞췄다(측정 근거 6.13절). 라이트를 바꾼 뒤에도 흰 재질이 255로 날아가지 않는지 Play/`-game`으로 확인한다.
  - **배경/바닥 메시는 반드시 NoCollision**(또는 Visibility 채널 Ignore)으로 둔다. Inspection 클릭은 `ECC_Visibility` 라인 트레이스로 캐릭터 본을 찾는데, 카메라를 감싸는 배경 구나 바닥이 Visibility를 Block하면 캐릭터보다 먼저 맞아 파츠 클릭이 전부 실패한다. 새 배경 소품을 추가할 때도 같다.
  - 배경 구의 **Cast Shadow는 끈 채로** 둔다(닫힌 구가 그림자를 드리우면 장면 전체가 그늘이 된다). 카메라 프로필의 **Max Distance는 배경 구 반지름 2,500 cm보다 작게** 둔다(현재 최대 Manny Full 917 cm, TutorialTPP 700 cm).
  - `Config/DefaultEngine.ini`의 `r.DefaultFeature.AutoExposure=False`는 그대로 두었고, 레벨 안에서는 `StudioPostProcess`가 우선한다. 이 볼륨을 지우면 노출이 이전 기본값(배율 2.0, 지금보다 1단 밝음)으로 돌아간다.
- `WBP_CharacterViewer`를 열어 스타일을 다듬을 경우 **PanelRoot / NameText / ControlsBox / DescriptionScroll / DescriptionText / ListsScroll / ListsBox** 필수 7개 이름과 각각의 "Is Variable" 체크를 그대로 유지해야 한다. 이 이름이 바뀌거나 사라지면 C++가 해당 위젯을 채우지 못한다(로그에 누락 이름 경고). 8번째 이름 **StatusText**(TextBlock, 촬영 상태 줄)는 **선택**이다 — 없으면 상태 줄만 안 보이고 나머지는 정상 동작한다. **`ListsScroll`의 부모(VerticalBox) 슬롯 Size는 반드시 `Fill`로 유지한다** — `Auto`(또는 슬롯 크기를 만지지 않은 기본값)로 바꾸면 제한된 높이를 잃어 목록이 스크롤되지 않고 화면 밖으로 흘러넘친다.
- **패널 폭 규칙**: `PanelRoot`가 오른쪽 가장자리에 고정된 Canvas 슬롯(앵커 X 최소/최대 = 1)에 있으면 C++가 매 틱 폭을 `clamp(뷰포트 폭 × 24%, 300, 460)` Slate 단위로 맞춘다(`bAutoPanelWidth`, `PanelWidthFraction`/`PanelMinWidth`/`PanelMaxWidth` — WBP의 Class Defaults → `Viewer|Layout`에서 조정). 디자이너가 직접 폭을 정하려면 `bAutoPanelWidth`를 끄거나 앵커를 바꾸면 C++가 손대지 않는다. 버튼 글자(`ButtonFontSize` 13)와 섹션 제목(`HeaderFontSize` 15)도 같은 곳에서 바꾼다. 버튼 글자는 자동 줄바꿈되므로 잘리지 않는다. 설명(Description)은 줄바꿈되며 최소 6줄이 보인 뒤 스크롤된다(`DescriptionSizeBox` 최대 높이).
- **DPI 배율**: `Config/DefaultEngine.ini`의 `[/Script/Engine.UserInterfaceSettings]`에서 `UIScaleRule=ShortestSide`, 짧은 변 기준 720 → 0.8, 1080 → 1.0, 1440 → 1.25(사이 값은 선형). 엔진 기본값(720p = 0.666)일 때 패널이 약 213px로 줄어 글자가 잘렸다. 결과적으로 패널은 1280×720에서 384 Slate 단위(약 307px), 1920×1080에서 460px이다. 이 곡선은 프로젝트의 모든 UMG에 적용된다.
- 1.2~1.3절의 방법으로 실행해 캐릭터가 보이는지, 우측 패널에 새 캐릭터 이름/목록이 뜨는지 확인한다.

### 2.10 프로필 검증 도구

새 `CharacterProfileData`를 만들었거나 고쳤으면 실행 전에 검증 스크립트를 돌린다. 메시와 프로필을 대조해 "무엇이, 어디서(필드), 어떻게 고치는지"를 한국어로 출력한다. **읽기 전용**이라 에셋을 저장하거나 바꾸지 않는다. Editor를 닫은 상태에서 실행한다(열려 있으면 같은 프로젝트를 두 번 여는 것이 된다).

```powershell
# 기본: /Game/Portfolio/Data 아래 모든 프로필. Error가 하나라도 있으면 종료 코드가 0이 아니다.
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "<프로젝트 절대경로>\CharacterShowcase.uproject" -run=pythonscript "-script=<프로젝트 절대경로>\Scripts\ValidateProfiles.py" -unattended -nosplash -nop4 -NullRHI -log
# 특정 프로필(또는 폴더)만: -ProfilePath= 를 추가(여러 개면 반복하거나 쉼표로 구분)
... -ProfilePath=/Game/Portfolio/Data/DA_Character_Manny
```

- `-ExecutePythonScript="<절대경로>\Scripts\ValidateProfiles.py"`(1.6절 스크립트와 같은 방식)로도 같은 보고서가 나오지만, 이 방식은 Error가 있어도 프로세스 종료 코드가 0이다(로그 끝에 `Python script executed with errors`). 종료 코드로 판단하려면 위의 `-run=pythonscript`를 쓴다.
- Editor 안에서 바로 보려면 Output Log의 Python 입력줄에 `print(unreal.CharacterProfileValidator.format_report(unreal.CharacterProfileValidator.validate_profile(unreal.load_asset('/Game/Portfolio/Data/DA_Character_Manny'))))`.
- Viewer 실행 중에도 시작 프로필(`PostLogin`)과 CHARACTER 전환(`SwitchProfile`) 때마다 같은 보고서가 로그(`LogTemp`, `[ProfileValidator]`)에 찍힌다. 경고/오류 등급이 아닌 일반 로그로만 남기므로 실행이나 자동화 테스트를 멈추지 않는다.

**출력 읽는 법**(로그의 `LogPython:` 줄):

```
[ValidateProfiles] /Game/Portfolio/Data/DA_Character_Cube: E=0 W=2 I=0
[ValidateProfiles]   [Warning][Mesh] Skeletal Mesh 'SkeletalCube'에 Physics Asset이 없어 파츠 1개를 클릭으로 선택할 수 없습니다... (SkeletalMesh.PhysicsAsset)
[ValidateProfiles] TOTAL profiles=3 E=0 W=2 I=0 load_failures=0 RESULT=PASS
```

- `E/W/I` = Error / Warning / Info 개수. **Error**는 기능이 실제로 안 되는 것(버튼이 아무 일도 안 함, 애니메이션 거부, 슬롯이 안 바뀜) — 포트폴리오에 쓰기 전에 반드시 고친다. **Warning**은 동작은 하지만 의도와 다를 가능성이 큰 것(대체값 사용, 파츠 클릭 불가, 이상한 값). **Info**는 참고(기능이 꺼져 있음, 큰 텍스처).
- `[Mesh]` 등은 분류(Mesh/Camera/Animation/Expression/Material/Part/Play), 마지막 괄호는 Details 패널의 위치다. 예: `Parts[2].BoneNames[0]` = Parts 배열 3번째 행의 Bone Names 첫 칸(배열 번호는 0부터).
- 본/슬롯/Morph/프리셋/애니메이션 Id 이름 비교는 엔진과 같이 **대소문자를 구분하지 않는다**(`Head` = `head`). 밑줄·좌우 표기 차이(`spine01`, `hand`, `upperarm_left`)는 오류로 잡고 메시에 있는 가장 가까운 이름을 제안한다.

| 검사 항목 | 심각도 | 고치는 방법 |
| --- | --- | --- |
| Skeletal Mesh가 비어 있음 | Error | Character → Skeletal Mesh 지정. 비어 있으면 메시를 쓰는 검사(본·슬롯·Morph·텍스처)는 건너뛴다 |
| 메시에 Skeleton이 없음 | Error | Skeleton을 만들거나 지정해 다시 Import |
| 메시에 Physics Asset이 없음 | Warning(Parts가 있을 때, 파츠 클릭 불가) / Info(Parts가 비었을 때) | 메시 우클릭 → Create → Physics Asset, 메시 에디터의 Physics Asset 칸에 지정 |
| 메시 높이가 50 cm 미만 또는 300 cm 초과 (Editor에서만 검사) | Warning "스케일 확인: 높이 N cm" | FBX Import Uniform Scale(원본이 m 단위면 100) 확인 후 다시 Import. 의도한 크기면 무시 |
| Display Name이 비어 있음 | Warning | Character → Display Name 입력 |
| Camera Presets의 Id가 비어 있음 / 중복 | Warning | 고유한 Id 입력(중복이면 첫 번째만 선택됨) |
| Default Preset Id가 Camera Presets에 없음 | Warning | 있는 Id 입력(메시지에 목록). 없으면 Reset(R)이 Default Framing으로 감 |
| Min Distance ≥ Max Distance, Min Pitch ≥ Max Pitch, FOV가 10~120 밖 (Default Framing과 모든 프리셋) | Error | Min < Max로, FOV는 보통 30~60 |
| Animations의 Id가 비어 있음 / 중복 | Error | 고유한 Id 입력(중복이면 두 번째 행은 재생되지 않음) |
| Animations의 Sequence가 비어 있음 | Error | Sequence 지정 또는 행 삭제 |
| Sequence의 Skeleton이 메시와 호환되지 않음(`IsAnimationSkeletonCompatible`, 2절 ③·5절과 같은 규칙) | Error | 같은 Skeleton의 애니메이션을 쓰거나 IK Retargeter로 리타겟한 Sequence 지정 |
| Is Pose인데 Pose Time이 0~Sequence 길이 밖 | Warning | 범위 안의 초 입력 |
| Default Animation Id가 Animations에 없음 | Error | 있는 Id 입력 또는 비우기 |
| Default Anim Class와 Default Animation Id가 둘 다 있음 | Info | AnimBP가 우선, Id는 안 쓰임. 의도가 아니면 하나 비우기 |
| Expression의 Morph Name이 메시에 없음 | Error(메시에 있는 Morph 이름 최대 10개를 함께 표시) | 목록에 있는 이름으로 수정 |
| 메시에 Morph Target이 0개인데 Morph를 쓰는 Expression이 있음 | Warning(프로필당 1건, 위 Error 대신) | Morph Target이 포함된 메시를 Import(Import Morph Targets 체크)하거나 Morphs 비우기 |
| Material Variant 슬롯: Slot Name이 메시에 없음 | Error(Slot Index도 유효하지 않을 때) / Warning(Slot Index로 대신 적용될 때) | 메시 슬롯 이름으로 수정(메시지에 `이름(번호)` 목록) |
| Material Variant 슬롯: Slot Name 없이 Slot Index가 범위 밖 | Error | Slot Name으로 지정 권장 |
| Material Variant 슬롯: Slot Name과 Slot Index가 모두 비어 있음 | Warning | Slot Name 입력 |
| Material Variant 슬롯: Material이 비어 있음 | Warning | Material 지정 또는 행 삭제 |
| 메시 재질이 쓰는 텍스처 크기가 2의 거듭제곱이 아님 | Warning | 1024·2048처럼 2의 거듭제곱으로 다시 저장해 Import |
| 메시 재질이 쓰는 텍스처가 4096보다 큼 | Info | 필요하면 텍스처 에디터의 Maximum Texture Size로 축소 |
| Wireframe Material이 비어 있음 | Info | `/Game/Portfolio/Materials/M_Wireframe` 지정(비우면 W 버튼 비활성) |
| Parts의 Id가 비어 있음 / 중복 | Warning | 고유한 Id 입력 |
| Bone Names의 본이 메시 Reference Skeleton에 없음 | Error(비슷한 본 이름을 제안: 밑줄·`left/right`·`L_` 접두사 차이, 좌우 접미사 누락 `hand` → `hand_l` 또는 `hand_r`) | Skeleton Tree의 이름으로 수정 |
| Material Slot Names의 슬롯이 메시에 없음 | Error | 메시 슬롯 이름으로 수정 |
| Bone Names·Material Slot Names·Component Tag가 모두 비어 있는 파츠 | Warning | 파츠를 이루는 본 이름 입력 |
| 같은 본이 두 파츠에 들어 있음 | Warning | 한쪽에서 삭제(클릭 시 항상 앞쪽 파츠가 선택됨) |
| Walk Speed ≥ Run Speed | Warning | Walk < Run(예: 300 / 600) |

검사 코드는 `Source/CharacterShowcase/Character/CharacterProfileValidator.h/.cpp`(`UCharacterProfileValidator`, Blueprint/Python에서도 호출 가능), 테스트는 `Tests/CharacterProfileValidatorTests.cpp`다. 새 검사를 추가하면 이 표도 같이 고친다.

## 3. 설계 규칙

| 책임 | 소유 클래스 | 원칙 |
| --- | --- | --- |
| 시작 설정 | `CharacterViewerGameMode` | Default Profile/Profile Library, Pawn/Controller/Widget 연결 |
| 입력·UI 연결 | `CharacterViewerController` | 입력 상태, UI 표시, 드래그/클릭 판정, 기능 호출 |
| 카메라 | `CharacterViewerCameraPawn` | Orbit 중심/각도, Zoom clamp, 프리셋 보간/Reset |
| 캐릭터 상태 | `PortfolioCharacterActor` | Turntable, 표정, Animation, Variant, 선택 파츠 적용/복원 |
| 화면 표현 | `CharacterViewerWidget` + `WBP_CharacterViewer` | 데이터 목록 표시, 이벤트 전달, 상태 표시 |
| 아트 표현 | Level/Blueprint/Material Instance | 조명, 배경, 플랫폼, 스타일 |

- 별도 Manager/Subsystem은 두지 않는다. 상태를 UI와 Actor에 중복 저장하지 않는다.
- `CharacterProfileData`는 설정(구성) 데이터다. 현재 선택/Turntable 회전/재생 시간/카메라 값 같은 실행 상태를 Asset에 저장하지 않는다.
- 캐릭터 교체 시 이전 선택·Morph·재질 override·Animation·Wireframe·선택 파츠를 먼저 정리한 뒤 새 프로필을 적용한다(`ClearRuntimeState()`).
- UMG 위에서 시작한 입력은 Orbit/Zoom/Inspection으로 전달하지 않는다(`IsPointerOverPanel()`). 드래그 종료·앱 포커스 상실 시 캡처/버튼 상태를 해제한다.
- Clean View(H)는 UI/선택 강조/커서를 숨기되 Orbit/Zoom/Space/H는 유지한다. Wireframe은 기본 Overlay 모드에서 셰이딩·Variant 위에 선만 겹치고(슬롯 불변), 레거시 슬롯 교체 모드(`bWireframeReplacesSlots`)에서만 Variant보다 우선한다(끄면 현재 Variant로 정확히 복원).

## 4. 개발 검증 결과

| 항목 | 날짜 | 결과 | 근거 경로 |
| --- | --- | --- | --- |
| Editor 빌드 (`CharacterShowcaseEditor Win64 Development`) | 2026-09-29(최종, 이전 여러 차례 반복) | 성공, 종료 코드 0, 오류 0 / 경고 0 | 빌드 로그(6.5~6.9절 각 회차) |
| Game 빌드 (`CharacterShowcase Win64 Development`) | 2026-09-28 | 성공, 종료 코드 0, 오류 0 / 경고 0(2~3회 재현) | 6.7/6.8절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-09-29 | **6/6 통과** | `Saved/Automation/EditorC1/index.json` |
| Win64 Development 패키지 빌드 (`RunUAT BuildCookRun`) | 2026-09-28 | BUILD SUCCESSFUL, 종료 코드 0 | 6.9절 |
| Win64 Development 패키지 스모크(`CharacterShowcase.exe -ExecCmds=...`) | 2026-09-28 | **1/1 통과, 오류 0, 경고 0** | `Saved/Automation/PackagedP2b/index.json` |
| Win64 Shipping 패키지 빌드 (`RunUAT BuildCookRun -clientconfig=Shipping`) | 2026-09-28 | BUILD SUCCESSFUL, 종료 코드 0, 94.0초 | `Saved/Packaged/Windows/CharacterShowcase/Binaries/Win64/CharacterShowcase-Win64-Shipping.exe` |
| Shipping 프로세스 기동/종료 확인 (자동화 테스트 없음) | 2026-09-28 | 실행 30초 후 프로세스 생존 확인, 정상 종료 확인 | 6.9절 |
| 스크린샷 육안 확인(패키지, Development) | 2026-09-28 | UI/Clean/Inspect/Wireframe/Profile1/Profile2 전부 의도대로 렌더링(마젠타 강조·청록 Wireframe·CHARACTER 섹션 2버튼 포함) | `Saved/Packaged/Windows/CharacterShowcase/Saved/Screenshots/Windows/` |
| `CreatePortfolioAssets.py` 기존 자산 보존 재검증 | 2026-09-29 | 임시 복사본에서 2회 재실행 후 자산 7개 SHA-256 전부 동일(수동 편집 마커 유지), 누락 자산만 재생성됨. 실제 프로젝트에서 1회 실행해 `[keep] ... OK` ×7, `git status` 무변경 확인 | 6.6절 |
| `CreateViewerWidgetLayout.py` idempotent 확인 | 2026-09-29 | 1차: 트리 생성(해시 변경). 2차(즉시 재실행): `[keep] ... not modified.`, 해시 1차와 완전 동일 | 6.8절 |
| `-game` 스모크(`CharacterShowcase.Game.ViewerSmoke`), P0~P2 핵심 assertion | 2026-09-28 | 1/1 통과, 오류 0, 경고 0 | `Saved/Automation/GameP2b/index.json` |
| `-game` 스모크, 패널 위/밖 휠·드래그 경계 테스트(신규) | 2026-09-29 | 실패(3건) — 게임 창이 비활성/가려진 상태였음. 원인은 엔진 동작으로 확정(6.10절). 테스트에 전제 조건 검사 추가 | 6.10절 |
| `-game` 스모크 재실행(활성 전면 창, 커서 조작 없는 데스크톱) | 2026-09-30 | **1/1 통과, 오류 0, 경고 0.** 이전 실패 3건(패널 hover, 패널 위 휠 차단, 캔버스 드래그 Orbit) 전부 통과. 창 상태 로그 `visible=1 minimized=0 pos=(0,0)`, 패널 rect (1067,0)-(1280,720) | `Docs/Evidence/2026-09-30-game-smoke-index.json`, `Saved/Automation/GameFinal/index.json` |
| Win64 Development 패키지 재빌드 (커밋 ca08244 소스) | 2026-09-29 | BUILD SUCCESSFUL, 종료 코드 0, 오류 0/경고 0, 16초. 09-30에 실제 입력 재생으로 기동·조작 확인(키 홀드 실험 포함) | `Saved/Packaged/Windows/CharacterShowcase/Binaries/Win64/CharacterShowcase.exe` |
| Win64 Shipping 패키지 재빌드 (커밋 ca08244 소스) | 2026-09-29 | BUILD SUCCESSFUL, 종료 코드 0, 오류 0/경고 0, 33초 |
| **Shipping 실제 입력·시각 검증** (`ViewerInputDriver.ps1`이 user32 SendInput으로 실제 마우스/키보드 이벤트를 게임 창에 전달, 단계마다 창 캡처) | 2026-09-30 | 실행 2회, 캡처 25장 육안 확인: ① 드래그 Orbit 회전 ② 휠 4틱 Zoom out ③ R Reset으로 Full Body 복귀 ④ Space Turntable On 후 회전, 짧은 드래그로 Off 정지(2.5초 간격 2장 동일) ⑤ H로 패널 숨김, 숨긴 상태에서 드래그 Orbit 동작, H로 복원 ⑥ I On, 몸통 클릭 시 마젠타 강조 전신 적용, 빈 공간 클릭 시 해제 ⑦ W On 청록 Wireframe, W Off 기본 재질 복원 ⑧ 패널 위 휠 6틱: Zoom 없음, 패널 목록만 스크롤(설명 영역 스크롤 확인). 캔버스 휠 대조군은 Zoom 됨 ⑨ 패널에서 시작한 드래그: Orbit 없음 ⑩ 마우스 누른 채 포커스 상실 후 복귀·이동: Orbit 없음, 버튼 잔류 없음 | `Docs/Evidence/2026-09-30-shipping-real-input/` (PNG 25장 + 드라이버 + 단계 JSON) | `Saved/PackagedShipping/Windows/CharacterShowcase/Binaries/Win64/CharacterShowcase-Win64-Shipping.exe` |
| Editor 빌드 (파츠 단위 강조/실측 수치 변경, 6.11절) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0(최종 증분 빌드 4개 액션) | 6.11절 |
| Game 빌드 (같은 변경) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0(27개 액션, 93초) | 6.11절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-10-01 | **11/11 통과**, 실패 0(신규 `Viewer.PartHighlightMaterialSlots`/`PartHighlightBoneMarkers`/`MeshStats`/`AnimationSkeletonCompatibility`) | `Saved/Automation/A/index.json`(작업 worktree) |
| `CreatePortfolioAssets.py` (`M_ViewerPartHighlight` 추가) | 2026-10-01 | 1차: `Created M_ViewerPartHighlight` + 기존 7개 `[keep] ... OK`, `DONE, NO ERRORS`, 종료 코드 0. 2차: `[keep] ... OK` ×8, 종료 코드 0. 기존 `.uasset` 변경 없음(`git status`) | 6.11절 |
| Editor 빌드 (패널 레이아웃·촬영 패스) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0 (1차는 `const UGameViewportClient::GetWindow()` 컴파일 오류 1건 → 수정 후 재빌드) | 6.12절 |
| Game 빌드 (패널 레이아웃·촬영 패스) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0, 30개 액션, 74.9초 | 6.12절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-10-01 | **10/10 통과**(기존 7 + `Viewer.PanelLayout`/`Viewer.CaptureNaming`/`Viewer.CaptureSequence`), 오류 0 / 경고 0. PanelLayout 로그: 1280×720에서 384 Slate 단위 = 307px, 1920×1080에서 460px | `Saved/Automation/B/index.json` |
| `WBP_CharacterViewer` 재생성 (`CreateViewerWidgetLayout.py` 2회) | 2026-10-01 | 에셋 삭제 후 1차: `[create]` + `[build] ... compiled, and saved.`(SHA-256 `1bacf567…` → `4c49e99a…`). 2차: `[keep] ... All 7 required widgets present ... Optional StatusText present.`, 해시 1차와 동일, 로드 오류 없음 | 6.12절 |
| `-game` `CharacterShowcase.Game.ViewerCapture` (1280×720 / 1920×1080) | — | **미실행**(NullRHI에서 실행 불가, 4.1절) | — |
| `CreatePortfolioAssets.py` 스튜디오/Manny 추가 후 1차 실행(`UnrealEditor-Cmd -ExecutePythonScript ... -NullRHI`) | 2026-10-01 | 종료 코드 0, `==== DONE, NO ERRORS ====`. **신규 생성 5개**(`M_StudioBackdrop`, `MI_StudioBackdrop`, `M_StudioFloor`, `MI_StudioFloor`, `DA_Character_Manny`) + 기존 7개 `[keep] ... OK` ×7 + `DefaultEngine.ini` OK | 6.13절 |
| 1회성 in-place 편집(`LV_Portfolio` 스튜디오 Actor·Profile, GameMode 기본 프로필, MI 값 조정) | 2026-10-01 | 종료 코드 0, 저장 후 다시 읽은 값이 2절 ⑧/⑨ 표와 일치 | 6.13절 |
| `CreatePortfolioAssets.py` 2차·3차 실행(편집 후) | 2026-10-01 | 둘 다 종료 코드 0, **`[keep] ... OK` ×12** + ini OK, DIFFERS 0. 실행 전후 에셋 해시 2차 11/11, 3차 12/12 동일(아무것도 다시 저장하지 않음) | 6.13절 |
| `-game` 스모크(`CharacterShowcase.Game.ViewerSmoke`), Manny 기본 프로필 + 스튜디오 룩 | 2026-10-01 | 2회 실행, 둘 다 **Fail, 오류 1건(같은 내용)**: 합성 포인터 전제 조건 `IsActive()=1, IsCursorDirectlyOverSlateWindow()=0` — 실행 중 OS 커서가 게임 창 밖에 있었음(6.10절의 환경 조건, 커서를 움직이지 않는 규칙으로 실행). 그 외 assertion 오류 0, 경고 0 — Manny에서 Torso 클릭(액터+120 cm 지점 → `Torso`), 빈 공간 클릭 해제, Grid/Default 재질, Idle/Pose, Wireframe 토글, 프로필 전환(Cube → `DA_Character`) 포함. 스크린샷 6장 성공 | `Saved/Automation/AgentC/index.json`, `Saved/Automation/AgentC2/index.json`, `Saved/Screenshots/WindowsEditor/ViewerSmoke_*.png` |
| 스튜디오 룩 화면 측정(위 스모크 2차 `ViewerSmoke_UI.png`, 1280×720) | 2026-10-01 | 바닥(캐릭터 앞) sRGB 평균 120(18% 회색 목표 118), Manny 가슴 영역 27,200픽셀 중 ≥250 **0개**, 캐릭터 전체 136,500픽셀 중 ≥250 6개(스페큘러 반짝임), 배경 지평선 48~53 / 위쪽 25~29, 먼 바닥 71 | 6.13절 |
| Editor 빌드 (프로필 검증 도구, 6.15절) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0 (10개 액션, 메시지 문구 수정 후 증분 4개 액션도 0/0) | 6.15절 |
| Game 빌드 (같은 변경) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0 (16개 액션, `CharacterProfileValidator.cpp` 포함, 118.6초) | 6.15절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-10-01 | **18/18 통과**, 실패 0, 테스트 오류/경고 0 (신규 `Validator.NullAndEmpty`/`DeliberateMistakes`/`MorphsTexturesSkeleton`/`ContentProfiles`) | `Saved/Automation/E/index.json`(작업 worktree) |
| `ValidateProfiles.py` (`-run=pythonscript`, 기본 경로) | 2026-10-01 | 종료 코드 0. `DA_Character` E=0 W=0 I=0, `DA_Character_Cube` E=0 W=2 I=0(Physics Asset 없음, 높이 25 cm), `DA_Character_Manny` E=0 W=0 I=0, `TOTAL profiles=3 E=0 W=2 I=0 RESULT=PASS` | 6.15절 |
| `ValidateProfiles.py` 실패 경로·다른 실행 방식 | 2026-10-01 | `-ProfilePath=/Game/Portfolio/Data/DoesNotExist` → `RuntimeError`, 종료 코드 **-1**. `-ExecutePythonScript="... -ProfilePath=/Game/Portfolio/Data/DA_Character_Manny"` → `profiles=1 E=0 W=0 I=0 RESULT=PASS`, 종료 코드 0 | 6.15절 |
| Editor 빌드 (INSPECTION 실측·Shaded Wireframe·설명 줄 맞춤, 6.14절) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0(최종 증분 빌드). 개발 중 컴파일 오류 2회(C4458 `Slot` 이름 가림, 테스트 `Printf` 형식 불일치 — `FSlateFontInfo::Size`가 float) 각각 수정 | 6.14절 |
| Game 빌드 (같은 변경) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0, 80.9초 | 6.14절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-10-01 | **16/16 통과**, 실패 0 / 경고 0(신규 `Viewer.WireframeOverlay`, `Viewer.InspectionPanelText`; `Viewer.PanelLayout`에 6줄 맞춤 규칙 추가; `Viewer.WireframeRestore`/`PartHighlightMaterialSlots`는 레거시 슬롯 교체 경로 검증으로 전환) | `Saved/Automation/D/index.json`(작업 worktree) |
| `CreatePortfolioAssets.py` (`M_WireframeOverlay` 추가) | 2026-10-01 | 1차: `Created M_WireframeOverlay` + 기존 13개 `[keep] ... OK` + ini OK, `DONE, NO ERRORS`, 종료 코드 0. 2차: `[keep] ... OK` ×14 + ini OK, 종료 코드 0, 실행 전후 `Content/Portfolio` 14개 파일 SHA-256 동일 | 6.14절 |
| `M_WireframeOverlay` 실제 RHI 컴파일 확인(`MaterialEditingLibrary.get_statistics`, D3D12 SM5, NullRHI 아님, 읽기 전용) | 2026-10-01 | 컴파일 성공, VS 234 / PS 67 명령(`M_Wireframe` VS 220 / PS 67) | 6.14절 |
| `WBP_CharacterViewer` 재생성 (`CreateViewerWidgetLayout.py` 2회, 설명 높이 134 → 126) | 2026-10-01 | 에셋 삭제 후 1차: `[create]` + `[build] ... compiled, and saved.`(SHA-256 `4c49e99a…` → `44b6422a…`). 2차: `[keep] ... All 7 required widgets present ... Optional StatusText present.`, 해시 동일 | 6.14절 |
| `-game` `CharacterShowcase.Game.ViewerCapture` 1280×720 (Fable, 커밋 ee937b5 병합본) | 2026-10-01 | 1/1 통과. 캡처 육안: 본 마커 3개(`head, neck_01, neck_02`)가 머리/목 안에 묻혀 **안 보임**, Wireframe Overlay(불투명)는 **단색 청록 실루엣**, INSPECTION의 선택 파츠 줄이 목록 끝까지 스크롤되지 않아 **안 보임**(목록 스크롤이 새 행의 줄바꿈 높이가 정해지기 전에 끝 위치를 계산). 설명 박스는 정확히 6줄(7번째 줄 없음; 6번째 줄 아래의 어두운 띠는 스크롤 박스의 "더 있음" 그림자) | `Saved/Screenshots/ViewerCapture/ViewerCapture_*_1280x720.png`(main) |
| 리뷰 수정 Editor / Game 빌드 (`M_ViewerBoneMarker`, 반투명 Wireframe, 목록 자동 스크롤) | 2026-10-01 | 둘 다 성공, 오류 0 / 경고 0 (Game 51.8초) | 6.14절 |
| 리뷰 수정 Editor Automation (NullRHI) | 2026-10-01 | **16/16 통과**, 실패 0 / 경고 0 (`PartHighlightBoneMarkers`에 마커 재질·12 cm 확인, `WireframeOverlay`에 반투명·깊이 테스트 유지 확인, `PanelLayout`에 한글 설명 줄 맞춤 추가) | `Saved/Automation/D/index.json`(작업 worktree) |
| `CreatePortfolioAssets.py` (`M_WireframeOverlay` 재생성 + `M_ViewerBoneMarker` 추가) | 2026-10-01 | 이전 `M_WireframeOverlay.uasset`(이 스크립트가 만든 것, main에는 아직 없음)을 삭제 후 1차: `Created M_WireframeOverlay`, `Created M_ViewerBoneMarker` + 기존 13개 `[keep] OK`, `DONE, NO ERRORS`. 2차: **`[keep] ... OK` ×15** + ini OK, 실행 전후 15개 파일 SHA-256 동일 | 6.14절 |
| 새 재질 실제 RHI 컴파일(`get_statistics`, D3D12 SM5) | 2026-10-01 | `M_WireframeOverlay`(반투명) VS 452 / PS 116, `M_ViewerBoneMarker` VS 354 / PS 116, 오류 없음 | 6.14절 |
| Editor 빌드 (재생 컨트롤·LOD·배경 프리셋, 6.18절) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0 (본 코드 10개 액션, 테스트 파일 추가 4개 액션) | 6.18절 |
| Game 빌드 (같은 변경) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0, 17개 액션(첫 Game 빌드라 공유 PCH 포함), 85.3초 | 6.18절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-10-01 | **24/24 통과**, 실패 0 / 경고 0 (신규 `Viewer.PlaybackMath`/`AnimationPlayback`/`ForcedLOD`/`BackdropPresets`) | `Saved/Automation/G/index.json`, 최종 재실행 `Saved/Automation/G2/index.json`(작업 worktree, 둘 다 24/24) |
| `LV_Portfolio` 배경/바닥 재질 확인(읽기 전용 Python, 저장 안 함) | 2026-10-01 | `PlatformCylinder`(`StaticMeshActor_0`) slot 0 = `MI_StudioFloor` → `M_StudioFloor`, `StudioBackdrop`(`StaticMeshActor_1`) slot 0 = `MI_StudioBackdrop` → `M_StudioBackdrop`. `git status` 무변경 | 6.18절 |

| Editor 빌드 (일괄 촬영·턴테이블 영상, 6.19절) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0 (새 worktree 13개 액션 88.8초, 테스트 수정 후 증분 4개 액션 4.6초) | 6.19절 |
| Game 빌드 (같은 변경) | 2026-10-01 | 성공, 종료 코드 0, 오류 0 / 경고 0 (17개 액션 85.2초, 최종 소스 증분 재빌드 3개 액션 12.5초도 0/0) | 6.19절 |
| Editor Automation (`Automation RunTests CharacterShowcase`, NullRHI) | 2026-10-01 | **22/22 통과**, 실패 0 / 경고 0 (신규 `Viewer.BatchCapturePlan`, `Viewer.BatchCaptureRunner`). 1차 실행은 21/22 — Runner 테스트가 일부러 만든 실패 시나리오의 `LogTemp: Error` 줄을 자동화가 오류로 집계 → `AddExpectedError`로 선언한 뒤 통과 | `Saved/Automation/H/index.json`(작업 worktree) |
| `MakeTurntableVideo.bat` 실제 실행 (ffmpeg 9.0.1 full_build, gyan.dev — winget 사용자 설치로 이미 있었음) | 2026-10-01 | `Turntable_DA_Character_Manny_20261001-121743`(2프레임, 1280×720) → `turntable.mp4` 32,330 B, h264 yuv420p 1280×720 12fps 2프레임 0.167초 / `--gif` → `turntable.gif` 146,972 B 640×360 2프레임, 팔레트 임시 파일 삭제, 종료 코드 0. ffmpeg를 못 찾게 한 실행 → 설치 명령 출력 + 종료 코드 2, `--fps 0` → 종료 코드 1 | 6.19절 |
| `CaptureAll.bat --check` / `MakeTurntableVideo.bat --check` | 2026-10-01 | 종료 코드 0, 출력은 6.19절. 가짜 `UE_ROOT`(인자를 받아 `Batch_*` 폴더에 PNG 2개를 만드는 콘솔 exe)로 실행 경로 2회: `New folders: 1, new files: 2`, 두 번째 실행은 첫 폴더를 다시 세지 않음, 로그의 `[CharacterViewerBatch]` 줄 출력 | 6.19절 |
| `-game` `CharacterShowcase.Game.ViewerBatchCapture`, `CaptureAll.bat` 실제 실행 | — | **미실행**(게임 창이 필요, 4.1절) | — |
| `-game` 시각 검증 체인 (Fable, main 9127280, Editor 바이너리, DPI/프레임레이트 게이트 1 FPS) | 2026-10-01 | `ViewerCapture` 1280×720 **1/1**(F12 HighResScreenshot 3.4초 → 2560×1440; Inspect 캡처: 본 마커 3개 `head/neck_01/neck_02` 투과 표시, ListsScroll 595.2/595.2 끝까지, 선택 파츠 블록 표시; Wireframe 캡처: Overlay 모드, 반투명 선 위로 셰이딩 비침), `ViewerCapture` 1920×1080 **1/1**(F12 5.4초), `ViewerBatchCapture` **1/1**(경고 1 = 중복 시작 거부 로그, 6장 2560×1440 `Batch_DA_Character_Manny_*`), `ViewerSmoke` **1/1** 오류 0(커서 전면 배치 후 50초), `Demo.MovementSmoke` **1/1** 오류 0 | `Saved/Automation/{V720,V1080,VBatch,VSmoke,VDemo3}/index.json`, `Saved/Screenshots/ViewerCapture/*.png`, `Saved/Screenshots/Portfolio/Batch_DA_Character_Manny_20261001-142009/` |
| `Tools\PackageViewer.bat --no-pause` 실제 실행 (Fable, main 9127280) | 2026-10-01 | RunUAT BuildCookRun Development **BUILD SUCCESSFUL**, ExitCode 0, zip 생성 `Saved\Packaged\CharacterShowcase-Win64-20261001.zip` 311,683,973 bytes | `.../scratchpad/package_bat.log`(세션 임시), zip 파일 |

### 4.1 미검증·대기 항목

- **2026-10-01 저녁 시각 검증 완료(4절 표 참조)**: 파츠 단위 강조(본 마커 3개가 머리 위로 투과 표시), 반투명 Shaded Wireframe, INSPECTION 실측 수치·선택 파츠 블록, 패널 레이아웃(720p/1080p), F12 고해상도(2560×1440), 일괄 촬영 6장, ViewerSmoke 1/1, 플레이 데모 MovementSmoke 1/1을 `-game`에서 확인했다. 남은 시각 확인은 아래 항목뿐이다.
- **본 마커 3개가 겹쳐 하나의 덩어리로 보임(다듬기 후보)**: 머리 파츠의 `head`/`neck_01`/`neck_02` 12 cm 구체가 겹쳐 마젠타 타원 하나로 보인다. 기능상 "선택 = 머리"는 전달되지만, 지름 8 cm·불투명도 0.6 정도로 줄이면 관절별로 구분된다(`BoneMarkerDiameter`, `M_ViewerBoneMarker` Opacity).
- **LOD0 Wireframe은 여전히 조밀**: 92k 삼각형 Manny는 720p에서 반투명 청록으로 덮인다(셰이딩은 비침). 토폴로지 확인은 L키로 LOD1(27k)/LOD2(13k)를 보는 것을 권장(6.18절). 실제 캐릭터(2~5만 삼각형)는 LOD0에서도 읽힐 것으로 예상하나 미확인.
- **사람이 손으로 직접 조작한 확인**: 2026-09-30 검증은 OS 수준 실제 입력 이벤트(SendInput)를 자동 재생한 것이다. 게임 입장에서는 실제 마우스/키보드와 구분되지 않지만, 사람이 손으로 조작한 기록은 아직 없다. 패널 버튼을 마우스로 직접 클릭하는 경로는 키보드 단축키 경로로만 확인했다(합성 Slate 클릭 테스트는 `-game` 스모크에서 통과). **대기(선택)**.
- **Expression(표정)의 실제 시각 검증**: `TutorialTPP`/`SkeletalCube` 모두 Morph Target이 없다. Expression 스키마/무충돌만 확인됐고, 실제 Morph 적용 후 얼굴이 바뀌는 모습은 **미검증**이며 Morph가 있는 메시가 들어오기 전까지는 검증할 수 없다.
- **사람이 만든 디자이너 WBP 레이아웃의 hover 동작**: 지금 존재하는 `WBP_CharacterViewer` 트리는 C++ 에디터 툴이 자동 생성한 것이다. 아티스트가 디자이너에서 직접 커스터마이즈한 레이아웃에서 `IsPointerOverPanel()`/패널 클릭 소비가 그대로 동작하는지는 **미검증**.
- **새 레벨 생성 경로**: `LV_Portfolio`가 없을 때 스크립트가 만드는 경로(`create_or_update_level()` + `apply_studio_setup()`)는 실행하지 않았다. `apply_studio_setup()` 자체는 기존 레벨 편집에서 실행·검증됐다.
- **사용자 GUI PIE**: Editor 툴바의 Play 버튼을 사람이 직접 눌러 확인한 기록이 없다(전부 `-game`/패키지/Automation으로 대체 검증).
- **재생 컨트롤·LOD·배경 프리셋의 화면 확인(6.18절)**: 상태 값(일시정지/프레임/속도, 강제 LOD와 그 LOD의 수치, MI 파라미터 값)은 Editor Automation(NullRHI)으로 검증했다. **실제 화면에서** 일시정지·프레임 이동 시 포즈가 그 프레임으로 바뀌는지, 강제 LOD에서 메시가 실제로 거칠어지는지, Black/White/Mid Grey 배경·바닥 색이 보이는지(White 0.8이 날아가지 않는지), PLAYBACK 버튼 2줄과 `◀`/`▶` 글자가 300~460 Slate 폭에서 잘리지 않는지, 실제 키(P, [, ], -, =, 0, L, B) 입력은 **미검증**이다. `LV_Portfolio`의 배경/바닥 Actor가 `MI_StudioBackdrop`/`MI_StudioFloor`를 쓰는지는 레벨을 읽기 전용으로 열어 확인했다(6.18절).
- **프로필 검증 도구(2.10절)의 런타임 로그**: 2026-10-01 저녁 `-game` ViewerSmoke 로그에 `[ProfileValidator]` 보고 5회(PostLogin 1 + SwitchProfile) 기록됨 — 확인 완료. 화면 상태 줄 표시는 6.20절(진행 중).

## 5. 남은 작업·위험

- `ApplyProfile` 순서(`PostLogin`이 `BeginPlay`보다 먼저 실행됨)에 맞춘 방어 코드는 정적으로만 점검했고, 이 UE 버전의 PIE로 최종 확인하지 않았다.
- `SetAnimation`의 스켈레톤 호환성 검사(2026-10-01, 6.11절)는 같은 Skeleton, 양방향 Compatible Skeletons 목록, 본 계층 일치(`USkeleton::IsCompatibleMesh`) 중 하나면 허용한다. 계층은 맞지만 비율이 다른 리그의 애니메이션은 허용되며 화면상 리타겟 품질은 보장하지 않는다(IK Retargeter로 미리 리타겟한 Sequence 사용 권장).
- 파츠별 Triangle Count/Material Name 메모는 현재 메시가 단일 Material Slot이라 전부 동일한 값을 공유한다(2절 ⑦). 실제 캐릭터가 파츠별 슬롯을 가지면 Material Slot Names를 채우면 INSPECTION 패널에 측정값이 표시된다(6.14절).
- 본 마커 강조는 관절 위치를 점으로 보여 줄 뿐 파츠 표면 전체를 칠하지 않는다. 표면 단위 강조가 필요하면 파츠를 별도 Material Slot으로 나누거나(가장 간단), 별도 Component 구조 또는 Custom Stencil 기반 Post Process 작업이 필요하다.
- `-game` 스모크의 합성 포인터 단계는 게임 창이 활성·비가려짐 전면 창일 때만 유효하다(원인 확정, 6.10절). 무인 실행 환경에서는 이 조건을 보장할 수 없으므로 실패 시 전제 조건 오류 메시지를 먼저 확인한다.
- `Scripts/CreatePortfolioAssets.py`의 예전 "레벨 재생성" 로직이 유발하던 간헐적 크래시는 원인을 특정해 제거했지만(6.10절), 대규모 반복 재현 테스트는 하지 않았다.
- 촬영(1.7절): 고해상도 경로(`GetHighResScreenshotConfig().SetResolution`)가 이 PC의 `-game`에서 실제로 파일을 쓰는지 아직 모른다. 실패해도 6초 후 Slate 캡처(뷰포트 크기, 배율 미적용)로 대신 저장하도록 했지만, 그 경우 F12 결과가 2배 해상도가 아니다(로그와 `GetLastCaptureMethod()`로 구분). Windows는 디버거가 붙은 프로세스에서 F12를 디버그 중단 키로 쓴다(일반 실행에는 영향 없음). PIE에서는 Esc가 PIE 종료 키라 촬영 취소보다 먼저 처리될 수 있다.
- DPI 곡선 변경(2절 ⑨)은 프로젝트 전체 UMG에 적용된다. 다른 UI(예: PlayDemo)가 생기면 그 화면도 720p에서 약 20% 커진다.
- 턴테이블 촬영은 카메라는 고정하고 캐릭터 Actor만 돌린다. 조명은 월드에 고정이므로 프레임마다 조명 방향이 캐릭터 기준으로 바뀐다(일반적인 턴테이블과 같음).

## 6. 과거 기록 (참고)

이 절은 현재 상태(0~5절)와 혼동되지 않도록 뒤에 모아 둔 개발 이력이다. 숫자/원인 분석은 원문을 보존하되 서술은 압축했다.

### 6.1 최초 인계 상태 (더 이상 사실 아님)

최초 인계 시점에는 `UCharacterProfileData`가 `DisplayName`/`Description`/`SkeletalMesh` 3필드뿐이었고, 카메라/입력/GameMode/UMG/Turntable/표정/애니메이션 선택/재질 선택/Inspection/Wireframe이 전부 미구현이었다. `.uasset`/`.umap`도 없었고 Git 저장소도 아니었다(빈 홈 디렉터리 `.git`을 일부 도구가 오인했을 뿐). 2026-09-28 세션부터 이 상태는 순차적으로 해소됐다(아래 절들).

### 6.2 엔진/도구 설치 확인 (2026-09-28)

세션 시작 시점에는 UE/Epic Games Launcher/Visual Studio/MSVC/Windows SDK/.NET SDK가 이 PC에 전혀 없었다(레지스트리, 설치 프로그램 목록, 폴더 검색으로 확인). 같은 날 오후 사용자가 UE 5.6.1, VS 2022 Community(C++ 게임 개발 워크로드, MSVC 14.38.33130, Windows SDK 10.0.22621)를 설치했고, 이후 모든 빌드/테스트는 이 환경에서 실행했다. 하드웨어: Intel i5-9600K, RAM 24GB, 내장 GPU(UHD 630) — Lumen/Nanite는 비현실적이나 이 프로젝트 규모에는 충분했다.

### 6.3 Git/도구 상태

`git init -b main` + `git lfs install --local` 완료. `origin` = `https://github.com/1995dongseok/character-showcase`, `main` → `origin/main`. Git 2.55.0, Git LFS 3.7.1. `.uasset`/`.umap`은 LFS로 추적된다(`git check-attr filter`로 확인 이력 있음). `gh` CLI 로그인 상태였으나 저장소 자동 생성에는 쓰지 않았다.

### 6.4 원래 작업 지시 (원문)

```text
Docs/CHARACTER_VIEWER_SETUP.md를 읽고 현재 소스/Content/Git 상태와 실제 UE 버전을 먼저 확인해라.
현재 구현은 프로필과 Actor의 기반뿐이며 P0 완료가 아니다. 기존 코드와 아트 에셋을 보존해라.
설치된 엔진으로 빌드와 CharacterShowcase.Profile.NullSafety 테스트부터 확인해라.
P0 → P1 → P2 순서로 구현하고 각 단계의 완료 조건과 실행 증거를 확인한 뒤 다음 단계로 진행해라.
UE Editor를 사용할 수 없으면 가능한 C++ 구현과 정확한 수동 연결 절차를 남기고 실행 미검증을 밝혀라.
P0/P1을 검증할 수 없는 상태에서 P2로 범위를 넓히지 마라. .uasset/.umap 파일을 임의로 조작하지 마라.
새 문서/프레임워크/Manager/온라인 기능을 만들지 말고 이 문서 하나에 실제 설정과 검증 결과를 갱신해라.
표정은 Morph, Animation은 Sequence/Pose, 재질은 Slot별 Material Instance부터 구현해라.
실제 캐릭터가 없어도 보유 placeholder로 진행하되 에셋 호환성 검증 범위를 구분해라.
엔진/도구/remote를 자동 설치·생성하지 마라. Git/LFS 정책 확인 후 원래 요청대로 commit/push하고 결과를 보고해라.
```

### 6.5 P0/P1 C++ 구현 (2026-09-28)

엔진 설치 전, C++만 먼저 작성한 세션에서 `CharacterProfileData.h`에 8절(구 문서) 계획대로 스키마(카메라 프레이밍/프리셋, Animation 엔트리, Expression/Morph, Material Variant, Turntable 속도)를 추가하고, `CharacterViewerCameraPawn`(Orbit/Zoom/Reset/프리셋 보간), `CharacterViewerController`(Enhanced Input 바인딩 + 런타임 폴백, 드래그 판정, Turntable/Clean View), `CharacterViewerGameMode`(DefaultProfile 적용), `CharacterViewerWidget`(목록 Getter/Request 전달)를 새로 작성했다. `Config/DefaultEngine.ini`/`DefaultGame.ini`/`DefaultInput.ini`를 신규 작성했고, `CharacterShowcase.Build.cs`에 `InputCore`/`EnhancedInput`/`UMG`/`Slate`/`SlateCore`를 추가했다.

**런타임 입력 폴백**: `ACharacterViewerController::bCreateFallbackInputAssets=true`(기본값)이면 `MappingContext`가 비어 있을 때만 폴백 `UInputAction`/`UInputMappingContext`를 런타임에 생성해 매핑한다. Editor 자산(`IMC_CharacterViewer` + `IA_OrbitPress`/`IA_Orbit`/`IA_Zoom`/`IA_ResetCamera`/`IA_ToggleTurntable`/`IA_ToggleCleanView`/`IA_ToggleInspection`/`IA_ToggleWireframe`)을 만들어 할당하면 그것을 우선 사용한다. 폴백 키 매핑은 실제 사용 중인 값과 동일하다: LeftMouseButton(Orbit Press), Mouse2D(Orbit), MouseWheelAxis(Zoom), R(Reset), Space(Turntable), H(Clean View), I(Inspection), W(Wireframe).

엔진 설치 후(같은 날 오후) 빌드 0오류/0경고(16개 액션), `CharacterShowcase.Profile.NullSafety` 통과, 신규 테스트 3개(`CharacterShowcase.Viewer.ActorFeatureNullSafety`/`CameraClamp`/`ProfileLookup`) 통과 — 합계 4/4. 1차 테스트 실패(`InitializeActorsForPlay` 누락으로 `PostInitializeComponents` 미실행) 원인 확인 후 테스트 코드만 수정해 통과시켰다(액터 코드는 변경 없음).

### 6.6 Editor 자산 자동 생성 스크립트 이력

`Scripts/CreatePortfolioAssets.py`가 Unreal Editor Python API로 `DA_Character`/`DA_Character_Cube`/`BP_CharacterViewerGameMode`/`WBP_CharacterViewer`/`LV_Portfolio`/`M_Wireframe`/`M_ViewerHighlight` 7개를 생성한다(전부 엔진 튜토리얼/기본 도형 placeholder만 참조, 프로젝트 아트 없음).

- `DA_Character`: SkeletalMesh=`TutorialTPP`, CameraPresets(Face/Upper/Full, DefaultPresetId=Full), Animations(Idle/Walk 루프, Pose=Tutorial_Idle 0.5s), Expressions(Neutral 하나, 빈 Morphs — 이 메시는 Morph가 없음), MaterialVariants(Default/Grid), TurntableSpeed=20, Parts 6개(6.9절), WireframeMaterial=`M_Wireframe`.
- `DA_Character_Cube`: SkeletalMesh=`SkeletalCube`, DefaultFraming을 측정된 half-extent(12.598cm) × 6(경험적 배율, 최초 2.3배는 너무 가까워 재조정) 기준으로 계산, Animations/Expressions 빈 배열, TurntableSpeed=45(DA_Character와 구분), Parts 1개(PhysicsAsset이 없어 실제 클릭 대상 아님).
- `BP_CharacterViewerGameMode`: `DefaultProfile=DA_Character`, `ViewerWidgetClass=WBP_CharacterViewer`, `ProfileLibrary=[DA_Character, DA_Character_Cube]`.
- `LV_Portfolio`: `PortfolioCharacterActor`(yaw 90, Profile=DA_Character) 1개, KeyLight(pitch -40/yaw 30, 7 lux, ForwardShadingPriority 1)/FillLight(pitch -15/yaw -50, 2 lux, 그림자 없음)/SkyLight(DaylightAmbientCubemap) 각 1개, Cylinder 플랫폼 1개. World Settings `DefaultGameMode`=BP_CharacterViewerGameMode.

**"존재하면 보존" 동작으로 전환(2026-09-29)**: 원래는 기존 자산을 삭제 후 재생성했으나, 이제는 없을 때만 만들고 있으면 최소 형태만 읽기 전용 검증한다(`[keep] ... OK/DIFFERS`). 임시 복사본에서 2회 재실행 검증: 7개 자산 SHA-256이 재실행 전후로 완전히 동일(수동 편집 마커 `DA_Character.Description="MANUAL EDIT MARKER"`, `KeyLight` yaw +10 회전 모두 유지), 자산을 지우고 재실행하면 그것만 재생성되고 나머지 해시는 그대로. 실제 프로젝트에서도 1회 실행해 7개 전부 `[keep] ... OK` 확인.

**Python이 WBP 디자이너 트리를 만들 수 없었던 이유**: `WidgetBlueprint.get_editor_property('widget_tree')`가 `UBaseWidgetBlueprint::WidgetTree`에 CPF_Edit 지정자가 없어 실패하고, `unreal.WidgetTree` 자체도 이 엔진 빌드의 Python 모듈에 노출되지 않는다(엔진 소스로 확인). 한 번의 정직한 시도 후 중단하고, 대신 신규 Editor 전용 C++ 헬퍼 `UCharacterViewerEditorTools::BuildDefaultViewerWidgetLayout()`(`Source/CharacterShowcase/Editor/CharacterViewerEditorTools.h/.cpp`, `#if WITH_EDITOR`, `Target.bBuildEditor`로만 `UMGEditor`/`Kismet` 의존)를 추가해 C++에서 `WidgetTree->ConstructWidget<T>()`로 7위젯을 만들고 컴파일/저장한다. 최초 시도는 파라미터를 `UWidgetBlueprint*`로 선언해 Game 타깃 UHT 파싱이 실패했고(`UMGEditor`가 없는 타깃에서도 UHT가 모든 UFUNCTION 파라미터 타입을 해석하려 함), `UObject*`로 바꾸고 `.cpp`에서 `Cast<UWidgetBlueprint>()`하도록 수정해 해결(Editor/Game 둘 다 0오류/0경고).

### 6.7 검증 실행 로그 (날짜별 요약)

- **2026-09-28 (13.6절 자산 생성 후 `-game` 스모크, 최초)**: 여러 차례 시도 끝에(커맨드라인 따옴표 문제 → 레벨 재생성 중복 액터 문제 → 조명/카메라 프레이밍 문제 → 스크린샷 캡처 경로 문제 → 폴백 패널이 `NativeConstruct()`가 아니라 `RebuildWidget()`에서 만들어져야 화면에 나온다는 문제, 총 5개 원인) 순차 해결. 최종 1/1 통과, 오류 0/경고 0, 스크린샷(UI/Clean) 육안 확인 정상.
- **2026-09-28 (P1 완료 증거 보강)**: `ProfileLibrary` 2개(DA_Character↔DA_Character_Cube)로 코드 수정 없는 런타임 프로필 교체를 `-game` 스모크로 검증(1/1 통과). Win64 Development 패키지 빌드(BUILD SUCCESSFUL, 42.97초) + 패키지 exe 스모크(1/1 통과) — 쿡된 에셋에 두 프로필/GameMode/Widget/Level이 모두 포함됨을 확인.
- **2026-09-28 (P2 구현, 6.9절)**: Editor Automation 6/6(기존 4 + PartLookup/WireframeRestore 2개), `-game` 스모크 1/1(1차 시도 hover 가드 실패 1건 발견·수정 후), Win64 Development 패키지 빌드/스모크 1/1, Win64 **Shipping** 패키지 빌드 성공(94.0초) + 프로세스 기동/종료 확인(자동화 테스트 미포함).
- **2026-09-28 (Opus 리뷰 반영 패스)**: 패널 위 클릭이 실제로는 뷰포트로 새어나가 선택이 풀리던 버그를 합성 Slate 입력 테스트로 발견·수정(`UBorder::OnMouseButtonDownEvent` 바인딩 추가). 강조 색을 주황 35%→마젠타 55%로 변경(가시성 개선). Editor 6/6, `-game` 스모크 재실행 1/1(1차 실패 → 수정 후 통과), 패키지 스모크 1/1.
- **2026-09-29 (WBP 디자이너 트리 생성 후속)**: Editor 빌드 0/0(3회), Game 빌드 0/0(2회), Editor Automation 6/6, `CreateViewerWidgetLayout.py` idempotent 확인(6.8절). `-game` 스모크는 기존 P0~P2 assertion 전부 통과했으나 이번에 새로 추가한 패널 위/밖 휠·드래그 경계 테스트 3건이 이 PC의 창 최소화→복원 부작용으로 실패(6.8절에 원인 분석). 스크린샷 6장은 전부 성공.

### 6.8 폴백 UI / 디자이너 WBP 패스

`UCharacterViewerWidget`은 두 레이아웃 경로를 자동 선택한다: `WBP_CharacterViewer`(또는 그 자식)의 디자이너 트리가 있으면 그것을 `BindWidgetOptional`로 채우고, 없으면 `RebuildWidget()`에서 C++가 같은 7개 이름(`PanelRoot`/`NameText`/`ControlsBox`/`DescriptionScroll`/`DescriptionText`/`ListsScroll`/`ListsBox`)으로 최소 트리를 직접 만든다(`BuildFallbackUI()`). 두 경로 모두 이후 로직(`RefreshUI()`, `AddListSection()`, `BuildDisplaySection()`, `BuildInspectionSection()`)을 공유한다.

`NativeConstruct()`가 아니라 `RebuildWidget()`(Super 호출 전)에서 트리를 만들어야 하는 이유: `NativeConstruct()` 시점에는 Slate 변환이 이미 끝나 있어 그 뒤에 트리를 채워도 화면에 반영되지 않았다(2026-09-28 Opus 에스컬레이션에서 발견·수정).

**2026-09-29 창 최소화→복원 부작용 상세**: `-game` 실행 창이 최소화된 채 시작되어 `FSlateApplication::TakeScreenshot`이 6개 스크린샷 전부 실패한 문제를, `ShowWindow(SW_RESTORE)` + `AttachThreadInput` 기반 `SetForegroundWindow` 강제 복원 루프로 해결(스크린샷 6장 전부 성공, 연속 2회 재현). 그런데 이 복원을 겪은 실행에서는 위젯의 절대 좌표(`GetCachedGeometry().LocalToAbsolute()`)가 실제 창 위치와 어긋나는 부작용이 나타나 합성 포인터 좌표 기반 경계 테스트 3건이 실패했다. `-WinX/-WinY`로 창을 (0,0)에 고정하는 레시피(`UnrealEditor.exe`를 직접 `Start-Process`, `ShowWindow`/`SetForegroundWindow`/`SetWindowPos` 조작 전부 금지)로도 동일하게 재현되어, 창 상태/실행 방식이 원인이 아님을 확인했다(원인 미확정, 추가 세션 필요).

### 6.9 P2 구현 (Inspection/Wireframe, 2026-09-28)

**P2-0 조사**: `TutorialTPP`는 본 68개, PhysicsAsset(`TutorialTPP_PhysicsAsset`)의 Constraint 트리로 역산한 Physics Body 22개, Material Slot 1개(`TutorialTPP_Mat`, 텍스처 0개), LOD0 3,924 verts / **6,118 triangles**. `SkeletalCube`는 PhysicsAsset 없음, 본 2개, 12 triangles — Physics Asset이 없어 파츠 클릭 대상이 아니다.

**파츠 식별 방식**: Bone 기반(Line Trace hit의 `BoneName`을 `FViewerPartInfo::BoneNames`와 매칭, 실패 시 부모 본 최대 10단계 탐색). 단일 Component 구조라 Component Tag 기반은 이번에 사용하지 않았다(스키마는 지원).

**Part 매핑 6개** (물리 바디 22개를 정확히 분배): Head(head, neck_01), Torso(pelvis, spine_01~03), LeftArm/RightArm(clavicle/upperarm/lowerarm/hand _l/_r), LeftLeg/RightLeg(thigh/calf/foot/ball _l/_r) — 6개 모두 Material Slot이 1개뿐이라 TriangleCount=6,118(메시 전체)을 공유한다(6.6절 표와 동일 한계).

**강조/선택 구현**: `Mesh->SetCollisionEnabled(QueryOnly)` + Visibility 채널만 Block. `InspectAtScreenPosition()`이 트레이스 → `FindPartByBone()` → 부모 탐색 → `SetSelectedPart()`가 `Mesh->SetRenderCustomDepth(true)` + `SetOverlayMaterial(M_ViewerHighlight, 마젠타 1.0/0.0/0.8, Opacity 0.55)`를 적용한다. **Custom Depth/Overlay 모두 Component 단위이므로 메시 전체가 강조되고, 선택된 파츠 자체는 INSPECTION 패널 텍스트로만 구분된다**(0절/2절 ⑦에 동일 내용). Clean View 진입 시 강조는 숨겨지고(선택 id는 유지) 해제 시 복원된다. Clean View 중 Inspection 클릭은 무시한다(설계 결정).

**Wireframe**: `Profile->WireframeMaterial`(`M_Wireframe`, unlit/opaque/two-sided/Wireframe=true, 청록)을 모든 슬롯에 적용. 끌 때는 override 배열을 복사하지 않고 `ApplyMaterialsForCurrentVariant()`로 현재 Variant를 재조회해 복원한다(override 유실 방지). Wireframe이 켜진 동안 Variant를 선택하면 id만 갱신되고 화면은 계속 Wireframe(Wireframe이 시각적으로 우선). `WireframeMaterial`이 없는 프로필은 `SetWireframeEnabled()`가 안전하게 `false`를 반환한다(버튼도 비활성화).

**테스트**: Editor Automation 신규 2개(`CharacterShowcase.Viewer.PartLookup`, `CharacterShowcase.Viewer.WireframeRestore`), `-game` 스모크에 Inspection on→토르소 클릭→선택 확인→빈 공간 클릭 해제→Wireframe on/off→Clean View 중 클릭 무시→프로필 교체 시 선택 해제/Inspection 유지 단계를 추가. 합성 Slate 포인터(`ProcessMouseMoveEvent`/`ProcessMouseButtonDownEvent`/`ProcessMouseButtonUpEvent`)로 "패널 위 클릭은 선택하지 않는다"를 검증하는 과정에서 실제 버그(패널 배경 press가 뷰포트로 새어 나가 선택이 풀림)를 발견해 `UBorder::OnMouseButtonDownEvent` 바인딩으로 수정했다.

### 6.10 알려진 문제와 원인 분석 기록

**2026-09-29 `-game` 합성 포인터 테스트 실패 원인 확정(엔진 소스 근거)**: 실패 3회는 각각 창 최소화, 왼쪽 모니터에 뜬 비활성 창, 주 모니터 (0,0)에 떴지만 다른 응용 프로그램 창에 가려진 상태였다. 엔진 `FSlateUser::SynthesizeCursorMoveIfNeeded()`는 매 틱 실제 OS 커서 위치로 `FSlateApplication::ProcessMouseMoveEvent(..., bIsSynthetic=true)`를 호출하고, 이때 `bOverSlateWindow = !bIsSynthetic || IsActive() || IsCursorDirectlyOverSlateWindow() || ...` 조건이 거짓이면 빈 위젯 경로로 라우팅되어 패널 hover가 지워진다(`SlateApplication.cpp`, `WindowsApplication.cpp`의 `WindowFromPoint` 검사). 또 Win32는 배경 창의 마우스 캡처를 허용하지 않아 `FSceneViewport::OnMouseMove`의 축 입력(`HasMouseCapture()` 조건)이 끊긴다. 따라서 hover 가드가 풀려 패널 위 휠이 Zoom으로 새고, 캔버스 드래그가 Orbit되지 않았다. 9/28 통과 2회는 게임 창이 활성 전면 창이었다. 코드 결함이 아니므로 제품 코드는 바꾸지 않았고, 테스트의 첫 합성 포인터 단계에 `IsActive()`/`IsCursorDirectlyOverSlateWindow()` 전제 조건 검사를 넣어 이 상태를 명확한 오류로 보고하게 했다(6.8절의 "좌표 어긋남" 추정은 이 분석으로 대체한다).


- **간헐적 크래시(해결됨)**: 구버전 `CreatePortfolioAssets.py`가 레벨을 "삭제 후 완전히 새로 생성"하던 방식에서, 스크래치 레벨 전환(`new_level("/Temp/CreatePortfolioAssets_Scratch")`)이 매번 실패(대상 경로에 이미 자산 있음)했는데 스크립트가 이 반환값을 확인하지 않고 계속 진행 → 그 순간 Editor에 로드되어 있던 현재 월드(=LV_Portfolio 자신)의 패키지를 `delete_asset()`으로 삭제 → 같은 경로에 즉시 새 월드 생성 → 댕글링 포인터 접근으로 `EXCEPTION_ACCESS_VIOLATION`(5회 중 2회 재현). 크래시 덤프(`CrashContext.runtime-xml`, 로그 타임라인)로 원인을 확정했다. 2026-09-29 "존재하면 보존" 방식으로 전환하면서 이 호출 순서 자체가 코드에서 제거되어 해결됐다.
- **별개 크래시(이미 그 세션에 해결)**: `Assertion failed: !IsRooted()`(MaterialEditor 경유) — 이미 로드되어 참조 중인 Material의 Expression 그래프를 재실행마다 지우고 새로 만들던 것이 원인. 스칼라 프로퍼티만 멱등 재설정하고 표현식 그래프는 최초 생성 시에만 만들도록 수정.
- **큐브 프레이밍 조정**: `DA_Character_Cube`의 `DefaultFraming.Distance`를 half-extent × 2.3으로 계산했더니 스크린샷에서 큐브가 화면을 뒤덮어(근접 샷) 형태를 알아볼 수 없었다. × 6.0으로 올려 재확인, 정상 프레이밍 확인. 절대 크기가 작은 물체일수록 상대적으로 더 큰 여유 배율이 필요함을 기록.
- **Material usage flag 경고**: `bUsedWithSkeletalMesh=True`를 빠뜨리면 스켈레탈 메시에 적용 시 조용히 기본 머티리얼로 대체된다(`LogMaterial` 경고) — 두 신규 Material 모두 명시적으로 설정해 해결.
- **비쿠킹 `-game`에서 강조/Wireframe이 안 보이던 현상**: 값 자체(`GetOverlayMaterial()`, 슬롯 재질)는 테스트가 프로그램적으로 확인해 통과했지만, 화면에는 "Preparing Shaders" 오버레이가 뜬 채 기본 셰이딩으로 찍혔다(셰이더 프리컴파일이 끝나지 않음). 셰이더를 미리 컴파일해 두는 **쿡된 패키지에서는 두 효과 모두 정상 표시**되어 코드 결함이 아닌 것으로 판단했다. 비쿠킹 `-game`으로 확인하려면 먼저 Editor에서 해당 머티리얼을 열어 셰이더 컴파일을 끝내 두는 것을 권장(미검증).

### 6.11 파츠 단위 강조, 메시 수치 실측, 스켈레톤 호환 검사, ProjectID (2026-10-01)

**무엇/왜**: 기존 선택 강조는 `SetOverlayMaterial(M_ViewerHighlight)` + Custom Depth를 SkeletalMeshComponent 전체에 걸어서, 어느 파츠를 클릭했는지 화면으로는 알 수 없었다. 파츠 기술정보(삼각형/재질/텍스처)도 손으로 적은 값이라 6개 Part가 모두 6,118을 공유했다.

- **파츠 단위 강조**(`APortfolioCharacterActor`): `FViewerPartInfo`에 선택 필드 `MaterialSlotNames`를 추가했다. 선택 시 우선순위 ⓐ 슬롯 이름이 `Mesh->GetMaterialIndex()`로 해석되면 그 슬롯에만 신규 `M_ViewerPartHighlight`(unlit/opaque/two-sided, 마젠타 emissive)를 적용 ⓑ 아니면 Bone Names 중 메시 Reference Skeleton에 있는 본과 그 직계 자식 본마다 `/Engine/BasicShapes/Sphere` 마커(지름 10cm, 절대 스케일, 충돌·그림자 없음, Transient, 같은 재질)를 소켓에 붙임 ⓒ 둘 다 없으면 예전 메시 전체 Overlay. 현재 모드는 `GetActiveHighlightMode()`(`EViewerHighlightMode`: None/MaterialSlots/BoneMarkers/WholeMesh, 강조가 숨겨져 있으면 None)로 조회한다. Custom Depth(stencil 1)는 모든 모드에서 유지한다.
- **정확한 왕복 복원**: 슬롯 재질은 매번 `ApplyMaterialState()`가 (현재 Variant, Wireframe, 선택 파츠, 강조 표시 여부)에서 처음부터 다시 계산한다(기본값 → Variant override → Wireframe 전 슬롯 → 선택 슬롯 강조). 이전 override 배열 위에 덧칠하지 않으므로 Wireframe on/off, Variant 변경, 선택 해제, Clean View 왕복이 정확히 복원된다. 마커는 파츠 사이 전환 시 재사용(pool), Clean View 중 숨김, 선택 해제/프로필 교체(`ClearRuntimeState()`) 시 파괴된다.
- **메시 수치 실측**: `GetMeshStats()`(LOD0 삼각형/정점, 본, 슬롯, LOD, Morph, Skeleton/Physics Asset 이름, 렌더 데이터가 없으면 `bValid=false`+0), `GetSlotStats()`(슬롯별 삼각형/재질/텍스처 요약), `GetPartMeasuredStats(PartId)`(Part의 MaterialSlotNames 합산). 텍스처는 `GetUsedTextures()`를 우선 쓰고, 컴파일된 머티리얼 리소스가 없으면(-NullRHI Editor) 머티리얼 그래프 참조 텍스처 + Material Instance 텍스처 파라미터 override로 대체한다. Editor에서 플랫폼 데이터가 아직 없으면 원본(Source) 해상도를 쓴다. 손으로 적던 Triangle Count/Material Name/Texture Resolution은 호환을 위해 남기되 "선택 메모"로 바꿨다. **INSPECTION 패널은 아직 이 API를 쓰지 않는다**(위젯은 다른 작업 범위).
- **스켈레톤 호환**: `SetAnimation`이 포인터 동일성 대신 `IsAnimationSkeletonCompatible()`을 쓴다 — 같은 Skeleton, 양방향 Compatible Skeletons(런타임 데이터), `USkeleton::IsCompatibleMesh()`(본 계층 일치) 중 하나. Editor 전용 `IsCompatibleForEditor()`는 쓰지 않는다. UE5 마네킹(SK_Mannequin) 스켈레톤 ↔ UE4 TutorialTPP 메시는 쇄골의 부모 본이 달라 거부된다.
- **ProjectID**: `Config/DefaultGame.ini`의 placeholder를 무작위 GUID `E380E637D98F4E00ACE5440D4FEFA3EE`로 교체.
- `-game` 스모크(`CharacterViewerGameSmokeTest.cpp`)의 "Torso 선택 후 OverlayMaterial이 설정됨" assertion 3곳은 이제 틀린 기대값이라 "BoneMarkers 모드 + 마커 표시"로 바꾸고, 해제/Clean View 단계에 마커 0개 확인을 추가했다(주석 표기). 이 스모크는 이 세션에서 실행하지 않았다.

**어떻게 검증했나**: Editor 빌드 0오류/0경고, Game 빌드 0오류/0경고(27개 액션). Editor Automation(NullRHI) 11/11 통과 — 신규 4개: `Viewer.PartHighlightMaterialSlots`(SKM_Manny_Simple 슬롯 `M_Torso`만 강조, Wireframe/선택 해제/재선택/Wireframe 중 Variant/Wireframe off/Clean View/Variant 해제/선택 해제/프로필 교체의 각 단계에서 두 슬롯 재질 정확 비교), `Viewer.PartHighlightBoneMarkers`(TutorialTPP `upperarm_l` → 마커 3개 `upperarm_l, lowerarm_l, upperarm_twist_01_l`, Clean View 숨김/재표시, 없는 본 → WholeMesh, 재사용, 해제·프로필 교체 시 0개), `Viewer.MeshStats`(TutorialTPP 6,118 삼각형/3,924 정점/본 68/슬롯 1/텍스처 0, SKM_Manny_Simple 92,178 삼각형/48,705 정점/본 89/슬롯 2/LOD 3, 슬롯 `M_HeadLegs` 38,166 + `M_Torso` 54,012 = 합계 일치, 각 4 텍스처 최대 1024/4096), `Viewer.AnimationSkeletonCompatibility`(null 안전, 같은 스켈레톤 허용, UE5↔UE4 리그 거부 + SetAnimation이 이전 id 유지). 개발 중 1회 실패: 텍스처 해상도가 -NullRHI에서 0x0으로 나와(플랫폼 데이터 미생성) Source 해상도 대체를 추가한 뒤 통과. `CreatePortfolioAssets.py` 2회 실행 결과는 4절 표. 화면 렌더링(마커/슬롯 색이 실제로 보이는지)은 **미검증**(4.1절).

### 6.12 패널 레이아웃·포트폴리오 촬영 패스 (2026-10-01)

**배경**: 2026-09-30 Shipping 캡처(`01_default.png`, `16_cursor_over_panel.png`)에서 1280×720 패널이 약 213px(320 Slate 단위 × 엔진 기본 DPI 0.666)로 좁아 버튼 글자가 잘렸고("Tutorial Mannequi", "Turntable: Off (Space"), 설명은 3줄만 보였다. 원인은 고정 폭 320 + UTextBlock 기본 글꼴(Roboto Bold 24) + 줄바꿈 없음 + 기본 DPI 곡선.

**레이아웃**: 기본 트리 생성을 `UCharacterViewerWidget::BuildDefaultLayoutTree()` 하나로 합쳐 C++ 폴백(`BuildFallbackUI()`)과 Editor 툴(`BuildDefaultViewerWidgetLayout()`)이 같은 트리를 만든다(이전에는 배경 알파/이름 글꼴이 서로 달랐다). 패널 폭은 `NativeTick()`에서 `clamp(뷰포트 × 24%, 300, 460)`으로 맞추고(오른쪽 고정 앵커일 때만, `bAutoPanelWidth`로 끌 수 있음), 버튼 글자 13pt Regular + 자동 줄바꿈, 섹션 제목 15pt Bold, 설명 13pt·최소 6줄(`DescriptionSizeBox` 최대 높이 134), 8번째 선택 이름 `StatusText`를 추가했다. `Config/DefaultEngine.ini`에 DPI 곡선(ShortestSide 720 → 0.8, 1080 → 1.0, 1440 → 1.25)을 설정했다. `WBP_CharacterViewer`는 삭제 후 `CreateViewerWidgetLayout.py`로 재생성했다(이 스크립트가 이제 없는 WBP를 직접 만든다 — 생성 전용 규칙은 그대로). 재생성 첫 실행에서 시작 맵 로드 중 `BP_CharacterViewerGameMode`가 잠시 WBP를 찾지 못한다는 `LoadErrors` 경고가 한 번 나왔지만(그 순간 파일이 없었으므로 정상), GameMode는 저장되지 않았고(`git status` 무변경) 2차 실행에서는 경고가 없었다.

**촬영**: `ACharacterViewerController`에 `TakePortfolioScreenshot()`(F12), `StartTurntableCapture()`(Shift+F12), `CancelCapture()`(Esc)를 추가했다. 순수 로직(파일 이름, 프레임 수/각도, 상태 머신 `FViewerCaptureSequence`)은 `CharacterViewer/ViewerCapture.h/.cpp`에 분리해 Editor 테스트로 검증했다. 한 프레임 = 준비(회전) → `CaptureSettleFrames` 틱 대기 → 요청 → 파일 확인 순서이며 `PlayerTick()`에서 진행한다. 저장 경로는 `FScreenshotRequest::RequestScreenshot(파일, bShowUI=false, 접미사 없음)`, 배율 > 1이면 `GetHighResScreenshotConfig().SetFilename()/SetResolution()`을 함께 쓴다(엔진은 고해상도 요청 시 `FilenameOverride`로 파일 이름을 정하므로 둘 다 필요). `FScreenshotRequest::OnScreenshotRequestProcessed()`와 파일 존재로 완료를 판정하고, 6초 안에 파일이 없으면 그 프레임만 `FSlateApplication::TakeScreenshot`(패널을 그 순간만 숨김)으로 저장한다. Shift+F12는 폴백 IMC에서 `F12` + `UInputTriggerChordAction`(Shift 액션) 매핑을 일반 F12보다 먼저 넣어 엔진의 자동 코드 차단기(chord blocker)가 일반 F12를 막게 했다.

**검증**: Editor 빌드 1차 실패(`const` 포인터로 `UGameViewportClient::GetWindow()` 호출, C2662 1건) → 수정 1회 후 0/0. Game 빌드 0/0. Editor Automation 10/10(신규 `Viewer.PanelLayout`: 폭 계산·DPI 곡선 값·기본 트리 모양·폴백 위젯 버튼 글꼴/줄바꿈·자동 폭 적용·상태 줄·생성된 WBP 트리, `Viewer.CaptureNaming`, `Viewer.CaptureSequence`). `-game` 테스트 `CharacterShowcase.Game.ViewerCapture`는 작성만 했고 이 세션에서는 실행하지 않았다(4.1절).

### 6.13 스튜디오 룩 + Manny 기본 프로필 (2026-10-01)

**목적**: 기본 화면이 과노출된 노란 `TutorialTPP`(조준 자세)·순검정 배경·체커 무늬 원통 바닥(가장자리 보임)이었던 것을, 캐릭터 아티스트 포트폴리오에 맞는 중립 턴테이블 스튜디오 룩과 실제 인체 비율 placeholder(Manny)로 바꿨다. C++는 바꾸지 않았다(이 체크아웃의 바이너리는 커밋 d0c05b3 빌드 그대로 사용).

**새 에셋(`CreatePortfolioAssets.py`의 create-missing-only 함수 + 읽기 전용 validator)**:
- `M_StudioBackdrop`(Unlit, Two Sided, `WorldPosition.Z` 기반 세로 그라데이션: `lerp(BottomColor, TopColor, saturate((Z - GradientBottomZ) / GradientHeight)) * Brightness` → Emissive) / `MI_StudioBackdrop`. validator: Shading Model + 파라미터 이름 존재, MI는 Parent만 확인(값은 아티스트 소유라 비교하지 않음).
- `M_StudioFloor`(Default Lit, 바닥 Actor 피벗 기준 XY 거리로 `BaseColor→EdgeColor`, `Specular→0` 페이드, Roughness/Specular 노출) / `MI_StudioFloor`.
- `DA_Character_Manny`: `SKM_Manny_Simple`(Physics Asset `PA_Mannequin` 확인 — 메시는 참조만, 재저장 안 함), Animations Idle=`MM_Idle`(loop) / Walk=`MF_Unarmed_Walk_Fwd`(loop) / Jog=`MF_Unarmed_Jog_Fwd`(loop) / Pose=`MM_Idle` 0.5 s(`Is Pose`), Default Animation Id=Idle(Default Anim Class 비움), Expressions=Neutral(빈 Morphs), Material Variants=Default(override 없음) + Grid(실제 슬롯 이름 `M_HeadLegs`(0)/`M_Torso`(1) 둘 다 `WorldGridMaterial`), Wireframe=`M_Wireframe`, Turntable 20°/s. Parts 6개의 본 이름은 메시 참조 스켈레톤(89본)과 `SK_Mannequin`(161본) 양쪽에 모두 있는지 검사해 넣었다(누락 0개): Head(head, neck_01, neck_02), Torso(spine_01~05, pelvis, clavicle_l/r), Left/Right Arm(upperarm, lowerarm, hand), Left/Right Leg(thigh, calf, foot, ball).
- **카메라 프리셋(측정값 기반 계산, 가로 FOV/16:9)**: 메시 bounds Z 0~180.5 cm, 참조 자세 `head` 본 162.6 cm, `pelvis` 95.9 cm. 저장된 값(에셋에서 다시 읽음): **Full Body** Target Z 90.3 / Distance 458.6 / FOV 45 / Min·Max Distance 206.4·917.2 / Pitch -80~5 — 높이 전체 + 위아래 5% 여백, 발끝이 카메라 쪽으로 나온 깊이(bounds Y 28.1 cm)만큼 뒤로. **Upper Body** 138.2 / 229.6 / 40 / 114.8·574.1 / -80~13 — pelvis~머리 끝 + 5% 여백. **Face** 165.3 / 178.6 / 30 / 89.3·446.6 / -70~21 — Target = head + 0.15×(top−head), 반높이 1.5×(top−head). 계산식은 `create_or_update_character_profile_manny()`. **Max Pitch**는 Max Distance에서도 카메라가 바닥(Z 0) 아래로 내려가지 않도록 계산했다(양의 Pitch = 카메라가 타깃보다 아래).
- **파츠별 삼각형/재질/텍스처**: 5.6 Python API는 LOD별 정점 수와 섹션→슬롯 매핑은 주지만 섹션/본별 삼각형 수는 주지 않는다. 그래서 메시를 읽기 전용으로 ASCII FBX(LOD0)로 스크래치 폴더에 내보내(`unreal.Exporter.run_asset_export_task`) 삼각형마다 세 정점의 최대 스킨 가중치 본 → 부모를 따라 올라가 Part 본을 찾고 다수결로 Part를 정한 뒤 슬롯별로 셌다. 결과: Head 9,206(`M_HeadLegs` 100%), Torso 25,680(`M_Torso` 22,148 + `M_HeadLegs` 3,532), 팔 각 19,680(`M_Torso` 15,928 + `M_HeadLegs` 3,752), 다리 각 8,966(`M_HeadLegs` 8,962 + `M_Torso` 4), 합계 92,178 = AssetRegistry `Triangles` 태그(스크립트가 생성 시 다시 대조). 슬롯 합계 `M_HeadLegs` 38,166 / `M_Torso` 54,012. Material Name은 Part에서 5% 이상을 차지하는 슬롯의 MI 이름과 비율, Texture Resolution은 그 MI의 텍스처 파라미터에서 읽은 실제 해상도(`MI_Manny_01_New`: Base/BNormal/MRA 전부 1024², `MI_Manny_02_New`: Base 1024², BNormal **4096²**, MRA 1024²).

**기존 에셋의 1회성 in-place 편집(스크래치 스크립트, Editor GUI 조작과 동일한 내용 — 생성 스크립트의 일반 실행은 기존 에셋을 바꾸지 않는다)**:
1. `LV_Portfolio`(`load_level` → `EditorActorSubsystem`으로 수정 → `save_current_level`): `KeyLight` 7 → 2.7 lux, 색 (255, 244, 229) / `FillLight` 2 → 0.9 lux, 색 (222, 232, 255), Atmosphere Sun Light 켬 → **끔**(Key만 sun으로 남김) / `RimLight` 신규 / `AmbientSkyLight` 1.0 → 0.36 / `PlatformCylinder` Scale (4, 4, 0.2) → (51, 51, 0.2), 재질 DefaultMaterial(체커) → `MI_StudioFloor`, Collision BlockAll → **NoCollision** / `StudioBackdrop` 신규 / `StudioPostProcess` 신규 / `PortfolioCharacter.Profile` `DA_Character` → `DA_Character_Manny`. KeyLight/FillLight 회전은 그대로. 이 편집은 생성 스크립트의 `apply_studio_setup()`을 그대로 호출했으므로, `LV_Portfolio`를 지우고 스크립트로 다시 만들면 같은 결과가 나온다(새 레벨 경로 자체는 미실행 — 4.1절).
2. `BP_CharacterViewerGameMode` CDO: `DefaultProfile` `DA_Character` → `DA_Character_Manny`, `ProfileLibrary` [DA_Character, DA_Character_Cube] → [DA_Character_Manny, DA_Character, DA_Character_Cube], Compile + Save. 생성 스크립트도 GameMode를 새로 만들 때는 같은 값을 쓴다.
3. `MI_StudioBackdrop` `BottomColor` (0.090, 0.094, 0.102) → (0.060, 0.063, 0.068), `TopColor` (0.016, 0.017, 0.021) → (0.006, 0.0065, 0.008), `MI_StudioFloor` `EdgeColor` (0.06, 0.06, 0.065) → (0.035, 0.035, 0.038) — 첫 `-game` 스모크에서 배경이 보정 렌더보다 밝게(지평선 68~74, 위쪽 41~47) 나와 MI Details 편집과 같은 방식으로 1회 조정(스크립트 상수도 같은 값으로 갱신). 부모 머티리얼(`M_StudioBackdrop`/`M_StudioFloor`)의 파라미터 기본값은 처음 만들 때의 값(0.090… / 0.06…)으로 남아 있다 — MI가 덮어쓰므로 화면에는 영향이 없고, 스크립트로 다시 만들면 새 상수가 기본값이 된다.
4. 편집 후 생성 스크립트 validator는 그대로 `[keep] ... OK`(validator 변경 불필요).

**밝기 보정 방법(캘리브레이션 렌더)**: 이 PC에서 `-game` 1회가 약 11분이라, Editor(실제 RHI)에서 레벨을 메모리에서만 수정하고 `SceneCapture2D`(Final Color LDR)로 PNG를 찍은 뒤 레벨을 디스크에서 다시 읽어 버리는 스크래치 스크립트로 값을 정했다(`LV_Portfolio.umap` 해시가 매 실행 전후 동일함을 확인). 씬 캡처는 Post Process의 노출 보정을 무시하고 고정 노출로 찍히므로, **이전 레벨을 라이트 ×4.5로 찍으면 실제 게임 스크린샷(`Docs/Evidence/2026-09-30-shipping-real-input/01_default.png`)의 바닥 밝기(sRGB 170 대 171~176)와 일치**한다는 기준을 먼저 구하고, 새 값(노출 배율 1.0 = 이전 2.0의 절반)은 라이트 ×2.25로 찍어 판단했다. 엔진 소스(`PostProcessEyeAdaptation.cpp`)상 이전 설정(`r.DefaultFeature.AutoExposure=False`, 기본 Bias 1)의 노출 배율은 2.0, 새 볼륨(Manual, Physical Camera 끔, 보정 0)은 1.0이다. 결과: 바닥 sRGB 117(목표 118), Manny 가슴 영역 평균 135, 255 근처(≥250) 픽셀은 16,000개 중 1개(스페큘러 반짝임), 배경 지평선 46~54 / 위쪽 14~19, 먼 바닥 62. 보정 중 발견한 것: 먼 바닥이 스카이라이트 큐브맵을 스치는 각도로 반사해 지평선 바로 아래가 밝은 띠가 됨 → `M_StudioFloor`에서 Specular도 거리로 0까지 페이드하도록 수정. **실제 게임 화면 확인**: `-game` 스모크 1차 바닥 120 / 가슴 ≥250 0~1개(배경만 예상보다 밝아 위 3번 조정), 2차(최종 값) 바닥 120 / 가슴 0개 / 캐릭터 전체 6~10개 / 배경 지평선 48~53·위쪽 25~29 / 먼 바닥 71. 즉 라이트·노출 보정은 게임에서도 맞았고(바닥 117 예측 → 120 실측), Unlit 배경만 캡처와 게임의 밝기가 달랐다(원인 미확인).

**발견한 제약**:
- 이 엔진 빌드에서 **`-NullRHI`로 `StaticMeshActor`를 spawn하면 `EXCEPTION_INT_DIVIDE_BY_ZERO`로 Editor가 크래시**한다(재현 1/1, 실제 RHI에서는 정상). 레벨 편집 스크립트는 `-NullRHI` 없이 실행했다. 에셋 생성/검증(`CreatePortfolioAssets.py`, 기존 레벨이 있으면 Actor를 spawn하지 않음)은 `-NullRHI`로 정상.
- `-NullRHI`에서 스켈레탈 메시 FBX 내보내기도 `Assertion failed: MeshObject`로 크래시 — 실제 RHI에서는 정상.
- `MaterialEditingLibrary.set_material_instance_*_parameter_value`의 bool 반환값은 값이 적용됐는데도 False를 돌려줬다 → 스크립트는 반환값 대신 값을 다시 읽어 검사한다.

### 6.14 INSPECTION 실측 수치 표시, Shaded Wireframe, 설명 박스 줄 맞춤 (2026-10-01)

**배경**: 6.11절에서 만든 실측 API(`GetMeshStats`/`GetSlotStats`/`GetPartMeasuredStats`)가 패널에 연결되지 않아 INSPECTION은 손으로 적은 메모만 보여 줬다. 92,178 삼각형 Manny에서 Wireframe(모든 슬롯을 `M_Wireframe`으로 교체)은 픽셀마다 선이 겹쳐 청록 실루엣이 되어 토폴로지를 볼 수 없었다. 1280×720/1920×1080 캡처에서 설명 박스(최대 높이 134)는 6줄 + 7번째 줄 절반을 보여 줬다.

**1. INSPECTION 실측 표시** (`UCharacterViewerWidget::BuildInspectionText()`, `BuildInspectionSection()`이 사용 — 폴백·디자이너 레이아웃 공통, 바인드 이름 7+1개 변경 없음, 13pt·자동 줄바꿈 그대로):
- 메시 요약(프로필이 있으면 항상): `Triangles 92,178 · Verts 48,705 · Bones 89 · Slots 2 · LODs 3 · Morphs 0` / `Skeleton SK_Mannequin · Physics PA_Mannequin`. 렌더 데이터가 없으면(`bValid=false`) `measured: n/a`. 숫자는 OS 로캘과 무관하게 `FormatThousands()`로 쉼표를 넣는다.
- 슬롯별 줄: `M_HeadLegs: 38,166 tris · MI_Manny_01_New · 4 tex, max 1024x1024`, `M_Torso: 54,012 tris · MI_Manny_02_New · 4 tex, max 4096x4096`(메시 에셋 자체의 슬롯 재질 기준, Variant/강조 재질 아님).
- 선택 전 `Click a part`. 선택 후 `DisplayName (PartType)` / Description / `Measured: <삼각형> · <슬롯> · <재질> · <텍스처>`(Part의 Material Slot Names가 메시와 일치할 때) 또는 `Authored: <Triangle Count> · <Material Name> · <Texture Resolution>`(메모) / `Highlight: Material slots | Bone markers (N) | Whole mesh | hidden`. Whole mesh + Wireframe Overlay 중이면 `Whole mesh (tint hidden while Wireframe is on)`.
- 현재 `DA_Character_Manny`의 Part는 Material Slot Names가 비어 있어 선택 파츠는 `Authored:`(6.13절에서 FBX로 측정해 넣은 값)와 `Bone markers (N)`으로 표시된다.

**2. Shaded Wireframe** (`APortfolioCharacterActor`):
- 신규 `M_WireframeOverlay`(`CreatePortfolioAssets.py`의 `create_or_update_wireframe_overlay_material()` + 읽기 전용 validator, create-missing-only): Unlit, **Translucent(`LineOpacity` 0.35, 아래 리뷰 수정 — 처음엔 Opaque)**, Two Sided, Depth Test 유지, Wireframe=True, Used with Skeletal Mesh, Emissive = `LineColor`(청록 0,1,1), World Position Offset = `(CameraPosition − WorldPosition) × DepthBiasFraction`(기본 0.002). 엔진은 Overlay Material을 같은 섹션의 추가 메시 패스로 그리고 블렌드 모드는 제한하지 않는다(엔진 소스 `SkeletalMeshSceneProxy.cpp`: 스켈레탈 메시 usage 플래그만 검사). 선은 표면과 같은 삼각형이라 깊이가 같아 점선처럼 깨질 수 있으므로, 각 정점을 카메라 쪽 시선 위로 거리의 0.2%만큼 당긴다(화면 위치는 그대로, 깊이만 앞으로 — Full Body 4.6 m에서 약 0.9 cm).
- 액터 속성(뷰어 구현 세부, 프로필 데이터 아님): `WireframeOverlayMaterial`(기본 `M_WireframeOverlay` 자동 로드), `bWireframeReplacesSlots`(기본 끔). `GetActiveWireframeMode()` = None / **Overlay(기본)** / ReplaceSlots. 우선순위: `bWireframeReplacesSlots`가 켜져 있고 프로필 `WireframeMaterial`이 있으면 예전 슬롯 교체, 아니면 Overlay, Overlay 재질이 없으면 프로필 `WireframeMaterial`로 슬롯 교체(폴백). 둘 다 없으면 `SetWireframeEnabled(true)`는 `false`(버튼 비활성, `IsWireframeAvailable()`). 끄기는 항상 성공.
- Overlay 모드는 슬롯을 건드리지 않으므로 Variant·MaterialSlots 강조·본 마커가 선 아래에 그대로 보인다. 메시의 Overlay 슬롯은 하나뿐이라 **Wireframe Overlay가 켜진 동안 WholeMesh 선택 틴트(와 `bWholeMeshTintWithBoneMarkers` 틴트)는 표시하지 않고 Custom Depth만 유지**하며 로그 1줄(`LogTemp: ... whole-mesh selection tint ... is not shown`)을 남긴다. Wireframe을 끄면 틴트가 돌아온다. Clean View는 선택 강조만 숨기고 Wireframe은 유지한다.
- `UCharacterProfileData::WireframeMaterial`은 "선택(레거시) 슬롯 교체용"으로 의미만 바뀌었다(필드·기존 에셋 값 변경 없음).

**3. 설명 박스 줄 맞춤** (`UCharacterViewerWidget`):
- 줄 높이는 Slate 텍스트 레이아웃이 실제로 쓰는 값(`FSlateTextRun::GetMaxHeight` = 글꼴 최대 문자 높이 + |그림자 Y| × 배율, 정수 픽셀)이다. `MeasureTextLinesHeight()`가 임시 `STextBlock`에 N줄 텍스트를 넣어 `SlatePrepass(배율)`로 그 높이를 직접 잰다(Slate가 없으면 예전 추정식으로 폴백). 측정값(13pt Roboto Regular, 그림자 (1,1)): 배율 1.0 → 줄 21 px, 6줄 126 Slate 단위 / 배율 0.8(720p) → 줄 16 px = 20 단위, 6줄 120 단위 / 배율 1.25 → 줄 27 px = 21.6 단위, 6줄 129.6 단위. 예전 134는 1080p에서 6.38줄, 720p에서 6.70줄이었다(캡처의 "7번째 줄 절반"과 일치).
- `BuildDefaultLayoutTree()`(C++ 폴백과 Editor 툴 공용)는 배율 1.0 기준 126을 저장하고, 런타임에는 `NativeTick()`의 `ApplyDescriptionLineFit(MyGeometry.Scale)`이 DPI 배율·글꼴·그림자가 바뀔 때만 `DescriptionScroll`의 부모 SizeBox 높이를 `DescriptionVisibleLines`(기본 6)줄로 다시 맞춘다(`bFitDescriptionToWholeLines`, 기본 켬 — 디자이너가 직접 높이를 정하려면 WBP Class Defaults에서 끈다). 생성 트리 값이 바뀌어 `WBP_CharacterViewer`를 삭제 후 `CreateViewerWidgetLayout.py`로 재생성했다(4절 표).

**`CharacterShowcase.Game.ViewerCapture` 추가 단계(포인터 없음)**: UI/Clean 캡처 뒤 Inspection On + 현재 프로필의 첫 Part를 코드로 선택(클릭 없음 → 전면 창/커서 전제 조건 불필요), 패널 목록을 끝까지 스크롤, 2초 후 `ViewerCapture_Inspect_<W>x<H>.png`(강조 모드·보이는 본 마커 수·본 이름 로그), Wireframe On(선택 유지) 2초 후 `ViewerCapture_Wireframe_<W>x<H>.png`(Wireframe 모드·Overlay 재질 이름 로그), 이어서 선택 해제·Wireframe Off·Inspection Off·스크롤 원위치. 약 4.5초 추가. 이 세션에서는 실행하지 않았다(`-game` 창이 필요).

**`-game` 스모크(`CharacterViewerGameSmokeTest.cpp`) assertion 갱신(실행 안 함)**: "Click a part"는 이제 본문 끝 + 본문이 `Triangles `(또는 `measured: n/a`)로 시작하는지 검사, Wireframe On은 Overlay 모드면 Overlay 재질 = `WireframeOverlayMaterial`이고 슬롯이 레거시 `WireframeMaterial`이 아닌지, Wireframe 중 Grid 선택은 슬롯 0 = Grid + Overlay 유지, Wireframe Off 후 Overlay 없음(레거시 모드면 예전 assertion).

**검증**: 4절 표. Editor Automation 16/16 — 신규 `Viewer.WireframeOverlay`(SKM_Manny_Simple: 기본 Overlay 모드, 켜도 override 0개, Variant/슬롯 강조/본 마커가 Overlay 아래 유지, WholeMesh + Wireframe = Overlay는 선·Custom Depth 유지, Clean View에서 Wireframe 유지, 끄면 틴트 복귀, 반복 왕복 후 override 0, 레거시 옵트인 왕복, 프로필 해제), `Viewer.InspectionPanelText`(위 숫자가 폴백 패널 본문에 그대로 나오는지, Measured/Authored/Highlight 줄, `measured: n/a`, Inspection Off 시 접힘), `Viewer.PanelLayout`(저장 높이 = 측정 6줄, 6줄 = 6 × 1줄, 7번째 줄이 박스 바닥에서 정확히 시작, 배율 0.8/1.0/1.25 런타임 맞춤, 옵션 끄면 유지, 생성된 WBP도 126). 화면 확인(720p Manny에서 선이 음영 위에 끊김 없이 보이는지, INSPECTION 텍스트 가독성, 설명 6줄)은 **미검증**(4.1절).

**알려진 사항**: 새 바이너리로 `M_WireframeOverlay`가 없는 상태에서 Editor를 처음 띄우면 CDO 생성자 로그 `LogUObjectGlobals: Error: CDO Constructor (PortfolioCharacterActor): Failed to find .../M_WireframeOverlay`가 1회 나온다(에셋 생성 전 1차 스크립트 실행에서 발생, 2차부터 없음 — 생성자에서 기본 재질을 찾는 기존 방식의 부트스트랩 순서 문제). 3절 설계 규칙의 "Wireframe은 Variant보다 화면에서 우선한다"는 레거시 슬롯 교체 모드에만 해당하며, 기본 Overlay 모드에서는 Variant가 선 아래에 보인다(3절은 이 패스에서 수정하지 않았다).

**리뷰 수정(같은 날, Fable의 `-game` `ViewerCapture` 1280×720 캡처 기준 결함 3건)**:
1. **본 마커가 안 보임**: `head`/`neck_01`/`neck_02`의 불투명 10 cm 구체가 Manny 머리·목 안에 완전히 묻혔다. → 신규 `M_ViewerBoneMarker`(create-missing-only + validator: Translucent, Unlit, Two Sided, **Disable Depth Test = True**, 마젠타 emissive, Opacity 0.85; Disable Depth Test는 반투명 전용 플래그라 불투명 슬롯 강조 `M_ViewerPartHighlight`와 분리). 액터 속성 `BoneMarkerMaterial`(기본 자동 로드, 없으면 `PartHighlightMaterial`로 대체)을 마커 컴포넌트에만 쓰고, 슬롯 강조는 계속 `M_ViewerPartHighlight`. `BoneMarkerDiameter` 기본 10 → 12 cm.
2. **Wireframe이 단색 실루엣**: 깊이 바이어스가 아니라 밀도 문제(720p에서 거의 모든 픽셀에 선). → `M_WireframeOverlay`를 **Translucent + `LineOpacity` 0.35**로 변경(선 하나는 표면을 35% 물들이고, 겹친 선은 1−0.65ⁿ로 포화), Wireframe/Two Sided/Unlit/청록/WPO 깊이 바이어스 유지, Disable Depth Test는 끔(validator가 검사). 기존 `M_WireframeOverlay.uasset`은 이 스크립트가 이 작업 브랜치에서 만든 것(main에는 아직 없음)이라 **파일을 삭제하고 스크립트로 다시 만들었다**(생성 전용 규칙 그대로 — validator만 새 값으로 바뀜).
3. **선택 파츠 블록이 패널에 안 보임**: INSPECTION 섹션은 이미 `ListsScroll` 안의 마지막 섹션이었다(`ListsBox`의 자식). 원인은 캡처 테스트가 `NotifySelectionChanged()` 직후 `ScrollToEnd()`를 1회 호출했는데, 막 만들어진 행(특히 자동 줄바꿈 텍스트)의 최종 높이가 1~2 레이아웃 패스 뒤에야 정해져 스크롤이 이전 끝 위치에서 멈춘 것(캡처의 스크롤바 썸이 끝에 닿지 않음, "Grid" 버튼 윗부분이 잘려 보이는 것은 스크롤 중간 위치이기 때문). → `RequestListsScrollToEnd(Ticks)`가 다음 N틱 동안 매 틱 `ScrollToEnd()`를 다시 호출하고, `RefreshUI()`는 Inspection 중 **새 파츠가 선택될 때마다** 이를 요청한다(같은 선택에서의 일반 갱신은 스크롤을 건드리지 않음). 실제 사용에서도 파츠를 클릭하면 선택 파츠 정보가 패널에 바로 보인다. `ViewerCapture`는 캡처 직전 `GetScrollOffset()`이 `GetScrollOffsetOfEnd()`와 1 Slate 단위 안인지(또는 목록이 스크롤 없이 다 들어가는지) 검사하고, 목록 영역 높이와 INSPECTION 본문을 로그로 남긴다. 1280×720에서 목록 영역은 약 190 px(이름·CHARACTER/VIEW/DISPLAY 13개 버튼·설명 6줄 다음 남는 높이)이라 INSPECTION 전체는 한 화면에 안 들어가며, 끝으로 스크롤하면 선택 파츠 블록(마지막 행)이 보이는 구조다.
- 추가: 설명 줄 높이 측정에 실제 설명의 비 ASCII 문자(최대 16자, `GetLineHeightSample()`)를 넣어 한글처럼 대체 글꼴로 그려지는 줄도 재도록 했다. 이 엔진의 기본 글꼴 구성에서는 한글 줄도 라틴 줄과 같은 높이로 측정됐다(배율 0.8 → 120, 1.0 → 126, `Viewer.PanelLayout` 로그). main 캡처(`ViewerCapture_UI_1280x720.png`)를 확대해 보면 설명은 정확히 6줄이고, 6번째 줄 아래가 어둡게 보이는 것은 스크롤 박스의 "아래에 더 있음" 그림자다(7번째 줄 일부가 아님).
- **검증**: 4절 표(빌드 0/0, Editor Automation 16/16, 스크립트 `[keep] OK` ×15, 새 재질 실제 RHI 컴파일). 사용자가 PC를 쓰고 있어 이 세션에서는 `-game` 캡처를 실행하지 않았다 — 세 결함의 화면 확인은 **미검증**(4.1절).

### 6.15 프로필 검증 도구 (2026-10-01)

**목적**: 주니어 아티스트가 `CharacterProfileData`를 잘못 채우면(본 이름 오타, 없는 슬롯, 다른 Skeleton의 애니메이션, Min/Max가 뒤집힌 카메라 등) 지금까지는 Viewer에서 "버튼이 아무 일도 안 함"으로만 드러났다. 무엇이 어디서 틀렸고 어떻게 고치는지 바로 알려 주는 읽기 전용 검사기를 추가했다(사용법·검사 표는 2.10절).

- **`UCharacterProfileValidator`**(`Character/CharacterProfileValidator.h/.cpp`, `UBlueprintFunctionLibrary`, 런타임 모듈 — Editor·`-game`·패키지 모두 동작): `ValidateProfile(Profile)` → `TArray<FViewerProfileIssue>`(`Severity` Error/Warning/Info, `Category` Mesh/Camera/Animation/Expression/Material/Part/Play, 한국어 `Message`, Details 경로 `Field`), `FormatReport`, `CountBySeverity`, `CountByCategory`, `SeverityToString`, `LogProfileReport`. 구조체/열거형은 `BlueprintType`이라 Python에서 `unreal.CharacterProfileValidator.validate_profile()`로 그대로 호출된다. 에셋은 읽기만 한다.
- **재사용한 규칙**: 애니메이션 호환은 `APortfolioCharacterActor::IsAnimationSkeletonCompatible()`(6.11절), Variant 슬롯 해석 순서(Slot Name → Slot Index)는 `ApplyVariantOverrides()`와 같다. 텍스처 수집은 Actor의 `CollectTextures`(private namespace) 로직을 검사기 안에 최소한으로 복사했다(Actor 파일은 다른 작업 범위라 수정하지 않음 — 두 곳을 같이 고쳐야 한다). Editor 전용 검사는 메시 높이(`GetImportedBounds`, 50 cm 미만/300 cm 초과 → "스케일 확인")만이며 `#if WITH_EDITOR`.
- **판단 기준**: 엔진의 `FName` 비교가 대소문자를 구분하지 않으므로 `HEAD`는 `head`와 같은 본으로 통과시킨다(실제로 동작함). Expression은 메시에 Morph가 0개면 Morph마다 Error를 내지 않고 프로필당 Warning 1건만 낸다. Variant 슬롯 이름이 틀려도 Slot Index로 대신 적용되면 Warning, 적용되지 않으면 Error.
- **런타임 로그**: `ACharacterViewerGameMode::PostLogin`(시작 프로필)과 `ACharacterViewerController::SwitchProfile`(CHARACTER 전환)에서 `LogProfileReport()` 1줄 호출 — `LogTemp` Display/Log 등급만 써서 Error가 있는 프로필로 전환해도 자동화 테스트가 실패하지 않는다. 위젯에는 아무것도 추가하지 않았다(상태 줄 연결은 후속 작업).
- **`Scripts/ValidateProfiles.py`**: `/Game/Portfolio/Data`(또는 `-ProfilePath=`)의 모든 `CharacterProfileData`를 검사해 `[ValidateProfiles] <asset>: E=n W=n I=n` + 문제마다 1줄 + `TOTAL ... RESULT=PASS|FAIL`을 출력하고, Error(또는 로드 실패)가 있으면 예외를 던진다. `-ExecutePythonScript`로 실행한 Editor는 스크립트 예외와 관계없이 종료 코드 0으로 끝나므로(엔진 `EditorPythonExecuter.cpp`가 `QUIT_EDITOR`만 요청), 종료 코드가 필요한 경우를 위해 `-run=pythonscript` 커맨들릿 실행을 기본으로 문서화했다(커맨들릿은 예외 시 -1 반환).

**검증**: Editor 빌드 0/0(10개 액션), Game 빌드 0/0(16개 액션, 118.6초). Editor Automation **18/18 통과** — 신규 4개: `Validator.NullAndEmpty`(null → Error 1, 빈 프로필 → Mesh E1 W1 + Material I1, 총 3건), `Validator.DeliberateMistakes`(TutorialTPP에 실수 28개: Camera E3 W3, Animation E5 W1 I1(`MM_Idle` ↔ TutorialTPP Skeleton 거부 포함), Expression W1, Material E2 W3 I1, Part E4 W3(`Spine01` → `'spine_01'`, `hand` → `'hand_l' 또는 'hand_r'`, `upperarm_left` → `'upperarm_l'` 제안 확인), Play W1, Mesh 0 — 카테고리·심각도별 개수와 필드 위치를 정확히 비교), `Validator.MorphsTexturesSkeleton`(임시 메시: Skeleton 없음 Error, Physics Asset 없음 + 파츠 없음 Info, 높이 0 cm Warning, Morph 12개 중 없는 이름 2개 → Error 2건 + "외 2개" 목록 잘림, `MI_Manny_01_New` 자식 MI에 300×200 텍스처 → Warning, 8192×16 → Info), `Validator.ContentProfiles`(Manny·Tutorial E=0 W=0 I=0, Cube E=0 W=2 — Physics Asset 경고 문구 확인). `ValidateProfiles.py` 실행 결과는 4절 표. 런타임 로그 훅은 **미실행**(4.1절). 빌드 중 다른 세션의 `-game` 프로세스가 Live Coding 뮤텍스를 잡고 있어 UBT가 거부했으므로 `-NoHotReloadFromIDE`를 붙여 빌드했다(이 worktree의 바이너리만 만들며 실행 중인 프로세스에는 영향 없음).

### 6.16 아티스트용 진입 문서와 Tools\*.bat (2026-10-01)

**무엇/왜**: Unreal을 처음 여는 아티스트가 이 문서(개발/인계 기록)부터 읽지 않도록 짧은 진입 문서 3개와 더블클릭 실행 파일을 만들었다. C++/Content/Config는 바꾸지 않았다.

- `README.md`(저장소 소개, 10분 보기, 뷰어+플레이 데모 키 표, 촬영, 하드웨어, 문제 시 링크), `Docs/ARTIST_QUICKSTART.md`(0 설치 ~ 7 문제 해결, 단계마다 "확인 방법", 2절 ①~⑨ 체크리스트), `Docs/FBX_IMPORT_GUIDE.md`(단위·축, `SK_Mannequin` 재사용 vs 새 Skeleton, Physics Asset, Morph, Material Slot = 파츠, 텍스처 sRGB, 애니메이션 Root Motion, 자동 측정 값, 커밋 전 체크리스트).
- `Docs/Images/` 4장(`viewer_ui_1280x720.png`, `viewer_clean_1280x720.png` = `Saved/Screenshots/ViewerCapture/ViewerCapture_{UI,Clean}_1280x720.png`, `troubleshoot_preparing_shaders.png` = `ViewerSmoke_Inspect.png`, `playdemo_run.png` = `DemoSmoke_Run.png`, 각 1 MB 미만). `.gitattributes`에 `Docs/Images/*.png` LFS 규칙과 `*.bat text eol=crlf`를 추가했다(기존 `Docs/Evidence/*.png`는 일반 Git 그대로).
- `Tools\_Common.bat`(옵션 파싱, 프로젝트 루트 `%~dp0..`, `UE_ROOT` 탐지, `Binaries\Win64\UnrealEditor-CharacterShowcase.dll` 유무, `LV_Portfolio.umap` 1 KB 미만이면 LFS 포인터로 판정) + `OpenEditor`, `RunViewer`, `RunPlayDemo`, `ValidateProfiles`, `PackageViewer`, `BuildEditor` 6개. ASCII 전용, `setlocal`, 오류 시 원인 출력 + 종료 코드 1, 끝에 `pause`(`--check`/`--no-pause`면 생략).
- 작성 중 발견: 호출된 배치에서 `shift` 후에는 `%~dp0`도 밀린다 → `_Common.bat`이 맨 앞에서 `TOOLS_DIR`을 먼저 저장하도록 1회 수정(수정 전에는 프로젝트 루트를 한 단계 위로 잘못 계산해 6개 모두 `Project file not found`, 종료 코드 1).

**검증(`--check`만, UE를 띄우지 않음)**: 먼저 가짜 `UE_ROOT`(빈 exe/에코 배치)로 실행해 아무것도 시작되지 않음을 확인한 뒤 실제 경로로 실행했다. 8회 모두 종료 코드 0, 공통 머리말 `Project : ...\character-showcase\CharacterShowcase.uproject` / `Engine : C:\Program Files\Epic Games\UE_5.6` / `C++ : editor module built` / `Content : ok`.

| 명령 | `[check]` 출력(경로는 `<UE>` = `C:\Program Files\Epic Games\UE_5.6`, `<P>` = 프로젝트 루트로 줄임) |
| --- | --- |
| `OpenEditor.bat --check` | `start "" "<UE>\Engine\Binaries\Win64\UnrealEditor.exe" "<P>\CharacterShowcase.uproject"` |
| `RunViewer.bat --check` | `start "" "<UE>\...\UnrealEditor.exe" "<P>\CharacterShowcase.uproject" /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=1920 -ResY=1080` (`--720p`: `-ResX=1280 -ResY=720`) |
| `RunPlayDemo.bat --check` | map file `<P>\Content\PlayDemo\Maps\LV_PlayDemo.umap (31204 bytes)`, `... /Game/PlayDemo/Maps/LV_PlayDemo -game -windowed -ResX=1920 -ResY=1080` |
| `ValidateProfiles.bat --check` | `[ValidateProfiles] Scripts\ValidateProfiles.py is not present yet - nothing to validate.` + `would run once the script exists: "<UE>\...\UnrealEditor-Cmd.exe" "<P>\CharacterShowcase.uproject" "-ExecutePythonScript=<P>\Scripts\ValidateProfiles.py" -NullRHI -unattended -nosplash -nop4 "-abslog=<P>\Saved\Logs\ValidateProfiles.log"` |
| `BuildEditor.bat --check` | `Visual Studio installer: found`, `call "<UE>\Engine\Build\BatchFiles\Build.bat" CharacterShowcaseEditor Win64 Development -Project="<P>\CharacterShowcase.uproject" -WaitMutex` |
| `PackageViewer.bat --check` (`--zip-only` 동일 + "step 1 would be skipped") | `call "<UE>\...\RunUAT.bat" BuildCookRun -project="<P>\CharacterShowcase.uproject" -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory="<P>\Saved\Packaged" -unattended -noP4 -utf8output` → `robocopy "<P>\Saved\Packaged\Windows" "<P>\Saved\Packaged\_zipstage\CharacterShowcase-Win64-20261001" /E ... /XF *.pdb CharacterShowcase-Win64-Shipping.*` → `Compress-Archive ... -DestinationPath '<P>\Saved\Packaged\CharacterShowcase-Win64-20261001.zip'` |

추가 확인: 알 수 없는 옵션 `--bogus` → 종료 코드 1, 없는 `UE_ROOT` → `[ERROR] Unreal Engine 5.6 not found` 종료 코드 1, 스크래치 가짜 프로젝트(130바이트 LFS 포인터 `LV_Portfolio.umap`) → `Content : lfs-pointer` + 종료 코드 1.

**미검증**: .bat의 실제 실행(Editor/`-game`/UnrealEditor-Cmd/Build.bat/RunUAT 기동, robocopy·`Compress-Archive` 압축)은 하지 않았다 — 같은 체크아웃에서 패키지·스모크가 돌고 있었기 때문이다. 문서의 Editor 메뉴 경로·FBX 옵션 이름(UE 5.6 Interchange 가져오기 창), 막 받은 저장소의 전체 C++ 빌드·첫 셰이더 컴파일 시간, 일반 GPU의 FPS, 다른 PC에서 패키지 zip 실행은 측정·확인하지 않았다. `ValidateProfiles.bat`의 요약 줄 형식은 `Scripts/ValidateProfiles.py`(별도 작업)의 출력 형식 `[ValidateProfiles] <asset>: E=.. W=.. I=..`을 전제로 한다.

### 6.17 `-game` 자동화 테스트 시작 지연 제거, DCC 원본 gitignore (2026-10-01, Fable)

- **증상**: 1920×1080 `-game` `ViewerCapture`가 테스트를 시작하지 않고 10분을 기다렸다. 로그: `FWaitForInteractiveFrameRate: Game did not reach 10.00 FPS within 600.00 seconds. Current FPS=4`. 원인은 Editor 바이너리로 `-game`을 띄울 때 `FAutomationControllerManager::IsReadyForTests()`가 `UAutomationTestSettings`의 기본값(10 FPS를 5초 유지, 최대 600초 대기)을 요구하는 것인데, 이 PC(Intel UHD 630)는 1080p에서 4 FPS라 끝까지 못 넘는다(720p는 간헐적으로 10 FPS를 넘어 통과했다).
- **조치**: `Config/DefaultEngine.ini` `[/Script/Engine.AutomationTestSettings]`에 `DefaultInteractiveFramerate=1`, `DefaultInteractiveFramerateDuration=1`, `DefaultInteractiveFramerateWaitTime=30`. 테스트 자체의 대기(latent command)는 그대로다. 패키지 빌드에는 이 게이트가 없다(`WITH_EDITOR`).
- **같은 커밋 묶음**: `.gitignore`에 `*.blend`, `*.blend1`, `*.fbx`, `*.obj`, `*.abc`, `*.tif`, `*.tiff`, `*.exr`, `*.sbs`, `*.sbsar` 추가(FBX_IMPORT_GUIDE의 "원본은 저장소 밖" 규칙을 강제).
- **검증**: 이 설정으로 실행한 `-game` 결과는 4절 표의 이후 행 참조(사용자가 PC를 사용 중이면 게임 창을 띄우지 않으므로, 데스크톱이 빈 뒤 실행).

### 6.18 재생 컨트롤, LOD 표시, 배경 프리셋 (2026-10-01)

**목적**: 특정 프레임의 포즈·변형(어깨/팔꿈치 접힘 등)을 멈춰 놓고 보기, LOD별 메시 품질과 삼각형 수 확인, 단색 배경에서 실루엣 확인. 키·패널 사용법은 1.7절.

**1. 재생 컨트롤** (`APortfolioCharacterActor`, 신규 함수만 추가 — `SetAnimation()`은 성공 끝에 상태 적용 1줄만 추가):
- `IsAnimationPlaybackControllable()`: AnimationSingleNode 모드 + Single Node Instance에 Sequence가 있을 때만 true. Animation Blueprint(`DefaultAnimClass`)가 움직이는 동안, 또는 재생 중인 Sequence가 없으면 아래 setter는 전부 `false`이고 아무것도 바꾸지 않는다.
- `SetAnimationPaused(bool)` / `IsAnimationPaused()`, `StepAnimationFrames(int32)`(먼저 일시정지, 시퀀스 `GetSamplingFrameRate()` 기준으로 현재 프레임(반올림) ± N, 0..NumFrames에서 순환), `SetAnimationPlayRate(float)`(0.1~2.0으로 clamp) / `GetAnimationPlayRate()`, `GetAnimationTimeInfo(Time, Length, Frame, NumFrames)`(`GetPlayLength()`, NumFrames = `GetNumberOfSampledKeys() - 1` — MM_Idle: 7.567 s, 228키, 30 fps → 프레임 0..227), 순수 함수 `WrapAnimationFrame`/`ClampAnimationPlayRate`(단위 테스트).
- **결정**: 일시정지·속도는 애니메이션을 바꿔도 유지한다(일시정지 중 선택한 새 애니메이션은 0초에서 멈춘 채 시작 — `PlayAnimation()`이 속도를 1.0으로 되돌리므로 바로 다시 적용). Pose 항목(`bIsPose`)은 항상 정지(`IsAnimationPaused()` = true, 재개 요청은 false, 프레임 이동은 허용). 끝난 1회 재생 애니메이션을 재개하면 0초부터. `ClearRuntimeState()`(프로필 전환)가 재생·1.0배로 초기화한다.
- Controller: `ToggleAnimationPaused()`, `StepAnimationFrames()`, `ChangeAnimationPlayRate(Delta)`, `ResetAnimationPlayRate()`(전부 위젯 갱신, 턴테이블 촬영 중에는 무시), 속도 단계 `AnimationRateStep`(0.25). 키 P, `[`, `]`, `-`(+숫자패드 -), `=`(+숫자패드 +), `0`.
- Widget: ANIMATION 섹션 바로 아래 `PLAYBACK` — 1줄 `Pause (P)`/`Resume (P)` · `◀ ([)` · `▶ (])`, 2줄 `Slower (-)` · `Rate 1.00 (0)` · `Faster (=)`(같은 폭 3칸, 속도가 한계면 해당 버튼 비활성), 시간 줄 `0.10 s / 7.57 s · frame 3 / 227`. 시간 줄은 `NativeTick()`의 `UpdatePlaybackTimeText()`가 **프레임(또는 시퀀스) 번호가 바뀔 때만** `SetText`한다. 프로필에 애니메이션도 AnimBP도 없으면 섹션을 숨긴다. 디자이너 바인드 이름 변경 없음.

**2. LOD 표시** (`APortfolioCharacterActor`): `SetForcedLOD(int32)`(엔진 규칙 그대로 0 = Auto, N = LOD N-1 고정, 범위 밖이면 false), `GetForcedLOD()`, `GetNumLODs()`, `GetDisplayedStatsLOD()`(강제 LOD, Auto면 0). `GetMeshStats(LODIndex = -1)`/`GetSlotStats(LODIndex = -1)`에 선택 인자를 추가했다(-1 = 표시 중인 LOD, 기존 호출은 그대로 컴파일). `GetPartMeasuredStats()`도 표시 LOD 기준. `FViewerMeshStats`에 `LODIndex` 필드 추가. 슬롯별 삼각형은 그 LOD의 `LODMaterialMap`/렌더 섹션으로 센다. `BuildInspectionText()`는 수정하지 않았다(기본 인자로 자동 반영). Auto일 때 LOD0 기준인 이유: 자동 LOD는 카메라 거리로 계속 바뀌므로 패널 숫자는 저작 기준 LOD0으로 둔다. 프로필 전환 시 Auto. 패널 DISPLAY `LOD: Auto (L)` / `▶ LOD: LOD2 of 3 (L)`, 키 L(Auto → LOD0 → LOD1 → LOD2 → Auto). 측정(SKM_Manny_Simple): LOD0 92,178 / LOD1 26,998 / LOD2 12,998 삼각형, 정점 LOD0 48,705 / LOD2 7,504.

**3. 배경 프리셋** (`ACharacterViewerController`, 별도 블록): `EViewerBackdropPreset` Studio / Black / White / MidGrey, `SetBackdropPreset()`, `CycleBackdropPreset()`(키 B), `GetBackdropPreset()`, `GetBackdropTargetCount()`. 처음 적용할 때 월드의 Static Mesh 컴포넌트 중 재질(또는 부모 체인)이 `MI_StudioBackdrop`/`M_StudioBackdrop`, `MI_StudioFloor`/`M_StudioFloor`인 슬롯을 찾고(쿡된 빌드에는 Outliner 라벨이 없음 — Editor에서는 라벨 `StudioBackdrop`/`PlatformCylinder`도 인정), 그 슬롯에 Dynamic MI를 한 번 만들어(이미 Dynamic MI면 그대로 사용) 그 순간의 `TopColor`/`BottomColor`·`BaseColor`/`EdgeColor`를 원래 값으로 저장한다. Black 0 / White 0.8 / Mid Grey 0.18 단색(Linear), Studio = 저장한 원래 값. 조명·노출(`StudioPostProcess`)은 건드리지 않는다. `LV_Portfolio.umap`과 MI 에셋은 수정하지 않는다. Actor가 없으면 프리셋 값만 기록하고 로그 1줄(`no studio backdrop/floor found`, Log 등급). Clean View·프로필 전환에서 유지. 패널 DISPLAY `Backdrop: Studio (B)` / `▶ Backdrop: Mid Grey (B)`. 원래 값을 "BeginPlay에서" 잡는 대신 **처음 B를 누를 때** 잡는다 — 그 전에는 아무것도 값을 바꾸지 않으므로 같은 값이고, B를 쓰지 않으면 레벨 재질이 전혀 바뀌지 않는다.

**다른 세션과의 경계**: D 세션 영역(`ApplyHighlightState`/`ApplyOverlayState`/`UpdateBoneMarkers`, `BuildInspectionSection`/`BuildInspectionText`)은 수정하지 않았다. Controller에서는 입력 바인딩·폴백 IA·키 매핑을 캡처 항목 바로 뒤에 붙였고, 나머지 함수는 파일 끝의 별도 블록이다. Widget은 `RefreshUI()`에 `BuildPlaybackSection(ListsBox)` 1줄, `BuildDisplaySection()`에 `BuildViewOptionRows(SectionBox)` 1줄, `NativeTick()`에 1줄, 버튼 종류 enum 끝에 8개, 클릭 switch에 8개 case를 추가했다.

**검증**: Editor 빌드 0/0, Game 빌드 0/0(17개 액션, 85.3초). Editor Automation **24/24 통과**(기존 20 + 신규 4) — `Viewer.PlaybackMath`(프레임 순환 8건: 37→0, -1→36, -38→36, 75→1, NumFrames 0/음수 → 0; 속도 clamp 5→2.0, 0/-1→0.1, NaN→1.0; 시간 줄 형식; 프리셋 색), `Viewer.AnimationPlayback`(Manny + MM_Idle: 프로필 없음/AnimBP(`UAnimInstance`)에서 전부 false, +1 → 프레임 1·시간 1/30 s, 프레임 1에서 -2 → 227(= 길이), +1 → 0, 속도 5 → 2.0 / 0 → 0.1, 일시정지·0.5배가 다른 애니메이션 선택 후 유지되고 새 애니메이션은 프레임 0, 재개 상태도 유지, Pose = 정지·PoseTime 0.5 s·재개 거부·프레임 이동 가능, Controller의 P/=/0/[ ] 경로(= 10번 → 2.0에서 멈춤), 패널 시간 줄 = `FormatPlaybackTime` 결과, 다음 프레임 이동 후 `UpdatePlaybackTimeText()`만으로 `frame 4 / 227`, 버튼 글자 `Resume (P)`/`Rate 1.00 (0)`, 프로필 재적용 시 재생·1.0배, AnimBP 패널 문구), `Viewer.ForcedLOD`(LOD 3개, LOD0 92,178 > LOD1 26,998 > LOD2 12,998, `SetForcedLOD(3)` → 컴포넌트 3·기본 통계 LODIndex 2·슬롯 합 = 12,998, `GetSlotStats(0)` 합 = 92,178, 4/-1 거부, INSPECTION 본문 `Triangles 12,998`, Controller 순환 0→1→2→3→0, 패널 글자, 프로필 재적용 시 Auto·LOD0 수치), `Viewer.BackdropPresets`(Actor 없는 월드: 크래시 없음·프리셋 기록·대상 0·순환; 배경 = `MI_StudioBackdrop` 부모 Dynamic MI(그대로 재사용), 바닥 = `MI_StudioFloor`(Dynamic MI 새로 생성, 부모 확인) → 대상 2, Black/White/MidGrey에서 4개 파라미터 값, 프로필 전환·Clean View 후 유지, Studio에서 원래 값 TopColor (0.006, 0.0065, 0.008) / BottomColor (0.060, 0.063, 0.068) / BaseColor 0.18 / EdgeColor (0.035, 0.035, 0.038) 복원, MI 에셋 값 불변, 패널 글자). `LV_Portfolio` 읽기 전용 확인은 4절 표. **화면·실제 키 입력은 미검증**(4.1절) — `-game` 창을 띄우지 않았다.

### 6.19 일괄 포트폴리오 촬영·턴테이블 영상 (2026-10-01)

**목적**: 구도·Variant마다 손으로 F12를 누르던 일을 명령 하나(`Tools\CaptureAll.bat` / 콘솔 `Viewer.CaptureAll`)로, Shift+F12 프레임을 영상으로 바꾸던 ffmpeg 명령을 `Tools\MakeTurntableVideo.bat`으로 바꿨다(사용법 1.8절). Content는 바꾸지 않았다.

- **`CharacterViewer/ViewerBatchCapture.h/.cpp`**(렌더링 없음): `BuildBatchPlan(Profile, Options)` → `FViewerBatchShot` 배열(프리셋 → Variant → 표정 순 데카르트 곱, `poses=1`은 `bIsPose` 항목마다 기본 프리셋·첫 Variant로 1장, 이름은 `ViewerCapture::SanitizeToken` + 중복 시 `_2`), `MakeBatchFolderName`, `ParseConsoleArgs`. `FViewerBatchCaptureRunner`는 `IViewerBatchCaptureHost` 인터페이스만 호출하는 상태 머신(`ShowingProfile → ProfileSettling → ApplyingShot → ShotSettling → Capturing → … → Finished|Cancelled|Failed`)이고 모든 계획을 시작 시 만들어 `Batch 3/12`의 전체 수를 미리 안다. 실패한 샷은 세고 다음으로 넘어가며, 한 장도 못 쓰면 `Failed`.
- **`CharacterViewer/CharacterViewerControllerBatch.cpp`**: 실제 호스트 `FViewerBatchControllerHost`(컨트롤러의 friend) — 선택은 기존 `Select*` 함수, 촬영은 기존 F12 경로(`BeginCapture(Single, 경로, 1)`: UI 없음·고해상도·Slate 폴백 그대로), 상태 줄은 `SetCaptureStatus`. 프로필 전환은 `SwitchProfile()`(그 안의 `CancelCapture()`가 일괄 촬영을 끊지 않도록 러너의 `IsSwitchingProfile()`로 구분), 셰이더 대기는 `GShaderCompilingManager->GetNumRemainingJobs()`, 샷 직전 `IStreamingManager::StreamAllResources(2초)`. 끝/취소 시 이전 선택 복원, `quit=1`이면 `FPlatformMisc::RequestExit(false)`(명령이 시작조차 못 해도 종료해서 .bat이 멈추지 않음). 콘솔 명령은 같은 파일의 `FAutoConsoleCommandWithWorldAndArgs` 하나로 등록한다.
- **`ACharacterViewerController` 변경은 최소**: 헤더에 `StartBatchCapture`/`IsBatchCapturing`/`GetBatchRunner` + 멤버 `BatchRunner`, cpp는 `PlayerTick`의 `BatchRunner.Tick`, `CancelCapture()` 첫머리(Esc = 일괄 취소), `EndPlay`(복원 없이 중지) 각 1곳, 그리고 Shift+F12 완료 문구 끝에 `- video: Tools\MakeTurntableVideo.bat`.
- **.bat**: `CaptureAll.bat`은 `_Common.bat` 규칙(`--check`/`--no-pause`/`--720p`, `UE_ROOT`, `%~dp0..`)을 따르고 자기 옵션(`--all`/`--expressions`/`--poses`)은 먼저 걸러 낸다. `start "" /wait`로 창이 닫힐 때까지 기다리고, 실행 전 `Batch_*` 목록과 비교해 새 폴더만 센다. 명세의 명령줄에 `"-abslog=Saved\Logs\CaptureAll.log"`를 더했다(요약을 그 로그에서 읽기 위해). `MakeTurntableVideo.bat`은 엔진이 필요 없으므로 `_Common.bat`을 부르지 않고 같은 규칙만 따른다. MP4 명령에는 1.7절과 같은 짝수 해상도 보정 필터를 넣었고, GIF 팔레트 단계에 `-update 1`을 넣어 ffmpeg 9의 단일 이미지 경고를 없앴다.
- **ffmpeg**: 이 PC에는 `Gyan.FFmpeg` 9.0.1이 winget 사용자 범위로 이미 설치되어 PATH에 있었다(`%LOCALAPPDATA%\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0.1-full_build\bin`). 그래서 이번에는 winget 설치를 실행하지 않았다.

**`--check` 출력**(`<UE>` = `C:\Program Files\Epic Games\UE_5.6`, `<P>` = 프로젝트 루트):

| 명령 | 출력 |
| --- | --- |
| `CaptureAll.bat --check` | `console : Viewer.CaptureAll profile=current expressions=0 poses=0 quit=1` / `start "" /wait "<UE>\Engine\Binaries\Win64\UnrealEditor.exe" "<P>\CharacterShowcase.uproject" /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=1920 -ResY=1080 -WinX=0 -WinY=0 "-ExecCmds=Viewer.CaptureAll profile=current expressions=0 poses=0 quit=1" -log "-abslog=<P>\Saved\Logs\CaptureAll.log"` |
| `CaptureAll.bat --all --expressions --poses --720p --check` | `... -ResX=1280 -ResY=720 ... "-ExecCmds=Viewer.CaptureAll profile=all expressions=1 poses=1 quit=1" ...` |
| `MakeTurntableVideo.bat <폴더> --gif --check` | MP4 `ffmpeg -hide_banner -loglevel warning -y -framerate 12 -i "<폴더>\frame_%03d.png" -vf "scale=trunc(iw/2)*2:trunc(ih/2)*2" -c:v libx264 -pix_fmt yuv420p -crf 18 "<폴더>\turntable.mp4"`, 팔레트 `-vf "scale=640:-1:flags=lanczos,palettegen" -update 1 "<폴더>\turntable_palette.png"`, GIF `-i ... -i <팔레트> -lavfi "scale=640:-1:flags=lanczos[x];[x][1:v]paletteuse" -loop 0 "<폴더>\turntable.gif"` |
| `MakeTurntableVideo.bat --check`(이 worktree, 턴테이블 폴더 없음) | `[ERROR] No Turntable_* folder in <P>\Saved\Screenshots\Portfolio.` + 촬영 방법 안내, 종료 코드 1 |

**검증**: 4절 표(Editor/Game 빌드 0/0, Editor Automation 22/22, MakeTurntableVideo 실제 실행). `-game` 테스트 `CharacterShowcase.Game.ViewerBatchCapture`는 작성만 했고 실행하지 않았다(사용자가 이 PC를 쓰는 중이라 게임 창을 띄우지 않음, 4.1절).

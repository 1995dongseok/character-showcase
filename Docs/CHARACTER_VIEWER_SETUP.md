# Character Portfolio Viewer — 아티스트 인계 문서

이 프로젝트는 UE 5.6.1 기반 3D 캐릭터 포트폴리오 뷰어다. 캐릭터가 중심이며 C++/Blueprint 기능은 관찰과 촬영을 돕는다.
이 문서는 **현재 상태를 앞에, 과거 기록을 뒤에** 둔다. 0~5절만 읽으면 아티스트 작업에 충분하다. 6절은 참고용 개발 이력이다.

## 0. 현재 상태 요약 (2026-10-01)

- **구현 완료(코드)**: P0(카메라/입력/GameMode/기본 UI), P1(카메라 프리셋, Turntable, Animation/Pose, Morph 표정, Slot 재질, Clean View), P2(Inspection 파츠 선택, 선택 강조, Wireframe) 전부 C++로 구현되어 있다. 아티스트는 **C++를 수정하지 않고** 새 캐릭터를 등록할 수 있다(2절).
- **에셋 자동 생성**: `Scripts/CreatePortfolioAssets.py`가 없는 에셋만 만들고, 이미 있는 에셋은 절대 덮어쓰지 않는다(읽기 전용 검증만 함, `[keep] ... OK/DIFFERS`). `Scripts/CreateViewerWidgetLayout.py`는 `WBP_CharacterViewer`에 디자이너 트리가 없을 때만 최소 트리를 만든다. **일상적인 캐릭터 등록(2절)에는 두 스크립트 모두 다시 실행할 필요가 없다** — Editor GUI에서 Data Asset/Blueprint/Level을 직접 편집하면 된다(1절 "언제 스크립트를 실행하지 않는가" 참고).
- **검증된 것(숫자 있음, 4절 표)**: Editor 빌드 0오류/0경고, Game 빌드 0오류/0경고, Editor Automation 6/6 통과(2026-09-29), Win64 Development 패키지 빌드/실행 스모크 통과, Win64 Shipping 패키지 빌드 성공 + 프로세스 정상 기동/종료 확인(자동화 테스트 미포함), 두 Python 스크립트의 "기존 자산 보존" 동작을 해시 비교로 검증.
- **2026-09-30 추가 검증(4절 표)**: `-game` 합성 포인터 스모크가 활성 전면 창에서 1/1 통과(이전 실패 3건 해소). Shipping 패키지에 OS 수준 실제 마우스/키보드 입력(SendInput)을 넣어 드래그 Orbit, 휠 Zoom, R, Space와 드래그 정지, H, I/파츠 클릭/빈 공간 해제, W, 패널 위 휠·드래그 차단, 포커스 상실 복귀를 화면 캡처 25장으로 확인. 증거는 `Docs/Evidence/2026-09-30-shipping-real-input/`.
- **2026-10-01 검증(4절 표)**: `CreatePortfolioAssets.py` 신규 5개 생성 + 재실행 `[keep] OK` ×12, `-game` 스모크 2회 — 커서가 게임 창 밖이라 합성 포인터 전제 조건 1건으로 Fail, 나머지 assertion 오류 0(Manny Torso 클릭 포함), 화면 측정으로 바닥 중간 회색·가슴 클리핑 0 확인. 비쿠킹 실행이라 Manny의 강조/Wireframe 셰이더는 화면에 아직 안 나왔다(4.1절).
- **미검증/대기(4.1절)**: 사람이 손으로 직접 조작한 확인(자동 입력 재생과 구분), 패널 버튼 클릭 자체의 실제 입력 확인(키보드 경로로만 확인), 표정(Expression)의 실제 시각 검증(현재 캐릭터에 Morph Target이 없음), 사람이 만든 디자이너 WBP 레이아웃에서의 hover 동작, 파츠 단위(부분) 강조 표시.
- **기본 프로필 = `DA_Character_Manny` (2026-10-01)**: 엔진 3인칭 템플릿 마네킹 `SKM_Manny_Simple`(본 89개, LOD0 92,178 삼각형, Material Slot 2개 `M_HeadLegs`/`M_Torso`, 텍스처 1024²·Torso 노멀만 4096², Physics Asset `PA_Mannequin`, Morph 0)을 쓰는 placeholder 프로필이다. 시작 시 Idle(`MM_Idle`)로 서 있고 Full Body 구도로 보인다. `ProfileLibrary` 순서는 Manny → Tutorial Mannequin(`DA_Character`) → Skeletal Cube(`DA_Character_Cube`)이며, 기존 두 프로필은 변경 없이 남아 있다(2절 ⑧).
- **스튜디오 룩 (2026-10-01)**: `LV_Portfolio`는 Key/Fill/Rim 3점 조명(전부 Directional, Movable), 고정 노출 `StudioPostProcess`(Manual, 보정 0), 어두운 그라데이션 배경 구(`MI_StudioBackdrop`), 18% 중간 회색 바닥(`MI_StudioFloor`, 멀어질수록 배경으로 페이드)으로 바뀌었다. 배경/바닥은 NoCollision이라 파츠 클릭을 가로채지 않는다. 값과 조정 위치는 2절 ⑨, 변경 이력은 6.13절.
- **현재 파츠 강조는 메시 전체에 적용된다.** Custom Depth와 Overlay Material은 둘 다 Component 단위로 적용되므로, 어느 파츠를 클릭해도 SkeletalMeshComponent 전체가 강조된다. 선택된 파츠 자체는 INSPECTION 패널의 텍스트로만 구분된다(2절 ⑦, 6.9절).
- **placeholder 데이터 주의**: `DA_Character_Manny`가 참조하는 `SKM_Manny_Simple`(파츠별 삼각형 9,206~25,680, 6.13절의 측정 방법), `DA_Character`가 참조하는 `TutorialTPP`(6,118 삼각형, Material Slot 1개, 텍스처 0개), `DA_Character_Cube`가 참조하는 `SkeletalCube`(12 삼각형)는 전부 UE 엔진/템플릿이 기본 제공하는 에셋이다. **이 수치는 실제 캐릭터 정보가 아니며**, 실제 아트가 들어오면 각 Part의 `Triangle Count`/`Material Name`/`Texture Resolution`을 그 아트 기준으로 다시 측정해 입력해야 한다.
- 어디를 보면 되는지: 실행 명령 → 1절, 캐릭터 등록 절차 → 2절, 책임 분리 규칙 → 3절, 검증 수치 전체 → 4절, 남은 위험 → 5절, 과거 실패/원인 분석 상세 기록 → 6절.

## 1. 실행 방법

### 1.1 Editor에서 열기

`CharacterShowcase.uproject`를 더블클릭하거나 `UnrealEditor.exe`에 직접 넘긴다(설치 경로: `C:\Program Files\Epic Games\UE_5.6`).

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" `
    "C:\Users\WINCARD1\Downloads\develop\project\character-showcase\CharacterShowcase.uproject"
```

`EditorStartupMap`이 `LV_Portfolio`로 설정되어 있어 Editor가 이 레벨을 자동으로 연다.

### 1.2 PIE (Play In Editor)

Editor 툴바의 **Play** 버튼으로 사람이 직접 확인한 기록은 아직 없다(4.1절 — 지금까지의 실행 검증은 전부 `-game`/패키지 프로세스와 Editor Automation으로 이루어졌다). 아티스트는 캐릭터를 등록한 뒤 Play로 직접 눌러 확인하는 것을 권장한다.

### 1.3 `-game` 커맨드 (렌더링 실확인, NullRHI 아님)

```powershell
& "C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" `
    "C:\Users\WINCARD1\Downloads\develop\project\character-showcase\CharacterShowcase.uproject" `
    /Game/Portfolio/Maps/LV_Portfolio -game -windowed -ResX=1280 -ResY=720 -log -unattended -nosplash
```

**주의**: 자동화 테스트가 포함된 스모크(`CharacterShowcase.Game.ViewerSmoke`)의 합성 마우스/키보드 단계는 **게임 창이 활성·비가려짐 포그라운드 창일 때만** 정확히 동작한다. 엔진 동작상 앱이 비활성 상태면 Slate 자체 커서가 hover를 지우고, 창이 배경으로 밀려 있으면 Win32가 마우스 캡처를 거부한다. 최소화/숨김 창으로 실행하지 말 것. 6.10절에 기록된 원인으로 스크린샷/합성 입력이 모두 실패한다. 스모크는 `FSlateApplication::SetCursorPos`로 실제 OS 커서를 움직이므로 실행 중(약 12분) PC를 조작하지 않는다. 2026-09-30 이 조건에서 1/1 통과했다.

실제 입력 재생 검증(Shipping 등 자동화 테스트가 없는 빌드): `Docs/Evidence/2026-09-30-shipping-real-input/ViewerInputDriver.ps1`이 user32 `SendInput`으로 실제 마우스/키보드 이벤트를 보내고 단계마다 창을 캡처한다. 게임 창이 전면 창임을 확인한 뒤에만 입력을 보내며, 아니면 중단한다. 실행 중 PC를 조작하지 않는다. 이 PC는 약 5 FPS라 키 입력은 400 ms 이상 누르고 드래그는 한 단계 250 ms 간격으로 보내야 엔진이 인식한다(60 ms 탭은 프레임 사이에 묻힘).

### 1.4 빌드/테스트 명령

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

Automation 테스트(`CharacterShowcase.Profile.NullSafety`, `CharacterShowcase.Viewer.*`):

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $proj -unattended -nop4 -nosound -NullRHI `
    '-ExecCmds=Automation RunTests CharacterShowcase' '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Saved\Automation"
```

### 1.5 패키지 명령 (Development / Shipping)

```powershell
& "$ueRoot\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$proj" `
    -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive `
    -archivedirectory="C:\Users\WINCARD1\Downloads\develop\project\character-showcase\Saved\Packaged" `
    -unattended -noP4 -utf8output
```

Shipping은 `-clientconfig=Shipping`으로 동일하게 실행한다. Shipping 빌드는 `WITH_DEV_AUTOMATION_TESTS`가 꺼져 있어 자동화 테스트로 검증할 수 없다 — 배포 전 사람이 직접 화면을 확인해야 한다(4.1절).

### 1.6 두 Python 스크립트 — 언제 실행하고, 언제 실행하지 않는가

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

- **`CreatePortfolioAssets.py`**는 `DA_Character`, `DA_Character_Cube`, `DA_Character_Manny`, `BP_CharacterViewerGameMode`, `WBP_CharacterViewer`, `LV_Portfolio`, `M_Wireframe`, `M_ViewerHighlight`, `M_StudioBackdrop`, `MI_StudioBackdrop`, `M_StudioFloor`, `MI_StudioFloor` **12개가 존재하지 않을 때만** 새로 만든다(2026-10-01에 뒤의 5개 추가). 이미 있으면 절대 덮어쓰지 않고 `[keep] <경로> OK` 또는 `[keep] <경로> DIFFERS: <내용>` 한 줄만 출력한다.
- **일상적인 캐릭터 등록(2절)에는 이 스크립트를 다시 실행할 필요가 없다.** 새 캐릭터는 새 `CharacterProfileData` Data Asset을 Editor GUI로 직접 만들고 `ProfileLibrary`에 추가하면 된다 — 스크립트는 "프로젝트 최초 세팅 / 필수 에셋 12개 중 일부가 삭제되어 없어졌을 때"에만 쓴다. `LV_Portfolio`가 없어서 새로 만들 때는 스튜디오 조명/배경/PostProcess(`apply_studio_setup()`)까지 함께 만든다.
- 스크립트가 만든 에셋을 최신 생성 로직으로 다시 만들고 싶을 때만, 그 에셋을 Editor에서 직접 삭제한 뒤 재실행한다(스크립트가 "없는 에셋"으로 인식해 새로 만든다). 기존 값을 스크립트로 되돌리는 용도로 쓰지 않는다.
- **`CreateViewerWidgetLayout.py`**는 `WBP_CharacterViewer`(`widget_tree.root_widget`)가 비어 있을 때만 최소 7위젯 트리를 만든다. 이미 트리가 있으면(사람이 디자이너에서 편집했거나 이전에 생성됐으면) `[keep] ... not modified.`만 출력하고 아무것도 바꾸지 않는다. **디자이너에서 스타일을 다듬은 뒤에는 이 스크립트를 다시 실행해도 안전하지만 실행할 이유가 없다.**

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

`Part` 카테고리의 **Parts** 배열(`FViewerPartInfo`)에 행을 추가한다. 각 행: **Id**, **Display Name**, **Part Type**, **Description**, **Bone Names**(배열), **Component Tag**, **Triangle Count**, **Material Name**, **Texture Resolution**.

- 현재 구조(단일 `SkeletalMeshComponent`)에서는 **Bone Names**로 파츠를 식별한다. 클릭 지점의 `BoneName`이 어느 Part의 Bone Names와도 정확히 일치하지 않으면 부모 본을 최대 10단계까지 걸어 올라가며 다시 찾는다(예: 손가락 본 → `hand_l` → "왼팔"). 여기 적는 본 이름은 **메시의 Physics Asset에 실제로 존재하는 본 이름과 정확히 같아야** 하며, 파츠 클릭이 되려면 **메시에 Physics Asset이 할당되어 있어야 한다**(Physics Asset이 없으면 그 캐릭터의 파츠는 클릭되지 않는다 — 지금의 `DA_Character_Cube`가 이 경우다).
- 파츠가 별도 Component(예: Face/Hair/Jacket이 각각 다른 SkeletalMeshComponent)로 구성된 캐릭터라면 **Component Tag**로 식별 방식을 바꿀 수 있다(스키마는 이미 지원, 현재 placeholder는 미사용).
- **Triangle Count / Material Name / Texture Resolution은 매 프레임 계산하는 값이 아니라 아티스트가 한 번 측정해서 적어 넣는 authored 데이터다.** 실제 캐릭터가 파츠별로 별도 Material Slot/섹션을 가지면 파츠마다 다른 값을 적어야 한다. `DA_Character`(`TutorialTPP`)는 Material Slot이 1개뿐이라 6개 Part 모두 메시 전체 수치(6,118 삼각형)를 그대로 공유한다. 기본 프로필 `DA_Character_Manny`는 파츠별로 측정한 값(Head 9,206 / Torso 25,680 / 팔 각 19,680 / 다리 각 8,966, 합계 92,178 = LOD0 전체)과 슬롯별 비율(예: 팔은 `MI_Manny_02_New` 80% + `MI_Manny_01_New` 19%)을 갖는다(측정 방법 6.13절) — **어느 쪽이든 이 숫자를 실제 캐릭터 스펙으로 착각하지 않는다.**
- **파츠 강조(선택 시 색이 덮이는 효과)는 현재 메시 전체에 적용된다.** 어느 파츠를 클릭해도 Custom Depth와 Overlay Material이 전체 메시에 걸리므로, 실제로 어느 파츠가 선택됐는지는 INSPECTION 패널의 텍스트(Display Name 등)로만 알 수 있다. 파츠 단위로만 강조하려면 별도 Component 구조이거나 Stencil 기반 Post Process 작업이 추가로 필요하다(5절).

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
- `WBP_CharacterViewer`를 열어 스타일을 다듬을 경우 **PanelRoot / NameText / ControlsBox / DescriptionScroll / DescriptionText / ListsScroll / ListsBox** 7개 이름과 각각의 "Is Variable" 체크를 그대로 유지해야 한다. 이 이름이 바뀌거나 사라지면 C++가 더 이상 찾지 못해 자동으로 내장 폴백 패널로 전환된다(안전하지만 디자이너가 만든 스타일은 사라진다). **`ListsScroll`의 부모(VerticalBox) 슬롯 Size는 반드시 `Fill`로 유지한다** — `Auto`(또는 슬롯 크기를 만지지 않은 기본값)로 바꾸면 제한된 높이를 잃어 목록이 스크롤되지 않고 화면 밖으로 흘러넘친다.
- 1.2~1.3절의 방법으로 실행해 캐릭터가 보이는지, 우측 패널에 새 캐릭터 이름/목록이 뜨는지 확인한다.

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
- Clean View(H)는 UI/선택 강조/커서를 숨기되 Orbit/Zoom/Space/H는 유지한다. Wireframe은 Variant보다 화면에서 우선한다(끄면 현재 Variant로 정확히 복원).

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
| `CreatePortfolioAssets.py` 스튜디오/Manny 추가 후 1차 실행(`UnrealEditor-Cmd -ExecutePythonScript ... -NullRHI`) | 2026-10-01 | 종료 코드 0, `==== DONE, NO ERRORS ====`. **신규 생성 5개**(`M_StudioBackdrop`, `MI_StudioBackdrop`, `M_StudioFloor`, `MI_StudioFloor`, `DA_Character_Manny`) + 기존 7개 `[keep] ... OK` ×7 + `DefaultEngine.ini` OK | 6.13절 |
| 1회성 in-place 편집(`LV_Portfolio` 스튜디오 Actor·Profile, GameMode 기본 프로필, MI 값 조정) | 2026-10-01 | 종료 코드 0, 저장 후 다시 읽은 값이 2절 ⑧/⑨ 표와 일치 | 6.13절 |
| `CreatePortfolioAssets.py` 2차·3차 실행(편집 후) | 2026-10-01 | 둘 다 종료 코드 0, **`[keep] ... OK` ×12** + ini OK, DIFFERS 0. 실행 전후 에셋 해시 2차 11/11, 3차 12/12 동일(아무것도 다시 저장하지 않음) | 6.13절 |
| `-game` 스모크(`CharacterShowcase.Game.ViewerSmoke`), Manny 기본 프로필 + 스튜디오 룩 | 2026-10-01 | 2회 실행, 둘 다 **Fail, 오류 1건(같은 내용)**: 합성 포인터 전제 조건 `IsActive()=1, IsCursorDirectlyOverSlateWindow()=0` — 실행 중 OS 커서가 게임 창 밖에 있었음(6.10절의 환경 조건, 커서를 움직이지 않는 규칙으로 실행). 그 외 assertion 오류 0, 경고 0 — Manny에서 Torso 클릭(액터+120 cm 지점 → `Torso`), 빈 공간 클릭 해제, Grid/Default 재질, Idle/Pose, Wireframe 토글, 프로필 전환(Cube → `DA_Character`) 포함. 스크린샷 6장 성공 | `Saved/Automation/AgentC/index.json`, `Saved/Automation/AgentC2/index.json`, `Saved/Screenshots/WindowsEditor/ViewerSmoke_*.png` |
| 스튜디오 룩 화면 측정(위 스모크 2차 `ViewerSmoke_UI.png`, 1280×720) | 2026-10-01 | 바닥(캐릭터 앞) sRGB 평균 120(18% 회색 목표 118), Manny 가슴 영역 27,200픽셀 중 ≥250 **0개**, 캐릭터 전체 136,500픽셀 중 ≥250 6개(스페큘러 반짝임), 배경 지평선 48~53 / 위쪽 25~29, 먼 바닥 71 | 6.13절 |

### 4.1 미검증·대기 항목

- **사람이 손으로 직접 조작한 확인**: 2026-09-30 검증은 OS 수준 실제 입력 이벤트(SendInput)를 자동 재생한 것이다. 게임 입장에서는 실제 마우스/키보드와 구분되지 않지만, 사람이 손으로 조작한 기록은 아직 없다. 패널 버튼을 마우스로 직접 클릭하는 경로는 키보드 단축키 경로로만 확인했다(합성 Slate 클릭 테스트는 `-game` 스모크에서 통과). **대기(선택)**.
- **Expression(표정)의 실제 시각 검증**: `TutorialTPP`/`SkeletalCube` 모두 Morph Target이 없다. Expression 스키마/무충돌만 확인됐고, 실제 Morph 적용 후 얼굴이 바뀌는 모습은 **미검증**이며 Morph가 있는 메시가 들어오기 전까지는 검증할 수 없다.
- **사람이 만든 디자이너 WBP 레이아웃의 hover 동작**: 지금 존재하는 `WBP_CharacterViewer` 트리는 C++ 에디터 툴이 자동 생성한 것이다. 아티스트가 디자이너에서 직접 커스터마이즈한 레이아웃에서 `IsPointerOverPanel()`/패널 클릭 소비가 그대로 동작하는지는 **미검증**.
- **파츠 단위(개별) 강조 표시**: 현재 메시 전체 강조만 구현되어 있다(0절/2절 ⑦). 파츠별 강조는 구현되어 있지 않다.
- **Manny에서 선택 강조/Wireframe의 화면 표시(비쿠킹 `-game`)**: 2026-10-01 스모크 2회 모두 `Preparing Shaders (1~2)`가 떠 있어 `ViewerSmoke_Inspect.png`에 마젠타 강조가 안 보이고 `ViewerSmoke_Wireframe.png`는 청록 와이어 대신 회색 체커(셰이더 준비 중 대체 재질)로 찍혔다. 값 자체는 테스트가 확인했다(오류 0). 6.10절과 같은 비쿠킹 셰이더 지연으로 보이며 쿡된 패키지에서 Manny로는 아직 확인하지 않았다. 같은 스모크의 `ViewerSmoke_Profile1.png`(TutorialTPP)도 노란 재질 대신 어두운 회색으로 찍혔다(같은 대체 재질로 추정, 미확인).
- **새 레벨 생성 경로**: `LV_Portfolio`가 없을 때 스크립트가 만드는 경로(`create_or_update_level()` + `apply_studio_setup()`)는 실행하지 않았다. `apply_studio_setup()` 자체는 기존 레벨 편집에서 실행·검증됐다.
- **패키지(Development/Shipping) 재빌드·실행**: 스튜디오 룩/Manny 기본 프로필 상태로는 하지 않았다.
- **사용자 GUI PIE**: Editor 툴바의 Play 버튼을 사람이 직접 눌러 확인한 기록이 없다(전부 `-game`/패키지/Automation으로 대체 검증).

## 5. 남은 작업·위험

- `ApplyProfile` 순서(`PostLogin`이 `BeginPlay`보다 먼저 실행됨)에 맞춘 방어 코드는 정적으로만 점검했고, 이 UE 버전의 PIE로 최종 확인하지 않았다.
- `SetAnimation`의 스켈레톤 호환성 검사는 포인터 비교(`Sequence->GetSkeleton() == Mesh->GetSkeletalMeshAsset()->GetSkeleton()`)만 사용한다. 리타겟/호환 스켈레톤 조합에서 지나치게 엄격할 수 있다.
- 파츠별 Triangle Count/Material Name은 현재 메시가 단일 Material Slot이라 전부 동일한 값을 공유한다(2절 ⑦). 실제 캐릭터가 파츠별 섹션을 가지면 재측정이 필요하다.
- `Config/DefaultGame.ini`의 `ProjectID`는 실제 GUID가 아닌 placeholder 문자열이다.
- `-game` 스모크의 합성 포인터 단계는 게임 창이 활성·비가려짐 전면 창일 때만 유효하다(원인 확정, 6.10절). 무인 실행 환경에서는 이 조건을 보장할 수 없으므로 실패 시 전제 조건 오류 메시지를 먼저 확인한다.
- 파츠 단위 강조가 필요하면 별도 Component 구조 또는 Custom Stencil 기반 Post Process Material 작업이 추가로 필요하다.
- `Scripts/CreatePortfolioAssets.py`의 예전 "레벨 재생성" 로직이 유발하던 간헐적 크래시는 원인을 특정해 제거했지만(6.10절), 대규모 반복 재현 테스트는 하지 않았다.

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

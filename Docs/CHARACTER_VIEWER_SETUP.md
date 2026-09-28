# Character Portfolio Viewer — 실행 및 Claude Code 작업 계획

이 프로젝트는 UE5 기반 3D 캐릭터 포트폴리오 뷰어다. 캐릭터가 중심이며 개발 기능은 관찰과 촬영을 돕는다.
이 문서는 현재 최소 구현과 이후 구현 계획을 구분한다. 아래 미완료 항목은 구현된 기능이 아니다.

## 1. 현재 인계 상태

- 현재 구현: C++ 프로젝트/모듈, Character Profile Data Asset, 프로필의 Skeletal Mesh를 적용하는 Actor, null 전환 테스트 소스.
- `UCharacterProfileData`의 현재 필드: `DisplayName`, `Description`, `SkeletalMesh`뿐이다.
- `APortfolioCharacterActor`: `Mesh` 컴포넌트, 편집 가능한 `Profile`, `ApplyProfile(NewProfile)`, BeginPlay 시 프로필 적용. Tick은 사용하지 않는다.
- 프로필이 없거나 프로필의 Mesh가 비어 있으면 기존 Mesh를 제거한다. 이름과 설명은 데이터로만 보관하며 UI는 아직 없다.
- 미구현: 카메라, 입력, GameMode, UMG, Turntable, 표정, 애니메이션 선택, 재질 선택, Inspection, Wireframe, Clean View.
- `.uasset`, `.umap`, 캐릭터/애니메이션/재질 에셋은 포함하지 않는다. P0는 기반 일부만 작성된 상태다.
- UE/Visual Studio의 설치·버전·빌드 가능 여부는 미확정이다. 정규 경로에서 찾지 못한 사실만으로 미설치를 단정하지 않는다.
- 현재 인계에는 UE 컴파일, Automation 실행, 화면 표시 검증 결과가 없다. 소스 작성과 실행 성공을 구분한다.
- 프로젝트 JSON, 모듈/타깃 이름, generated header include 순서, UTF-8 정적 검사는 통과했다. 소스 검토에서 큰 문제는 발견하지 못했으나 컴파일 검증을 대신하지 않는다.
- 최초 작업 폴더는 Git 저장소가 아니었다. 다음 작업 시작 시 현재 Git 상태를 다시 확인한다.

### 1.1 2026-09-28 상태 재확인 결과 (분석만 수행, 파일 변경 없음)

- 파일 구성은 위 표와 정확히 일치한다. 소스 12개 파일 외에 추가된 것은 없다.
- `Content/`, `Config/`, `Plugins/` 폴더가 없다. `.uasset`/`.umap`/아트 에셋은 0개이며, 보존 대상은 소스 파일뿐이다.
- `Config/DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`가 없다. P0-6의 GameMode/Default Map과 Enhanced Input 기본 클래스 설정은 이 파일들을 새로 만들어야 한다.
- 이 폴더와 상위 폴더 모두 Git 저장소가 아니다. `C:\Users\WINCARD1\.git`가 빈 폴더로 존재하여 일부 도구가 홈 디렉터리를 저장소로 오인하지만(branch가 `HEAD`로 표시됨), git 자체는 저장소로 인식하지 않는다. 이 빈 폴더는 이 프로젝트와 무관하며 건드리지 않는다.
- 이 PC에는 UE, Epic Games Launcher, Visual Studio, MSVC, Windows SDK, .NET SDK가 모두 없다. 자세한 확인 범위는 2.1절, Git/도구 상태는 11.1절에 있다.
- 따라서 P0-0(Editor 타깃 빌드 + NullSafety 테스트 통과)은 미달성이며, 빌드/테스트/화면 표시는 여전히 실행 미검증이다.
- 소스 정적 검토 추가 사항: 테스트의 `GetSkeletalMeshAsset()`와 `TObjectPtr` 사용으로 코드는 UE 5.1 이상을 전제한다. 5.5 이후 `EAutomationTestFlags`가 enum class로 바뀌었으나 현재 `EditorContext | EngineFilter` 조합은 실제 버전에서 컴파일 확인이 필요하다. `Build.cs` 의존 모듈은 Core/CoreUObject/Engine뿐이므로 P0 구현 시 `EnhancedInput`, `InputCore`, `UMG`, `Slate`, `SlateCore` 추가가 필요하다.

| 현재 파일 | 역할 |
| --- | --- |
| `CharacterShowcase.uproject` | Runtime 모듈 등록. `EngineAssociation`은 버전 미확정으로 빈 값 |
| `Source/CharacterShowcase.Target.cs`, `Source/CharacterShowcaseEditor.Target.cs` | Game/Editor 빌드 타깃 |
| `Source/CharacterShowcase/CharacterShowcase.Build.cs`, `CharacterShowcase.cpp` | 모듈 의존성과 시작점 |
| `Source/CharacterShowcase/Character/CharacterProfileData.h` | 현재 3개 필드의 Data Asset |
| `Source/CharacterShowcase/Character/PortfolioCharacterActor.h/.cpp` | 프로필 적용/해제 |
| `Source/CharacterShowcase/Tests/CharacterProfileTests.cpp` | `CharacterShowcase.Profile.NullSafety` 테스트 |
| `.gitignore`, `.gitattributes` | 생성물 제외와 바이너리 LFS 정책 |

## 2. 설치된 엔진으로 먼저 빌드하기

1. 작업 폴더의 파일, `.uproject`, 모듈, Git status/branch/remote, 기존 Content를 다시 확인한다. 기존 변경을 덮어쓰지 않는다.
2. 실제 UE5 설치 경로와 정확한 버전을 확인한다. Visual Studio C++ 게임 개발 도구와 해당 UE 버전이 요구하는 MSVC/Windows SDK도 확인한다.
3. 엔진·도구가 없다면 설치 필요 사항을 보고한다. 에이전트가 엔진/개발 도구를 임의 설치하지 않는다.
4. `.uproject` 우클릭 → **Switch Unreal Engine version**으로 설치된 버전을 연결한다. 소스 빌드 엔진이면 해당 등록 식별자를 사용한다.
5. 두 Target의 `BuildSettingsVersion.Latest`는 임시 선택이다. 엔진 확정 후 그 버전의 C++ 프로젝트 템플릿 설정과 맞추고 재빌드한다.
6. `.uproject` 우클릭 → **Generate Visual Studio project files**. 메뉴가 없으면 아래 UBT 명령을 사용한다.
7. Editor를 닫고 `CharacterShowcaseEditor / Development / Win64`를 빌드한다. 실패 시 해당 오류만 수정한다.

### 2.1 2026-09-28 엔진/도구 확인 결과

확인 범위: 레지스트리(`HKLM\SOFTWARE\EpicGames\Unreal Engine`, `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds`), 설치 프로그램 목록(Uninstall 키), `C:\ProgramData\Epic`, `Program Files`/`Program Files (x86)`, `AppData\Local`, PATH, 그리고 C: 드라이브 전체의 `UE_*`/`UnrealEngine`/`Epic Games` 폴더 및 `UnrealEditor.exe` 검색. 고정 드라이브는 C: 하나뿐이다.

| 항목 | 결과 |
| --- | --- |
| Unreal Engine (Launcher 또는 소스 빌드) | 없음 |
| Epic Games Launcher | 없음 |
| Visual Studio 2019/2022, MSVC, vswhere | 없음 |
| Windows 10/11 SDK | 없음 |
| .NET SDK/Runtime | 없음 |
| 엔진/VS 설치 파일(Downloads) | 없음 |
| 다른 `.uproject` | 이 프로젝트 외 없음 |

- 하드웨어: Intel i5-9600K 6코어, RAM 24GB, GPU Intel UHD 630(내장), C: 여유 175GB. UE5 Editor 실행은 가능하나 내장 GPU라 느리며 Lumen/Nanite는 현실적이지 않다. UE 5.x 약 60GB + VS 2022 게임 개발 워크로드 약 30GB + DDC를 고려하면 디스크는 감당 가능하다.
- 설치는 사용자가 직접 수행한다. 설치 시 권장: UE 5.4~5.6 중 하나(코드가 5.1 이상 API를 전제), VS 2022의 "C++를 사용한 게임 개발" 워크로드(해당 UE 버전이 요구하는 MSVC/Windows SDK 포함), .NET SDK는 UE 설치본에 동봉된 것을 사용.
- 설치 전까지는 아래 UBT/Build.bat 명령과 10절의 Automation 명령을 실행할 수 없다. 이 상태에서 C++를 작성하면 컴파일 미검증으로 기록한다.

프로젝트 루트에서 PowerShell을 실행하고 `$ueRoot`를 실제 설치 경로로 바꾼다. 다음 명령은 아직 실행 검증되지 않았다.

```powershell
$ueRoot = 'C:\Path\To\UE_5_x'
$viewerRoot = (Resolve-Path '.').Path
$viewerProject = Join-Path $viewerRoot 'CharacterShowcase.uproject'
& "$ueRoot\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe" -projectfiles "-project=$viewerProject" -game -engine
& "$ueRoot\Engine\Build\BatchFiles\Build.bat" CharacterShowcaseEditor Win64 Development "-Project=$viewerProject" -WaitMutex
```

각 명령의 성공을 확인한 뒤 다음 명령을 실행한다. UBT 실행 파일 위치가 다른 엔진 버전이면 그 엔진의 실제 경로를 사용한다.
생성된 `.sln`을 사용할 때는 `Development Editor / Win64` 구성을 선택한다. `.sln`, Intermediate 등 생성물은 commit하지 않는다.

## 3. 현재 최소 구현을 Editor에서 확인하기

1. 빌드 성공 후 `CharacterShowcase.uproject`를 연다.
2. Content Browser에 `Portfolio/Characters`, `Portfolio/Data`, `Portfolio/Maps` 폴더를 만든다.
3. 사용 가능한 Skeletal Mesh를 Import하거나 이미 보유한 mannequin/placeholder를 추가한다. 원본 에셋은 수정하지 않는다.
4. `Portfolio/Data`에서 **Miscellaneous → Data Asset → CharacterProfileData**를 선택해 `DA_Character`를 만든다.
5. `Display Name`, `Description`, `Skeletal Mesh`를 지정하고 저장한다.
6. `Portfolio/Maps/LV_Portfolio` 레벨을 만들고 `PortfolioCharacterActor`를 배치한다. Details의 `Profile`에 `DA_Character`를 지정한다.
7. Mesh가 보이도록 Editor에서 조명과 카메라 위치를 조정한다. 조명은 Level에서 편집하며 코드에 고정하지 않는다.
8. **Simulate**로 BeginPlay를 실행하여 Mesh 표시를 확인한다. 현재 프로필 적용은 BeginPlay에서 이루어지므로 편집 뷰포트의 즉시 갱신을 기대하지 않는다.
9. 실행을 종료하고 Profile을 비운 뒤 다시 Simulate하여 Mesh가 없어지는지 확인한다. Mesh가 비어 있는 별도 프로필도 같은 방식으로 확인한다.

현재는 대화형 Viewer 입력과 UI가 없다. 위 절차는 Actor 연결 확인용이다. Blueprint 파생 Actor는 추가 표현 설정이 필요할 때만 만든다.
실제 캐릭터가 없으면 placeholder 사용 사실을 기록하고, 실제 리깅/Morph/재질/Animation 호환성은 미검증으로 남긴다.

## 4. 다음 구현의 책임과 상태 정책

| 책임 | 소유 클래스/위치 | 원칙 |
| --- | --- | --- |
| 시작 설정 | `CharacterViewerGameMode` | Default Profile, 대상 Actor, Pawn/Controller/Widget 연결 |
| 입력·UI 연결 | `CharacterViewerController` | 입력 상태, UI 표시, 드래그/클릭 판정, 기능 호출 |
| 카메라 | `CharacterViewerCameraPawn` | Orbit 중심/각도, Zoom clamp, 프리셋 보간/Reset |
| 캐릭터 상태 | `PortfolioCharacterActor` | Turntable, 표정, Animation, Variant, 선택 파츠 적용/복원 |
| 화면 표현 | `CharacterViewerWidget` + UMG Blueprint | 데이터 목록 표시, 이벤트 전달, 상태 표시 |
| 아트 표현 | Level/Blueprint/Material Instance | 조명, 배경, 플랫폼, 스타일 |

- 별도 Manager/Subsystem은 만들지 않는다. 중복 상태를 UI와 Actor에 각각 저장하지 않는다.
- 처음에는 레벨에 배치한 Viewer Actor 하나를 사용한다. 전용 레벨에 0개/2개 이상이면 명확히 알리고 안전하게 입력을 비활성화한다.
- 시작 GameMode의 Default Profile을 Actor에 적용한다. 캐릭터를 교체하면 이전 선택·Morph·재질 override·Animation 상태를 먼저 정리한다.
- Left press 시 위치를 저장한다. 이동이 설정 임계값을 넘으면 Orbit, 임계값 이하 release만 Inspection 클릭으로 판정한다.
- UMG 위에서 시작한 입력은 Orbit/Trace로 전달하지 않는다. 드래그 종료·포커스 상실 시 캡처/버튼 상태를 해제한다.
- 수동 Orbit 시작 시 Turntable을 정지한다. UI 클릭은 상태를 유지하며 버튼/Space로 다시 켠다.
- 프리셋 보간 중 수동 Orbit/Zoom은 보간을 중단한다. R은 현재 프로필의 Full Body 카메라 구도로 복귀한다.
- H로 Clean View 전환. 모든 Viewer UI/선택 강조/커서를 숨기되 Orbit, Zoom, Space, H는 유지한다. H 재입력으로 이전 UI·커서·선택 표시를 복원한다.
- Widget은 Actor/Pawn 상태를 읽어 표시한다. 레벨 종료 시 바인딩을 해제하고 UObject 참조는 UPROPERTY/적절한 weak 참조로 관리한다.
- Tick은 보간·Turntable 등 실제 갱신이 필요한 동안만 사용한다. 기능 구현 시 필요한 모듈만 Build.cs에 추가한다.

## 5. P0 — 먼저 실행 가능한 기본 Viewer 완성

아래 경로는 `Source/CharacterShowcase/` 기준이다. C++ 파일은 필요 시 `.h/.cpp` 쌍으로 작성한다.

| 순서 | 작업/파일 | 확인 및 완료 조건 |
| --- | --- | --- |
| P0-0 | 엔진 연결, 두 Target/Build.cs 호환성 확인 | Editor 타깃 빌드와 기존 NullSafety 테스트 통과 |
| P0-1 | `CharacterViewer/CharacterViewerGameMode`, `CharacterViewerController` | 기본 Profile을 배치 Actor에 적용하고 누락 시 무충돌 |
| P0-2 | `CharacterViewer/CharacterViewerCameraPawn` | 캐릭터 중심 Orbit, pitch clamp, 거리 clamp, Wheel Zoom, R Reset |
| P0-3 | Profile에 기본 Full Body 구도 설정 추가 | Mesh 크기가 다른 두 프로필로 전신 구도 설정 가능. 중심/거리/FOV 데이터화 |
| P0-4 | Controller에 Enhanced Input 연결 | Left Drag/Wheel/R 동작. UI 위 입력 차단, 포커스 복귀 시 드래그 잔류 없음 |
| P0-5 | `UI/CharacterViewerWidget`, `WBP_CharacterViewer` | 오른쪽 어두운 최소 패널에 Profile 이름/설명 표시, 잘못된 데이터 항목 비활성화 |
| P0-6 | `LV_Portfolio`, `BP_CharacterViewerGameMode`, 입력 에셋 | GameMode와 Default Map 설정 후 Editor 재시작/PIE에서 동일하게 동작 |

Editor에서 `IA_Orbit`, `IA_Zoom`, `IA_ResetCamera`, `IMC_CharacterViewer`를 만든다. 필요한 Input Action의 값 형식과 바인딩을 구현 후 이 문서에 기록한다.
Enhanced Input 플러그인/모듈은 실제 연결 단계에서 추가한다. 이전에 생긴 입력 구조가 있다면 우선 재사용한다.
UMG는 기본 Widget 클래스를 상속하고 캐릭터 이름을 고정 문자열로 쓰지 않는다. 카메라와 데이터 로직을 Blueprint 그래프에 중복 구현하지 않는다.
P0 완료 증거: 빌드 로그, NullSafety 결과, 표시/Orbit/Zoom 양 끝 clamp/Reset/UI 입력 차단의 직접 실행 확인.
P0가 통과하기 전에는 P1/P2 완료를 주장하지 않는다.

## 6. P1 — 포트폴리오 핵심 기능

| 순서 | 변경 대상 | 작업/완료 조건 |
| --- | --- | --- |
| P1-1 | Profile + CameraPawn + Widget | Face/Upper/Full 프리셋 목록과 짧은 보간. 모든 버튼이 데이터에서 생성되고 수동 입력으로 보간 중단 |
| P1-2 | Actor + Controller + Widget | 속도 설정 가능한 Turntable, UI/Space 토글, 수동 드래그 시 정지. 프레임 속도와 무관한 회전 |
| P1-3 | Profile + Actor + Widget | Animation Sequence/정지 Pose 선택. 없는 항목은 숨김/비활성화, 호환 Skeleton 확인, 기본 모드 복원 |
| P1-4 | Profile + Actor + Widget | Morph Target 기반 표정 선택. 이전 표정 제거, Neutral 복원, 없는 Morph 이름 무시/진단 |
| P1-5 | Profile + Actor + Widget | Slot 기반 Material Instance 교체. 부분 override 전 이전 Variant 흔적 제거, Default 복원 |
| P1-6 | Controller + Widget | H/버튼 Clean View, 숨긴 상태에서도 카메라/Turntable 동작, H 복원 |

P1 UI 섹션: VIEW, EXPRESSION, ANIMATION, APPEARANCE, DISPLAY. 데이터가 없는 선택 목록은 숨긴다.
Idle/Walk/Combat/Pose, Neutral/Smile/Angry/Surprised는 예시 이름이며 필수 에셋/고정 버튼이 아니다.
표정의 다른 실행 방식(Montage/Control Rig/Blueprint Event)은 현재 범위에서 구현하지 않는다.
P1 완료 증거: 프로필 2개로 코드 수정 없는 교체, 기능별 실행 확인, 모든 선택 데이터가 빈 경우의 무충돌, Win64 패키지 기본 실행.
실제 에셋이 없어 검증할 수 없는 기능은 명시적으로 미검증 처리한다. P0/P1 안정화 전 P2 개발을 시작하지 않는다.

## 7. P2 — Inspection과 Wireframe

| 순서 | 변경 대상 | 작업/완료 조건 |
| --- | --- | --- |
| P2-0 | 실제 Mesh/Physics Asset 구조 조사 | 분리 Component/Bone/명시적 선택 충돌 영역 중 파츠 구분 방법을 확정 |
| P2-1 | Profile + Controller + Actor | Inspection 토글, 클릭 시 Line Trace, 유효 대상만 선택, 빈 공간 클릭으로 해제 |
| P2-2 | Widget | 파츠 이름/종류/설명/삼각형 수/재질/텍스처 해상도를 데이터에서 표시 |
| P2-3 | Actor + Material/Level 설정 | Custom Depth/Stencil 강조, 선택 해제/Inspection 종료/프로필 교체 시 원상 복구 |
| P2-4 | Actor + Profile/전용 Material | Wireframe 표시, 해제 시 현재 Variant 복원, 대상 Shipping 패키지에서 확인 |

단일 Skeletal Mesh의 Face/Hair/Jacket이 별도 Component인 것처럼 구현하지 않는다. Bone hit도 아티스트가 정한 파츠와 반드시 일치하지 않는다.
추가 선택 충돌은 Viewer용 Blueprint/컴포넌트에 두고 원본 Mesh/Physics Asset을 임의 수정하지 않는다.
`CharacterPartComponent`는 이 구조에서 필요할 때만 추가한다. Component Tag/Bone 매핑만으로 충분하면 새 클래스를 만들지 않는다.
Custom Depth는 컴포넌트 단위이므로 통합 Mesh에 적용하면 전체가 강조될 수 있다. 파츠 단위 표현 가능 여부를 먼저 확인한다.
Wireframe은 `viewmode wireframe` 같은 Editor 전용 동작에 의존하지 않는다. Runtime Material/Overlay를 우선 시험한다.
대상 플랫폼에서 불가능하면 아티스트가 제공하는 topology presentation mesh 등 대안을 검토하고 제한을 기록한다.
삼각형 수/텍스처 해상도를 매 프레임 추론하지 않는다. 해당 LOD/에셋 기준을 포함한 작성 데이터를 표시한다.
P2 완료 증거: 클릭/드래그 충돌 없음, UI 위 선택 차단, 선택/해제 복원, Variant→Wireframe→Variant 정확한 복원, Shipping 실행.

## 8. 향후 Profile 스키마와 에셋 연결

아래 필드는 계획이며 현재 `CharacterProfileData.h`에 아직 없다. 필요한 단계에서 USTRUCT/UPROPERTY로 추가한다.

| 단계/데이터 | 최소 필드 | Editor에서 아티스트가 연결할 것 |
| --- | --- | --- |
| P0 기본 구도 | TargetOffset, Distance, FOV, 거리/각도 제한 | 현재 Mesh의 전신 구도와 줌 범위 |
| P1 CameraPresets | 안정적인 ID, 표시 이름, TargetOffset, Distance, FOV | Face/Upper/Full 설정. Full을 기본/Reset 대상으로 지정 |
| P1 Animations | ID, 표시 이름, Animation Sequence, Loop, 필요 시 Pose 재생 위치 | 같은 Skeleton용 Idle/Walk/Combat/Pose. 정지 Pose는 선택 위치에서 정지 |
| P1 기본 재생 | Default Animation 또는 AnimBP class, 명시적 기본 모드 | 초기 실행/선택 해제 시 돌아갈 재생 상태 |
| P1 Expressions | ID, 표시 이름, Morph 이름/Weight 쌍 목록 | Neutral은 빈 목록, Smile 등은 실제 Morph 이름과 가중치 |
| P1 MaterialVariants | ID, 표시 이름, Slot 이름/Index, Material 쌍 목록 | 유효한 Slot에 맞는 Material Instance. Default는 원래 Mesh 재질 |
| P2 Parts | ID, 표시 이름, Component Tag/Bone/선택 영역 식별자, 기술정보 | 실제 선택 방식과 일치하는 파츠 ID, 설명/삼각형/재질/해상도 |
| P2 Wireframe | 구현 방식에 필요한 Material 또는 presentation mesh | 확정한 Runtime 표시 방식의 에셋 |

현재의 직접 UObject 에셋 참조를 기본으로 유지한다. 비동기 로딩이 실제로 필요할 때만 Soft Reference로 전환한다.
Data Asset, Data Table, Config에 같은 캐릭터 설정을 중복 저장하지 않는다. Blueprint에는 레이아웃과 아트 표현 설정만 둔다.
Profile은 설정 데이터다. 현재 선택/Turntable/재생 시간/카메라 값 같은 실행 상태를 Asset에 기록하지 않는다.

## 9. 구현 후 아티스트 사용 절차

1. 최종 Skeletal Mesh와 필요한 Texture/Material을 `Portfolio/Characters/<캐릭터>`에 Import한다. 원본 제작 파일은 별도 관리한다.
2. `Portfolio/Data`에 CharacterProfileData를 만들고 표시 이름, 설명, Mesh를 지정한다.
3. P1 구현 후 Expressions에 실제 Morph 이름/Weight를 등록한다. Mesh에 없는 Morph는 추가하지 않고 Neutral은 빈 목록으로 둔다.
4. Animations에 호환되는 Sequence를 추가하고 Loop/Pose 위치를 정한다. AnimBP를 쓰면 기본 모드 복원까지 확인한다.
5. MaterialVariants에 표시 이름과 Slot별 Material Instance를 지정한다. 변경하지 않을 Slot은 기본 재질을 유지한다.
6. CameraPresets의 Face/Upper/Full 구도를 해당 캐릭터 키와 중심에 맞춘다.
7. P2 구현 후 Parts의 선택 식별자와 기술정보를 연결하고 각 파츠가 실제 클릭되는지 확인한다.
8. `BP_CharacterViewerGameMode`의 Default Profile을 새 데이터로 변경한다. **이 연결은 P0 GameMode 구현 후 사용 가능하다.**
9. 기본 레벨의 GameMode/Default Map, 카메라, Widget Class, Input Mapping Context를 연결하고 저장한다.
10. PIE와 패키지에서 캐릭터 교체 결과를 확인한다. 새 캐릭터마다 C++를 수정해야 한다면 완료 조건을 충족하지 못한다.

추가 Content 폴더는 필요한 시점에 `Portfolio/UI`, `Materials`, `Animations`, `Input`을 만든다. 빈 구조를 미리 양산하지 않는다.
Neutral Background/Platform/Key·Fill·Rim Light는 Level 또는 Blueprint에서 조절한다. Stability Matrix는 제작 도구이며 Runtime 연동은 하지 않는다.

## 10. 테스트와 구현 함정

기존 테스트는 transient game world에 Actor를 생성하여 null Profile 및 Mesh 없는 Profile 전환을 확인한다.
실제 Skeletal Mesh 렌더링/교체, 리깅, 카메라, UI, 패키지 로딩을 보장하는 테스트가 아니다.
빌드 후 Editor의 Automation/Session Frontend에서 `CharacterShowcase.Profile.NullSafety`를 선택해 실행하거나 다음 명령을 사용한다.

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $viewerProject -unattended -nop4 -nosound -NullRHI '-ExecCmds=Automation RunTests CharacterShowcase.Profile.NullSafety' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$viewerRoot\Saved\Automation"
```

Automation 보고서에서 실제로 해당 테스트가 실행되어 통과했는지 확인한다. 프로세스 종료 코드만으로 통과 판정하지 않는다.
위 명령은 그래픽 출력을 검증하지 않는다. 기능 추가마다 관련 빌드/수동 검증만 수행하며 전체 테스트 프레임워크를 새로 만들지 않는다.

- AnimBP ↔ Animation Sequence: 재생 모드, Anim Class, Loop/Pose 위치를 명시적으로 관리하고 기본 상태로 복원한다.
- Walk/Root Motion: 제자리 재생을 기본 정책으로 삼아 플랫폼 밖으로 이동하거나 카메라 중심에서 벗어나지 않는지 확인한다. 원본 Animation 수정 없이 Viewer 재생 설정으로 처리한다.
- Morph: 선택한 표정이 관리하는 Morph만 초기화한다. AnimBP의 Animation Curve가 같은 Morph를 덮어쓰는지 실제 에셋으로 확인한다.
- Material: Mesh 기본 재질과 현재 선택 Variant를 구분한다. Wireframe을 끌 때 이전 override 배열을 잘못 복사해 Variant를 잃지 않는다.
- Profile 교체: 오래된 선택/하이라이트/표정/재질/재생 상태를 제거하고 카메라를 새 기본 구도로 초기화한다.
- Null: Profile, Mesh, Animation, Morph, Variant, Part가 각각 없어도 crash 없이 해당 기능만 비활성화한다.
- Cook: 기본 Map과 Profile이 저장·참조되어야 한다. Soft Reference를 도입하면 Asset Manager/Packaging 설정으로 포함 여부를 검증한다.
- Editor에서 보였다는 사실만으로 패키지 포함을 보장하지 않는다. UI/입력/셰이더/Asset 누락은 Win64 패키지로 확인한다.
- Engine가 없으면 빌드·실행 미검증으로 기록한다. 실행 증거 없이 완료 체크를 하지 않는다.

## 11. Git과 완료 보고

Git 저장소/remote가 없으면 그 상태를 보고한다. remote URL을 추측하거나 원격 저장소를 자동 생성하지 않는다.
현재 Git LFS 3.7.1 실행 파일은 확인했으나 Git 저장소는 없다. `.gitattributes` 작성만으로 저장소의 LFS 연결이 완료되지는 않는다.
실제 구현 시 기존 Git 정책을 먼저 확인한다. 저장소를 초기화한 후 `git lfs install --local`을 실행한다.
최초 바이너리 stage 전 `git check-attr filter -- Content/Portfolio/Maps/LV_Portfolio.umap` 등 실제 경로로 LFS 속성을 확인한다.
stage 후 `git lfs ls-files`로 추적을 확인한 뒤 commit한다. 기존 history rewrite/LFS migration은 별도 허가 없이 하지 않는다.
`Binaries`, `DerivedDataCache`, `Intermediate`, `Saved`, IDE 임시 파일, 빌드 산출물은 제외한다.
`.ztl`, `.ma`, `.mb`, Substance 프로젝트, 원본 PSD/고해상도 소스는 임의 반입하지 않는다.
원래 요청의 최종 commit/push는 변경 검토와 검증 후, 실제 branch/remote/upstream을 확인할 수 있을 때 적용한다. push 실패나 미설정 상태를 성공으로 보고하지 않는다.
최종 보고는 구현 파일/기능, Editor 수동 연결, 빌드/실행 결과와 미검증 사유, branch/commit/push, 남은 에셋 작업만 간결하게 작성한다.

### 11.1 2026-09-28 Git/도구 상태 확인 결과

| 항목 | 결과 |
| --- | --- |
| 프로젝트 Git 저장소 | 없음 (`git rev-parse` 실패). 상위 폴더도 저장소 아님 |
| remote / upstream | 없음. URL을 추측하거나 원격 저장소를 자동 생성하지 않는다 |
| Git / Git LFS | Git 2.55.0, Git LFS 3.7.1 설치됨 |
| LFS 필터 | system config에 `filter.lfs.*` 등록됨. 저장소 생성 후 `git lfs install --local`은 여전히 실행한다 |
| 전역 `user.name` / `user.email` | 미설정. commit 전에 사용자가 신원을 지정해야 한다 |
| `gh` CLI | 2.98.0, github.com 계정에 로그인됨. 저장소 자동 생성에는 사용하지 않는다 |
| 홈 디렉터리 `C:\Users\WINCARD1\.git` | 빈 폴더. 유효한 저장소가 아니며 이 프로젝트와 무관 |

다음 세션에서 commit/push를 진행하려면 사용자로부터 (1) commit 신원, (2) remote URL을 받아야 한다. remote가 없으면 `git init` → `git lfs install --local` → 로컬 commit까지만 수행하고 push 미수행으로 보고한다.

### 11.2 2026-09-28 기준 다음 세션 진행 가능 범위

| 요청 | 가능 여부 | 사유 |
| --- | --- | --- |
| 빌드 + NullSafety 테스트 확인 (P0-0) | 불가 | 엔진·컴파일러 없음. 자동 설치 금지 |
| P0 C++ (GameMode, Controller, CameraPawn, Profile 구도 필드, Widget C++ 기반) + `Config/*.ini` | 가능, 컴파일 미검증 | 순수 소스/설정 파일 |
| P0 Editor 에셋 (IA/IMC, WBP, LV_Portfolio, BP_GameMode, DA_Character) | 불가 | `.uasset`/`.umap` 임의 생성 금지, Editor 없음. 수동 절차로 기록 |
| P1 C++ (프리셋, Turntable, Sequence/Pose, Morph 표정, Slot 재질, Clean View) | 가능, 컴파일 미검증 | 12절 지시가 "가능한 C++ 구현을 남겨라"를 허용 |
| P2 | 진행 불가 | P0/P1 검증 불가 상태에서 범위 확장 금지 |
| 로컬 commit | 가능 | 위 11.1의 신원 지정 후 |
| push | 불가 | remote 없음 |

다음 세션 착수 전 사용자 결정 사항:
1. 엔진/VS를 직접 설치할지, 아니면 "C++ 작성 + 수동 절차 + 실행 미검증"으로 한정할지.
2. 엔진 없는 상태에서 P0 C++만 작성할지, P0+P1 C++까지 작성할지. 어느 쪽이든 완료 주장은 하지 않는다.
3. commit 신원(`user.name`/`user.email`)과 remote URL.
4. IA/IMC 에셋을 만들 수 없으므로 Controller가 런타임에 `UInputAction`/`UInputMappingContext`를 생성하는 폴백을 둘지, 문서대로 Editor 에셋 필수로 두고 수동 절차만 남길지.

## 12. Claude Code에 전달할 작업 지시

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

설치 도구의 호환 버전은 [Epic의 Visual Studio 설정 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine)에서 선택한 UE 버전 기준으로 확인한다.
Automation UI/명령 옵션은 [Epic의 Automation 실행 문서](https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine)를 따른다.

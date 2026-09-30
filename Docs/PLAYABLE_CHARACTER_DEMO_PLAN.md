# 캐릭터 플레이 데모 기획 및 구현 인계

작성일: 2026-09-30 · 상태: 기획만 작성, 아래 신규 기능은 미구현

## 1. 목적

3D 캐릭터 아티스트의 취업용 포트폴리오를 위해, 실제 캐릭터가 작은 공간에서 이동하고 동작하는 모습을 보여준다.
관람자는 캐릭터를 직접 조작하면서 실루엣, 관절 변형, 재질, 의상과 머리카락의 표현을 확인한다.
개발자의 게임 포트폴리오나 재미있는 게임 완성이 목적은 아니다. 성공 기준은 캐릭터가 잘 보이고, 움직임이 안정적이며, 시연·촬영하기 쉬운가이다.

최종 전달물은 Windows 실행 폴더와 짧은 시연 영상이다. 웹 포트폴리오에는 나중에 영상·이미지·선택적 다운로드를 연결할 수 있으나, 이번 범위에 웹사이트나 브라우저 실행은 포함하지 않는다.

## 2. 현재 기반과 이 문서의 관계

- 기존 프로젝트: UE 5.6.1, `CharacterShowcase`. 기존 전시 Viewer의 P0/P1/P2와 인계 준비 작업이 진행되어 있다.
- 현재 캐릭터는 전시용 `APortfolioCharacterActor`이고, 사용자가 조작하는 Pawn은 카메라다. Walk Sequence 재생은 이동 조작 구현을 의미하지 않는다.
- 사용 중인 모델은 엔진 튜토리얼 마네킹과 큐브다. 실제 아티스트 캐릭터, Run 및 대표 동작, Morph 지원 여부는 아직 확정되지 않았다.
- 기존 동작과 실행 절차는 [CHARACTER_VIEWER_SETUP.md](CHARACTER_VIEWER_SETUP.md)를 따른다. 그 문서의 검증 결과를 새 플레이 기능의 검증 결과로 간주하지 않는다.
- 이 문서는 새 방향의 범위와 완료 조건이다. 기존 전시 기능을 삭제하거나 전면 재작성할 근거로 사용하지 않는다.

## 3. 최소 완성 범위

| 요소 | 이번 범위 |
| --- | --- |
| 주인공 | 실제 캐릭터 1명. 구현 초기에는 호환 애니메이션을 가진 placeholder 사용 |
| 공간 | 평탄한 작은 전시 공간 1개. 장애물은 충돌 확인과 화면 구성에 필요한 최소량 |
| 이동 | 카메라 기준 걷기·달리기, 이동 방향으로 부드러운 캐릭터 회전 |
| 카메라 | 3인칭 추적, 마우스 회전, 거리 제한이 있는 Zoom, 기본 구도 복귀 |
| 애니메이션 | Idle / Walk / Run 전환, 확보된 대표 동작 최대 2개 |
| 표현 | 실제 캐릭터에 맞춘 조명·배경·재질. 의상·머리카락은 제공된 에셋의 표현 범위 |
| 화면 | 짧은 조작 안내와 필요한 버튼, UI 숨김, 커서 해제 |
| 결과물 | 개발·Shipping 패키지 검증, 조작 가능한 데모와 시연 영상 |

**제외:** 적, AI, 전투 판정, 체력, 피해, 콤보, 무기 장착 시스템, 점프, 파쿠르, 퀘스트, 성장, 인벤토리, 저장, 멀티플레이, 온라인 기능, 여러 캐릭터 선택, 복잡한 이동 시스템.

공격 모션은 외형을 보여주는 대표 동작일 뿐이다. 타격 대상·판정·피해·게임 규칙을 만들지 않는다.
새로운 헤어/의상 시뮬레이션 시스템이나 시네마틱 촬영 도구도 만들지 않는다.

## 4. 사용자 경험과 입력

실행하면 작은 씬의 시작 위치에서 캐릭터 전신과 조작 안내가 보인다. 별도 메인 메뉴 없이 바로 조작한다.
캐릭터 주변을 이동하고 카메라를 돌려 관찰한 뒤, 멈춰서 대표 동작을 재생하거나 UI를 숨겨 촬영한다.

| 입력 | 플레이 모드 동작 |
| --- | --- |
| WASD | 카메라의 수평 방향을 기준으로 이동 |
| Left Shift 누름 | 이동 중 달리기. 놓으면 걷기로 복귀 |
| 마우스 이동 | 카메라 회전. 처음에는 커서를 숨기고 게임 입력을 받음 |
| 마우스 휠 | 제한된 범위에서 카메라 거리 변경 |
| R | 카메라를 기본 거리·구도로 복귀. 캐릭터 위치는 유지 |
| 1 / 2 | 등록된 대표 동작 재생. 없는 항목은 안내하지 않음 |
| H | 안내·버튼 등 데모 UI 숨김/복원 |
| Esc | 커서 해제 및 이동 입력 중지 / 다시 누르면 조작 복귀 |
| Backspace | 캐릭터를 시작 위치로 복귀, 동작 종료, 기본 카메라 복원 |

- 키를 누르고 있는 동안의 반복 입력으로 대표 동작이 매 프레임 재시작되어서는 안 된다.
- UI를 숨겨도 이동·카메라·대표 동작·H·Esc는 사용할 수 있다.
- 커서 해제 상태에서 UI 클릭·드래그·휠이 이동 또는 카메라로 전달되어서는 안 된다.
- 창 포커스를 잃으면 이동·달리기·입력 캡처를 해제한다. 복귀 시 키를 새로 누르기 전까지 이동하지 않는다.
- 카메라는 바닥·벽을 통과하지 않도록 처리한다. 각도·거리·속도는 편집 가능하게 둔다.
- 공간의 가장자리는 충돌로 막는다. 예외적으로 씬 밖으로 떨어지면 시작 위치로 복귀한다.

## 5. 이동과 대표 동작 정책

### 이동

- 최초 범위는 전후좌우 입력에 따라 몸이 이동 방향을 향하는 방식이다. 별도 전투 자세·조준·옆걸음 세트는 요구하지 않는다.
- 실제 이동 속도를 기준으로 Idle / Walk / Run이 전환되어야 한다. 정지했는데 걷거나 벽에 막혔는데 달리는 문제를 확인한다.
- 이동 속도와 애니메이션을 함께 조정하여 눈에 띄는 발 미끄러짐과 전환 튐을 줄인다. 발 IK 등 추가 시스템은 기본 구현으로 해결되지 않는 실제 문제가 확인될 때만 검토한다.
- 제자리 이동 애니메이션을 우선 사용한다. Root Motion 에셋만 있다면 적용 정책을 먼저 확정하며, 원본 에셋을 임의 수정하지 않는다.

### 대표 동작

- 등록된 동작을 입력당 한 번 재생한다. 실행 중 중복 실행과 다른 대표 동작으로의 중첩은 허용하지 않는다.
- 재생 시작 시 이동을 멈추고, 재생 중에는 이동 입력을 적용하지 않는다. 카메라 관찰과 H는 유지한다.
- 완료·중단·시작 위치 복귀 후에는 이동 애니메이션 상태로 돌아간다. 이전 이동 입력을 저장했다가 갑자기 재생하지 않는다.
- 최초에는 제자리 전신 동작을 사용한다. 앞으로 돌진하거나 무기가 필요한 동작은 에셋과 범위가 합의되지 않으면 넣지 않는다.
- 에셋이 없거나 호환되지 않으면 해당 동작을 비활성화하고 원인을 알린다. 빈 참조 때문에 데모 전체가 중단되어서는 안 된다.

## 6. 기존 코드의 재사용 경계

| 현재 자산/코드 | 활용 방침 |
| --- | --- |
| `CharacterProfileData` | Mesh·표정·재질·애니메이션 참조를 재사용. 플레이에 필요한 설정만 최소 추가하고 같은 정보를 별도 데이터에 중복 저장하지 않음 |
| `PortfolioCharacterActor` | 기존 전시용으로 유지. 플레이어 이동 객체처럼 사용하지 않음 |
| 표정·재질 적용 로직 | 실제 플레이 기능에 필요할 때 연결. 재사용을 이유로 관련 없는 Actor 전체를 리팩터링하지 않음 |
| Viewer 카메라·프리셋·Turntable | 기존 전시 화면에 유지. 이동 추적 카메라와 목적을 구분 |
| Viewer Controller·GameMode | 전시 화면 동작을 보존. 현재 카메라 Pawn을 그대로 플레이어 이동 대상으로 삼지 않음 |
| WBP·Clean View | 스타일과 UI 숨김 방식을 참고하되 플레이 화면에는 짧은 조작 안내만 표시 |
| 기존 레벨·조명·Material | 필요한 부분을 활용. 아티스트의 전시 레벨을 덮어쓰지 않고 플레이용 레벨을 별도로 구성 |
| 기존 테스트 | 전시 기능 회귀 확인에 사용. 새 이동·입력·동작 전환 검증은 별도로 수행 |

플레이 캐릭터는 Unreal의 기본 Character 이동 기능을 사용하는 방향으로 구성하고, 기존 이동 기능을 직접 새로 만들지 않는다.
C++는 입력·이동 설정·대표 동작 제어를, Animation Blueprint는 이동 애니메이션 전환을, Level/WBP는 표현과 레이아웃을 담당한다.
기존 `SetAnimation()`은 단일 Sequence 재생 모드로 전환한다. 플레이 중 이를 그대로 호출하여 이동용 AnimBP를 끊지 않도록 재생 경로를 구분해야 한다.
새 클래스 이름과 세부 파일 배치는 구현 시 기존 구조에 맞춰 결정한다. 별도 범용 프레임워크·Manager·플러그인은 만들지 않는다.

## 7. 아티스트와 개발자의 역할

| 아티스트 | 개발자 |
| --- | --- |
| 실제 Mesh, Skeleton, 재질·텍스처 제공 | Import·연결 과정의 기술 문제 해결 |
| 애니메이션 제공 또는 사용 가능한 외부 에셋 선택 | 이동 속도·애니메이션 전환·대표 동작 연결 |
| 얼굴·관절·의상 변형의 품질 확인 | 입력·충돌·카메라·상태 복원 구현 |
| 조명·배경·구도와 최종 표현 결정 | 아티스트가 Editor에서 설정할 수 있게 연결 |
| 제작 기여와 외부 에셋 출처 명시 | 빌드·패키징·기술 검증 |

캐릭터 모델러가 리깅·애니메이션까지 직접 제작한다고 가정하지 않는다. 외부 애니메이션을 쓰면 사용 범위를 확인하고 포트폴리오에서 본인 제작 범위를 구분한다.

구현 초기에 확인할 항목:
- 실제 캐릭터의 Skeleton/리깅 상태, 크기·방향, 재질·헤어·의상 방식.
- Idle / Walk / Run과 대표 동작의 보유 여부 및 Skeleton 호환성.
- 아티스트와 관람자가 사용할 목표 PC·화면 해상도. 현재 개발 PC의 낮은 프레임 성능을 제품 목표로 삼지 않는다.

확인 전에는 호환되는 placeholder로 기술 검증만 진행한다. 확보하지 않은 애니메이션·Morph·헤어를 구현 완료로 보고하지 않는다.

## 8. 구현 순서와 완료 기준

기존 Viewer의 P0/P1/P2와 혼동하지 않도록 새 작업은 D0~D3로 구분한다.

| 단계 | 작업 | 다음 단계로 넘어갈 조건 |
| --- | --- | --- |
| D0 범위·에셋 확인 | 최신 Git/문서/에셋 확인, 사용할 캐릭터·애니메이션과 대상 PC 정리, 기존 Viewer 기준 상태 확인 | 사용할 에셋과 부족한 에셋이 명시됨. 기존 작업을 덮어쓰지 않는 구성 확정 |
| D1 이동 데모 | 플레이 레벨, 플레이 Character, 걷기·달리기, 추적 카메라, Idle/Walk/Run 연결 | 실제 입력으로 이동·정지·회전·달리기 전환, 충돌·카메라·포커스 복귀가 정상. 에셋 부재 항목은 별도 표시 |
| D2 전시 동작·촬영 | 확보된 대표 동작 최대 2개, 최소 안내 UI, H/Esc/R/시작 위치 복귀 | 대표 동작 시작/끝/중단 이후 이동 복원, UI 숨김 상태 조작, UI 입력 차단 정상 |
| D3 실제 캐릭터·배포 | 실제 캐릭터 연결, 변형·조명·구도 조정, Development/Shipping 확인, 시연 영상 제작 | 실제 작품 기준으로 직접 확인하고 제출용 이미지·영상과 실행 폴더 확보 |

실제 캐릭터가 없으면 D1/D2의 기술 구현까지 진행하고, D3는 미완료로 남긴다. 이를 대체하려고 캐릭터 제작·다운로드를 임의 진행하지 않는다.

### 선택 후속: 전시 화면과 플레이 화면 전환

핵심 이동 데모가 통과한 뒤, 아티스트가 필요하다고 할 때만 추가한다. 최초 완료 조건에 포함하지 않는다.
기존 전시 화면에서 확대·Wireframe·Inspection을 보고 플레이 화면으로 이동하는 정도로 제한한다.
두 화면의 입력·Pawn·커서 상태가 섞이지 않게 하고, 동시 캐릭터 중복 표시와 상태 잔류를 막는다.
복잡한 위치·재생 시간 유지 대신 각 화면의 기본 상태로 진입하는 정책을 우선한다.

## 9. 최소 검증과 완료 보고

- 변경 위험과 직접 관련된 테스트만 수행한다. 기존 Viewer가 동작한다는 결과로 새 플레이 기능을 통과 처리하지 않는다.
- 이동: 대각선 속도, Shift 해제, 벽·가장자리 충돌, 포커스 상실 후 입력 잔류, 시작 위치 복귀를 확인한다.
- 애니메이션: 정지·걷기·달리기 전환, 대표 동작 연타·종료·중단 후 복원, 누락/비호환 에셋 처리를 확인한다.
- 화면: 카메라 제한·벽 충돌, UI 입력 경계, Clean View, 실제 캐릭터의 관절·재질·의상 표현을 확인한다.
- Development와 최신 Shipping에서 실행·시각 결과를 확인한다. 창이 살아 있는 것만으로 통과시키지 않는다.
- OS 자동 입력, 자동화 API 호출, 사람이 직접 한 조작을 구분한다. 대상 PC·해상도·측정 조건과 성능 문제도 기록한다.
- 기능 구현 완료와 아티스트의 결과물 승인 완료를 구분한다. 실제 에셋·사람 확인·영상 촬영이 남으면 그대로 보고한다.

기존 `CHARACTER_VIEWER_SETUP.md`에는 실제 구현 완료 후 실행·아티스트 설정 절차만 반영한다. 이 문서의 계획을 기존 검증 결과에 섞지 않는다.
변경 검토와 검증 후 현재 Git/LFS 정책을 따라 논리적인 단위로 commit/push한다. generated/build/test 출력과 현재의 무관한 미추적 폴더는 포함하지 않는다.

## 10. Claude Code 전달 지시

> 이 문서와 CHARACTER_VIEWER_SETUP.md를 읽고 현재 프로젝트를 재확인한 뒤 D0부터 진행해라. 목적은 3D 캐릭터 아티스트의 취업용 포트폴리오에 사용할 작은 플레이 데모다. 기존 Viewer를 재구현하지 말고, 캐릭터 1명·작은 씬 1개·걷기/달리기·추적 카메라·확보된 대표 동작 최대 2개로 범위를 제한해라. 기본 이동 기능과 기존 데이터를 재사용하고 원본 아트 및 기존 레벨을 보존해라. 에셋이 없으면 가능한 placeholder로 기술 검증하되 실제 작품의 검증으로 보고하지 마라. 단계별 완료 기준을 확인한 뒤 다음 단계로 진행해라. 전시/플레이 전환은 최초 필수 범위가 아니다. 불필요한 게임 시스템·리팩터링·의존성을 추가하지 마라. 최종 보고에는 구현 범위, 실행 검증, 아티스트가 연결할 항목, 미검증 사항, commit/push 결과만 간결히 적어라.

## 11. D0 확인 결과 (2026-09-30)

기술 검증용 placeholder를 확정했다. 실제 아티스트 캐릭터의 검증이 아니다.

| 항목 | 결과 |
| --- | --- |
| 기준 커밋 | 3ceddd7 (기존 Viewer P0~P2, 인계 준비, 실제 입력 검증 완료 상태). 기존 Viewer 자산·코드는 그대로 둔다 |
| placeholder | UE 5.6 동봉 템플릿 마네킹 `Templates/TemplateResources/High/Characters/Content/Mannequins`에서 서브셋 38개(53 MB)를 `Content/Characters/Mannequins/`에 원본 경로 그대로 복사. 다운로드 없음, 원본 무수정, LFS 추적 |
| 메시 | `SKM_Manny_Simple`(스켈레톤 `SK_Mannequin`, 89본, 높이 약 180 cm, 슬롯 2개 `M_HeadLegs`/`M_Torso`, LOD 3, Morph 0, 물리 에셋 `PA_Mannequin`) |
| 이동 애니메이션 | `MM_Idle`, 걷기 8방향(`MF_Unarmed_Walk_*`), 조깅 8방향(`MF_Unarmed_Jog_*`), `BS_Idle_Walk_Run`(축 추정: Direction -180~180, Speed 0/300/600), `ABP_Unarmed` |
| ABP 의존성 | `ABP_Unarmed`가 `MM_Jump`/`MM_Land`/`MM_Fall_Loop`와 `CR_Mannequin_FootIK`를 하드 참조한다. 없으면 컴파일 오류. 파일만 유지하고 점프 입력은 바인딩하지 않는다 |
| Root Motion | Walk/Jog: root 전진(300/600 cm/s)이지만 `force_root_lock=True`라 제자리 재생. Attack_01/02: root 150/90 cm 전진, `force_root_lock=False` |
| 대표 동작 후보 | `MM_Attack_01`, `MM_Attack_02`(제자리 아님). 노티파이 없음, 템플릿 C++ 클래스 의존 없음 |
| 회귀 | Editor 자동화 6/6, `CreatePortfolioAssets.py` `[keep]` ×7, Portfolio 자산 변경 없음 |
| 검사 스크립트 | `Scripts/D0_CheckMannequinAssets.py`(읽기 전용) |

정책 결정:
- 이동은 `ACharacter` + `CharacterMovementComponent` 기본 기능과 `ABP_Unarmed`를 그대로 쓴다. 속도 매핑은 걷기 300, 달리기 600 cm/s에서 시작해 D1에서 조정한다.
- 대표 동작의 Root Motion은 **무시(제자리 재생)** 한다. placeholder 공격 클립은 발이 미끄러지는 것을 감수한 기술 검증용이며, 실제 캐릭터에는 제자리 전신 클립을 요구한다. 돌진형 재생은 합의 전까지 넣지 않는다.
- 제외: Jump 재생, `MM_Attack_03`, `MM_ChargedAttack`, Quinn, Pistol/Rifle/Death.

미정 항목(사용자 확인 대기): 목표 PC 사양·해상도, 실제 캐릭터·애니메이션 제공 시점. 확인 전까지 D1/D2는 placeholder 기술 검증까지만 진행하고 D3는 미완료로 둔다.

## 12. D1 구현 결과 (2026-09-30)

범위: 걷기·달리기·추적 카메라. 대표 동작, 안내 UI, 전시/플레이 전환은 포함하지 않는다. placeholder(마네킹) 기술 검증이며 실제 아티스트 캐릭터의 검증이 아니다. 기존 Viewer 코드·에셋(`Content/Portfolio`, `CreatePortfolioAssets.py`, `Config/*.ini`)은 변경하지 않았다.

### 12.1 클래스와 파일

| 파일 | 역할 |
| --- | --- |
| `Source/CharacterShowcase/Character/CharacterProfileData.h` | `WalkSpeed`(300), `RunSpeed`(600) 추가(카테고리 "Play"). Mesh·AnimClass는 기존 `SkeletalMesh`, `DefaultAnimClass`를 그대로 사용 |
| `PlayDemo/DemoCharacter.h/.cpp` | `ADemoCharacter : ACharacter`. SpringArm(350, 충돌 검사, ProbeSize 12) + Camera, 이동 방향 회전(500도/초), `ApplyProfile`, `SetRunning`, `AddMoveInput2D`(카메라 yaw 기준, 길이 1로 제한), `ResetToStart`, 카메라 API(`AddCameraYaw/Pitch`, `ZoomCamera`, `ResetCamera`), 낙하 복귀(`FellOutOfWorld` + Tick에서 시작 Z + `KillZOffset` 검사). 캐릭터의 `WalkSpeed`/`RunSpeed`는 표시 전용(프로필이 없을 때의 fallback, 프로필의 Play 값이 우선) |
| `PlayDemo/DemoPlayerController.h/.cpp` | Enhanced Input(에디터 에셋이 없으면 런타임 fallback IMC/IA 생성), 커서 모드, 포커스 상실 처리(`InputKey` 재정의로 포커스 상실·커서 모드 이후 새로 누르지 않은 키의 OS 반복 입력 무시) |
| `PlayDemo/DemoGameMode.h/.cpp` | DefaultPawn=`ADemoCharacter`, Controller=`ADemoPlayerController`, `DemoProfile` 적용. `ChoosePlayerStart()`가 PlayerStart를 찾지 못하면 원점 +Z 100에 스폰(`FindPlayerStart()`는 World Settings 액터로 대체해 null을 반환하지 않으므로 판정에 쓰지 않음) |
| `Tests/DemoTests.cpp` | Editor(NullRHI) `CharacterShowcase.Demo.NullSafety` |
| `Tests/DemoMovementSmokeTest.cpp` | `-game` 스모크 `CharacterShowcase.Demo.MovementSmoke` (ClientContext) |
| `Scripts/CreatePlayDemoAssets.py` | 신규 에셋 생성(생성 전용, 기존 에셋은 `[keep]` 검증만). `Content/Portfolio`는 건드리지 않음 |

Build.cs 변경 없음(신규 모듈 없음).

### 12.2 에셋

| 경로 | 내용 |
| --- | --- |
| `/Game/PlayDemo/Data/DA_PlayCharacter_Manny` | `CharacterProfileData`. DisplayName "Manny (placeholder)", `SkeletalMesh`=`SKM_Manny_Simple`, `DefaultAnimClass`=`ABP_Unarmed`, WalkSpeed 300, RunSpeed 600 |
| `/Game/PlayDemo/Blueprints/BP_DemoCharacter` | 부모 `ADemoCharacter`. 카메라 거리·각도·`KillZOffset`·메시 상대 위치/회전 등을 BP에서 조정할 때 사용 |
| `/Game/PlayDemo/Blueprints/BP_DemoGameMode` | 부모 `ADemoGameMode`. `DemoProfile`=`DA_PlayCharacter_Manny`, `DefaultPawnClass`=`BP_DemoCharacter` |
| `/Game/PlayDemo/Maps/LV_PlayDemo` | 30m x 30m 바닥(상면 Z=0), 가장자리 벽 4개(높이 2m, 안쪽 면 +-1475), 장애물 3개, PlayerStart(0,0,100) +X 방향, Key/Fill Directional + SkyLight(Viewer 레벨과 같은 값). World Settings GameMode = `BP_DemoGameMode` |

`DefaultEngine.ini`의 `GameDefaultMap`(Viewer)은 변경하지 않았다. 패키지 빌드의 `MapsToCook`에는 아직 `LV_PlayDemo`가 없다(D3에서 결정).

### 12.3 입력 (fallback 이름과 키)

| Input Action (fallback 이름) | 값 | 키 | 동작 |
| --- | --- | --- | --- |
| `IA_DemoMove_Fallback` | Axis2D | W/S/A/D (W: Swizzle YXZ, S: Swizzle+Negate, A: Negate) | 카메라 yaw 기준 이동. 대각선은 길이 1로 제한 |
| `IA_DemoRun_Fallback` | Bool | Left Shift | Started: 달리기, Completed/Canceled: 걷기 |
| `IA_DemoLook_Fallback` | Axis2D | Mouse2D | X: yaw, Y: pitch(+Y = 위를 봄, 별도 Negate 없음). 감도 `LookSensitivity` 0.3도/단위 |
| `IA_DemoZoom_Fallback` | Axis1D | Mouse Wheel | 휠 1칸당 `ZoomStep` 60cm, 150~600 제한 |
| `IA_DemoResetCamera_Fallback` | Bool | R | 거리 350, pitch -15, yaw = 캐릭터 뒤. 위치 유지 |
| `IA_DemoRespawn_Fallback` | Bool | Backspace | 시작 위치 복귀 + 정지 + 카메라 복원 |
| `IA_DemoToggleCursor_Fallback` | Bool | Esc | 커서 모드 토글(커서 표시, GameAndUI, 이동·시점·줌·R·Backspace 무시, 이동 정지) |

- 시작: `FInputModeGameOnly`, 커서 숨김. 포커스 상실(`OnApplicationActivationStateChanged(false)`): `FlushPressedKeys`, 달리기 해제, 이동 정지. 복귀 후에는 키를 새로 눌러야 이동한다. 키를 누른 채 창을 다시 클릭해도 OS 키 반복(`IE_Repeat`)은 그 키를 새로 누를 때까지 `ADemoPlayerController::InputKey`에서 버린다(Enhanced Input은 `IE_Repeat`를 눌림으로 취급하므로). 커서 모드 진입(Esc) 후에도 같다.
- 카메라 제한(편집 가능): Pitch -60~30, 거리 150~600, 기본 -15 / 350. 카메라는 SpringArm 충돌 검사로 바닥·벽을 통과하지 않는다.
- 낙하: Z < 시작 위치 Z + `KillZOffset`(-500)이면 시작 위치로 복귀(`ResetToStart`). PlayerStart·레벨 높이를 옮겨도 기준이 따라간다.
- 이동 속도: `ADemoCharacter`의 `WalkSpeed`/`RunSpeed`는 표시 전용(VisibleAnywhere)이며 프로필이 없을 때만 쓰는 fallback이다. 실제 값은 프로필(`CharacterProfileData`의 Play 카테고리)에서 바꾼다.
- 안내 UI는 없다. `HintWidgetClass`가 비어 있으면 로그 한 줄만 남긴다(D2 항목).

### 12.4 실행 방법

- PIE: `LV_PlayDemo`를 열고 Play. GameMode는 World Settings 오버라이드로 적용된다. PIE에서 Esc는 에디터가 PIE를 종료하므로 커서 해제는 Shift+Esc 또는 `-game`에서 확인한다.
- `-game`: `UnrealEditor.exe <uproject> /Game/PlayDemo/Maps/LV_PlayDemo -game -windowed -ResX=1280 -ResY=720 -log`
- 에셋 생성: `UnrealEditor-Cmd.exe <uproject> -ExecutePythonScript=<abs>\Scripts\CreatePlayDemoAssets.py -unattended -nosplash -nop4 -log`

### 12.5 검증 결과 (개발 PC, 약 5 FPS)

| 항목 | 결과 |
| --- | --- |
| Editor / Game Development 빌드 | 오류 0 / 경고 0 (신규·변경 cpp 전체 재컴파일 기준) |
| `CreatePlayDemoAssets.py` | 1회차 4개 생성(DA, BP 2, LV), 2회차 `[keep] OK` x4, `Content/PlayDemo` 4개 파일 SHA-256 동일. `CreatePortfolioAssets.py` 재실행 `[keep] OK` x7 + ini, `git status`에 Portfolio 변경 없음 |
| Editor 자동화(NullRHI) | 7/7 (기존 6 + `Demo.NullSafety`), 경고 0 |
| `-game` `Demo.MovementSmoke` | 1/1 성공, 실행 41초(자동화 프레임워크가 10 FPS 대기에 600초 소비) |
| Viewer 맵 헤드리스 로드 | `LV_Portfolio` -game -NullRHI 로드, GameMode `BP_CharacterViewerGameMode_C`, 로그 Error 0 |

`-game` 스모크 단계별 측정값(Enhanced Input 주입 + `PlayerController::InputKey`):

| 단계 | 측정값 |
| --- | --- |
| 1 정지·연결 | 메시 `SKM_Manny_Simple`, AnimInstance `ABP_Unarmed_C`, 속도 0.00, 시작 (0,0,98.15) |
| 2 걷기 | 속도 300.0(범위 250~310), 2초간 전진 743cm, yaw 0.0. 오른쪽 이동 시 yaw 90.0, 속도 (X 0, Y 300) |
| 3 달리기 | 속도 600.0(범위 550~610), Shift 해제 후 300.0 |
| 4 대각선 (1,1) | 속도 300.0, 최대 300.0 (기준 305 이하) |
| 5 정지 | 입력 중단 1초 후 0.00 |
| 6 벽 | X = 1432.5 (한계 1475 - 42 = 1433), 밀고 있는 속도 0.00 |
| 7 카메라 | Look(200,0) yaw 60.0, Look(0,-200) pitch -60.00(Min), Look(0,400) pitch 30.00(Max), Zoom -5 arm 600.0(Max), Zoom +100 arm 150.0(Min), R 후 arm 350 / pitch -15.00 / yaw 0.00. 카메라 충돌: pitch 30 + arm 600에서 카메라 Z 12.0(바닥 위), 실제 거리 272.3 < 600 |
| 8 포커스 | Shift+W를 `InputKey`로 누른 채 속도 600.0 -> 포커스 상실 직후 `IsRunning()` false -> 1초 후 속도 0.00(W는 눌린 상태로 남겨 둠) -> 복귀 후 1초간 속도 0.00, 이동 0.0cm -> W를 다시 누르면 300.0 |
| 9 낙하 | Z = KillZ - 100 (-600)으로 이동 후 1.5초 뒤 시작 위치와 거리 0.0 |
| 10 Esc | 커서 표시, Move 주입 1.5초 동안 속도 0.00·이동 0.0cm. 다시 토글 후 Move 속도 300.0 |

- 테스트는 물리 키보드·마우스가 창에 들어와도 결과가 바뀌지 않도록 각 단계마다 `GameViewport->SetIgnoreInput(true)`를 다시 걸고, 시작 시 `ReleaseAllInput` + `ResetToStart`로 초기화한다. 이 처리를 넣기 전 두 번의 실행에서 10분 대기 중 들어온 실제 마우스·휠 입력으로 카메라/시작 상태가 달라져 실패했고(카메라 거리 410, yaw/pitch 변경), 원인을 확인한 뒤 초기화와 입력 무시를 추가했다. 허용 오차는 바꾸지 않았다.
- 스크린샷: `Saved/Screenshots/WindowsEditor/DemoSmoke_Idle.png`, `DemoSmoke_Run.png`, `DemoSmoke_Wall.png` (1280x720). Idle은 마네킹 뒷모습·바닥 타일·장애물·먼 벽, Run은 달리기 자세(팔 스윙, 모션 블러), Wall은 벽 앞에서 서 있는 자세로 확인했다.

### 12.6 알려진 한계

- placeholder(엔진 마네킹)이며 실제 캐릭터·애니메이션·조명은 검증하지 않았다. 레벨 조명은 Viewer와 같은 값이라 밝고 대비가 낮다(D3에서 조정).
- 입력은 Enhanced Input 주입과 `PlayerController::InputKey`(키 매핑 경로, 12.8)로 검증했다. OS 수준 실제 키보드·마우스 조작으로 검증하지 않았다(마우스 감도 0.3도/단위는 조정 전 값). 시작 시 카메라 구도는 컨트롤러 `BeginPlay` 직후 기록값으로 검증한다(12.8).
- 발 미끄러짐은 정지 화면과 속도 값으로만 확인했고 동작 영상 검토는 하지 않았다. Root Motion 정책: 걷기·조깅 클립은 `force_root_lock`이라 제자리 재생이며 캐릭터 이동은 CharacterMovement가 담당한다. 대표 동작(Attack)의 Root Motion 무시 정책은 D2에서 적용한다.
- 안내 UI, H(UI 숨김), 대표 동작(1/2), 전시/플레이 전환은 미구현(D2 이후). 점프 입력은 바인딩하지 않았다(`ABP_Unarmed`의 Jump/Fall 자산은 컴파일 의존성 때문에 유지).
- `-game` 스모크는 자동화 프레임워크의 10 FPS 대기(최대 600초) 때문에 이 PC에서 약 11~12분 걸린다. 그 동안 창이 마우스를 캡처한다.
- `LV_PlayDemo`는 패키지 `MapsToCook`에 없어 Shipping/Development 패키지 검증은 D3에서 다시 한다.

### 12.7 아티스트가 연결할 항목

1. `DA_PlayCharacter_Manny`를 복제하거나 실제 캐릭터 프로필을 만들어 `SkeletalMesh`, `DefaultAnimClass`(Idle/Walk/Run 전환 AnimBP), `WalkSpeed`, `RunSpeed`를 설정한다. Walk/Run 클립의 이동 속도와 이 값을 맞춰 발 미끄러짐을 줄인다. 속도는 프로필 값이 적용되며 `BP_DemoCharacter`의 `WalkSpeed`/`RunSpeed`는 표시 전용 fallback이다.
2. `BP_DemoGameMode`의 `DemoProfile`을 그 프로필로 교체한다. 메시가 마네킹과 방향이 다르면 `BP_DemoCharacter`의 Mesh 컴포넌트 상대 위치/회전(C++ 기본 yaw -90, Z = -캡슐 절반 높이)을 조정한다. `ApplyProfile`은 메시 에셋·AnimClass·속도만 적용하고 이 상대 위치/회전은 덮어쓰지 않는다.
3. 캐릭터 크기에 맞게 `BP_DemoCharacter`의 캡슐, 카메라 거리(기본 350, 150~600), Pitch 제한, 카메라 오프셋(SpringArm 상대 위치 Z 50)을 조정한다.
4. `LV_PlayDemo`의 바닥·벽·조명·배경은 실제 작품에 맞게 아티스트가 편집한다(`CreatePlayDemoAssets.py`는 기존 레벨을 덮어쓰지 않는다).

### 12.8 리뷰 반영 (2026-09-30)

D1 리뷰 8건을 반영했다. placeholder 기술 검증이며 실제 손 조작 검증은 아니다.

| # | 항목 | 반영 |
| --- | --- | --- |
| 1 | `ApplyProfile()`이 메시 상대 위치/회전을 매번 하드코딩 값으로 덮어씀 | 해당 줄 삭제. 기본값은 생성자에만 있고 `BP_DemoCharacter`에서 조정한 값이 유지된다. 메시 에셋·AnimClass·속도 적용은 그대로 |
| 2 | 포커스 복귀 시 누른 채인 키의 OS 반복(`IE_Repeat`)으로 이동 재개 가능 | `ADemoPlayerController::InputKey(const FInputKeyEventArgs&)` 재정의. `ReleaseAllInput()`(포커스 상실·커서 모드) 이후, 새 `IE_Pressed`가 없었던 키의 `IE_Repeat`는 소비하고 전달하지 않는다(키별) |
| 3 | 키 매핑 경로 검증 부족 | 스모크 11단계 추가: A/S/D, 마우스 X/Y(`EKeys::MouseX/MouseY` 축 이벤트), 휠(`MouseWheelAxis`), R, Backspace, Esc를 모두 `PlayerController::InputKey`로 보낸다. 기존 주입 단계는 유지 |
| 4 | 캐릭터 `WalkSpeed`/`RunSpeed`가 프로필에 덮어써지는데 편집 가능 | `VisibleAnywhere`/`BlueprintReadOnly`로 변경. 프로필이 없을 때의 fallback이며 프로필 Play 값이 우선(12.3, 12.7) |
| 5 | KillZ가 월드 절대 Z | `KillZOffset`(-500)으로 이름 변경, 시작 위치 Z + `KillZOffset` 아래에서 복귀. 스모크 낙하 단계도 상대 값 사용 |
| 6 | 스모크 1단계의 초기 카메라 검사가 초기화 뒤라 무의미 | 컨트롤러가 `BeginPlay` 직후 pitch/거리를 기록(`GetBeginPlayCameraPitch/ArmLength`), 첫 단계에서 초기화 전에 -15 / 350(+-0.5) 검사. 실제 창 입력에 흔들리지 않도록 기록값으로 판정하고 초기화 전 실시간 값은 로그로 남긴다. `PossessedBy()`의 `ResetCamera()`는 제거: 엔진 `APlayerController::OnPossess`가 `PossessedBy` 직후 컨트롤 회전을 폰 회전으로 덮어쓰고, 시작 시 `SpawnPlayActor`가 월드 `BeginPlay`보다 먼저라 컨트롤러 `BeginPlay`의 `ResetCamera()`가 최종 구도를 정한다 |
| 7 | 에셋 스크립트 `[keep]` 검사가 느슨함 | `BP_DemoGameMode` CDO `default_pawn_class`가 `BP_DemoCharacter` 생성 클래스인지, `LV_PlayDemo` World Settings `default_game_mode`가 `BP_DemoGameMode` 생성 클래스인지 비교. 다르면 DIFFERS만 출력(수정 없음) |
| 8 | `FindPlayerStart()`는 null을 반환하지 않아 원점 fallback이 죽은 코드 | `ChoosePlayerStart()` 결과가 없거나 `APlayerStart`가 아니면 원점 +Z 100에 스폰. 헤더 주석과 12.1 수정 |

재검증(개발 PC):

| 항목 | 결과 |
| --- | --- |
| Editor / Game Development 빌드 | 둘 다 성공, 오류 0 / 경고 0 (변경 cpp 6개 재컴파일) |
| `CreatePlayDemoAssets.py` | `[keep] OK` x4(DIFFERS 없음), `Content/PlayDemo` 4개 파일 SHA-256 실행 전후 동일 |
| `CreatePortfolioAssets.py` | `[keep] OK` x7 + ini OK, `git status`에 Portfolio 변경 없음 |
| Editor 자동화(NullRHI) | 7/7 성공, 경고 0 |
| `-game` `Demo.MovementSmoke` | 1/1 성공, 경고 0, 테스트 67초(프로세스 전체 11.4분), 로그 Error 0 |

`-game` 스모크 신규·변경 단계 측정값(모두 `PlayerController::InputKey` 경로):

| 단계 | 측정값 |
| --- | --- |
| 1 시작 구도 | BeginPlay 기록 pitch -15.00 / 거리 350.0, 초기화 전 실시간 pitch -15.00 / 거리 350.0 / yaw 0.0 |
| 8e~8g 키 반복 | W 누름 속도 300.0 -> 포커스 상실 -> 복귀 후 1초간 W `IE_Repeat`만: 속도 0.00, 최대 0.00, 이동 0.0cm -> W 새로 누름 속도 300.0 |
| 9 낙하 | 시작 Z 98.2 + KillZOffset -500 - 100 = Z -501.9로 이동, 1.5초 뒤 시작 위치와 거리 0.0 |
| 11 A / S / D | 속도 (0,-300) yaw -90 / (-300,0) yaw 180 / (0,300) yaw 90. 1.5초 이동 485 / 475 / 470cm. 키를 떼면 1초 안에 속도 < 5 |
| 11 마우스 | MouseX +100: yaw 0 -> 2.10(오른쪽), MouseY +100(마우스 위): pitch -15 -> -12.90(위를 봄). 마우스 1단위당 0.021도 = `LookSensitivity` 0.3 x 엔진 기본 Mouse 축 감도 0.07(`BaseInput.ini`) |
| 11 R | 시점 변경 뒤 거리 350.0, pitch -15.00, yaw 0.00 |
| 11 휠 | `MouseWheelAxis` +1: 거리 350.0 -> 290.0(ZoomStep 60, 1칸만 적용) |
| 11 Backspace | W로 475.4cm 이동 뒤 시작 위치와 거리 0.0, 속도 0.00, 거리 350.0, pitch -15.00 |
| 11 Esc | 커서 모드에서 W 1.5초: 속도 0.00, 이동 0.0cm. Esc 다시 누름 뒤 W 속도 300.0 |

- 테스트 중 `GameViewport->SetIgnoreInput(true)`가 켜져 있어도 `PlayerController::InputKey` 호출은 뷰포트를 거치지 않으므로 키 매핑 단계가 정상 동작함을 확인했다(위 수치).
- 기존 단계 수치(2~8d, 10)는 12.5와 같다(걷기 300.0, 달리기 600.0, 대각선 300.0, 벽 X 1432.6, 카메라 60.0 / -60 / 30 / 600 / 150, 충돌 카메라 Z 12.0). `DemoSmoke_Run.png`에서 달리기 자세를 다시 확인했다.
- 마우스 실효 감도는 1단위당 0.021도다(엔진 기본 축 감도 0.07 포함). 실제 마우스 조작 체감은 사람이 확인해야 한다.
- 남은 미검증: OS 수준 실제 키보드·마우스 조작(키 반복·포커스 전환 포함)은 사람이 직접 확인하지 않았다.

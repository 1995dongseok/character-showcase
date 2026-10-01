# CharacterShowcase — 3D 캐릭터 포트폴리오 뷰어

Unreal Engine 5.6.1로 만든 **캐릭터 전시용 뷰어**다. 캐릭터 한 명을 스튜디오 조명 아래 세워 두고 돌려 보기, 얼굴/상반신/전신 구도, 애니메이션·표정·재질 바꾸기, 파츠 클릭(삼각형 수·재질 정보), 와이어프레임, 고해상도 스크린샷과 36장 턴테이블 촬영을 할 수 있다. 3인칭으로 걸어 다니는 **플레이 데모** 맵도 함께 있다. 지금 들어 있는 캐릭터는 엔진 기본 마네킹(Manny)이며, 내 캐릭터는 코드 수정 없이 Editor에서 등록한다.

![뷰어 기본 화면 (1280×720)](Docs/Images/viewer_ui_1280x720.png)

## 10분 안에 보기 (설치가 끝난 뒤)

1. 설치: Epic Games Launcher → **UE 5.6.1**, **Visual Studio 2022**(워크로드 "C++를 사용한 게임 개발"), **Git + Git LFS**. 자세한 순서는 [Docs/ARTIST_QUICKSTART.md](Docs/ARTIST_QUICKSTART.md) 0단계.
2. 받기: `git lfs install` 후 `git clone https://github.com/1995dongseok/character-showcase`
3. `Tools\BuildEditor.bat` 더블클릭 — C++를 한 번 빌드한다(끝에 `[OK] Build succeeded`).
4. `Tools\RunViewer.bat` 더블클릭 — 1920×1080 창에 Manny와 오른쪽 패널이 뜬다. 화면이 작거나 느리면 명령 창에서 `Tools\RunViewer.bat --720p`.
5. Editor에서 열고 싶으면 `Tools\OpenEditor.bat` (`LV_Portfolio` 레벨이 자동으로 열린다 → 툴바 **Play**).

`.uproject` 더블클릭/우클릭 메뉴는 엔진이 레지스트리에 등록되지 않은 PC에서 동작하지 않을 수 있다. 그래서 `Tools\*.bat`을 쓴다. 모든 .bat은 `--check`를 붙이면 실행할 명령만 출력하고 아무것도 시작하지 않는다. 엔진이 다른 곳에 있으면 `set UE_ROOT=D:\...\UE_5.6` 후 실행한다.

## 조작 키

| 뷰어 (`RunViewer.bat`, 레벨 `LV_Portfolio`) | | 플레이 데모 (`RunPlayDemo.bat`, 레벨 `LV_PlayDemo`) | |
| --- | --- | --- | --- |
| 왼쪽 드래그 | 돌려 보기(Orbit) | W/A/S/D | 이동(카메라 기준) |
| 휠 | 확대/축소 (패널 위에서는 패널 스크롤) | 왼쪽 Shift | 달리기 |
| R | 카메라 초기화 | 마우스 | 시점 회전 |
| Space | 턴테이블 켜기/끄기 | 휠 | 카메라 거리 |
| H | Clean View (UI·강조 숨김) | R | 카메라 초기화 |
| I | Inspection (파츠 클릭 정보) | Backspace | 시작 위치로 |
| W | Wireframe | Esc | 커서 보이기/숨기기 |
| F12 | 고해상도 스크린샷 1장 | Alt+F4 | 종료 |
| Shift+F12 | 턴테이블 36장 연속 촬영 | | |
| Esc | 촬영 취소 (Alt+F4 = 종료) | | |

패널 버튼(CHARACTER / VIEW / DISPLAY / ANIMATION / EXPRESSION …)으로도 같은 기능을 쓸 수 있다.

## 내 캐릭터 넣기

FBX 내보내기·가져오기 규칙은 [Docs/FBX_IMPORT_GUIDE.md](Docs/FBX_IMPORT_GUIDE.md), Editor에서 등록하는 9단계는 [Docs/ARTIST_QUICKSTART.md](Docs/ARTIST_QUICKSTART.md) 3단계(요약 체크리스트)와 [Docs/CHARACTER_VIEWER_SETUP.md](Docs/CHARACTER_VIEWER_SETUP.md) 2절(필드 하나하나 설명)을 본다. 가장 쉬운 길은 `DA_Character_Manny`를 복제해서 메시만 바꾸는 것이다.

## 찍기

- **F12** → UI 없이 화면 해상도 × 2로 저장: `Saved/Screenshots/Portfolio/<프로필>_<구도>_<날짜-시각>.png`
- **Shift+F12** → 캐릭터를 10°씩 돌려 36장: `Saved/Screenshots/Portfolio/Turntable_<프로필>_<날짜-시각>/frame_000.png … frame_035.png`. 영상/GIF로 바꾸는 ffmpeg 명령은 ARTIST_QUICKSTART 5단계.
- 남에게 보낼 실행 파일: `Tools\PackageViewer.bat` → `Saved\Packaged\CharacterShowcase-Win64-<yyyyMMdd>.zip` (뷰어만 포함, 플레이 데모 제외).

## 하드웨어 참고

개발 PC는 Intel i5-9600K + **내장 그래픽(Intel UHD 630)** 이다. 여기서 뷰어는 **약 5 FPS**로 동작한다 — 느리지만 모든 기능과 촬영은 된다(키는 조금 길게 누른다). 일반 게이밍 그래픽카드에서는 훨씬 부드러울 것으로 예상하지만 **측정한 적은 없다**. 처음 실행할 때는 셰이더 컴파일 때문에 화면 왼쪽 위에 `Preparing Shaders (N)`이 한동안 뜨고 일부 재질이 회색으로 보일 수 있다(기다리면 사라진다).

## 문제가 생기면

| 증상 | 먼저 볼 것 |
| --- | --- |
| .bat이 `[ERROR] Unreal Engine 5.6 not found` | UE 5.6.1 설치 경로 확인, 또는 `set UE_ROOT=...` |
| `Content : lfs-pointer` / 에셋을 못 읽음 | `git lfs install` → `git lfs pull` |
| `editor module missing` | `Tools\BuildEditor.bat` (VS 2022 필요) |
| 검은 화면, 캐릭터가 안 보임, 버튼이 없음, 표정이 안 바뀜 … | [ARTIST_QUICKSTART.md 7단계 문제 해결 표](Docs/ARTIST_QUICKSTART.md#7-문제-해결) |

개발/인계용 상세 기록(설계, 검증 수치, 남은 위험)은 [Docs/CHARACTER_VIEWER_SETUP.md](Docs/CHARACTER_VIEWER_SETUP.md), 플레이 데모는 [Docs/PLAYABLE_CHARACTER_DEMO_PLAN.md](Docs/PLAYABLE_CHARACTER_DEMO_PLAN.md).

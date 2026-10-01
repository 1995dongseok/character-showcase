# FBX 내보내기·가져오기 가이드 (이 뷰어 기준)

Blender / Maya / ZBrush→Maya에서 캐릭터를 내보내 이 프로젝트에 넣을 때의 체크리스트다. 등록(Data Asset·GameMode·레벨)은 [ARTIST_QUICKSTART.md](ARTIST_QUICKSTART.md) 3단계, 필드 설명은 [CHARACTER_VIEWER_SETUP.md](CHARACTER_VIEWER_SETUP.md) 2절.
옵션 이름은 UE 5.6 기본 FBX 가져오기 창(Interchange) 기준이다. 버전·설정에 따라 표기가 조금 다를 수 있으니 **확인 방법**으로 결과를 판단한다.

**기준이 되는 placeholder**: `SKM_Manny_Simple` — 키 약 180 cm, 본 89개, LOD0 92,178 삼각형, Material Slot 2개(`M_HeadLegs`, `M_Torso`), 텍스처 1024²(Torso 노멀만 4096²), Physics Asset `PA_Mannequin`, Morph 0개. 내 캐릭터가 이것과 비슷한 규모·방향이면 기존 카메라 구도와 애니메이션이 그대로 맞는다.

## 1. 단위 · 스케일 · 축

- UE 1 unit = **1 cm**. 사람 캐릭터는 **약 180 cm**가 되게 한다. 발바닥이 원점 높이(Z 0), 원점은 두 발 사이.
- **Blender**: Scene Properties → Units → Unit System **Metric**, **Unit Scale 0.01**, Length **Centimeters** → 모델 높이 약 180. 캐릭터 정면은 **-Y**(Numpad 1 Front 뷰에서 얼굴이 보임), 위는 **Z**. FBX 내보내기: Apply Scalings **FBX Units Scale**, 축은 기본값(Forward -Z, Up Y) 유지 — UE가 Z-up으로 변환한다. 모든 오브젝트에 Apply → All Transforms 후 내보낸다.
- **Maya**: Preferences → Settings → Working Units Linear **centimeter**. Y-up(기본)/Z-up 어느 쪽이든 UE가 변환한다. 캐릭터 정면은 Y-up이면 **+Z**. FBX Export: Units **Automatic**(cm), Smoothing Groups 켬, Skins 켬, Blend Shapes 켬(표정이 있으면).
- **ZBrush → Maya**: ZBrush 하이폴리를 그대로 넣지 않는다. 리토폴로지/데시메이트 + UV + 베이크(노멀·AO) 후 Maya에서 스킨을 입혀 위 Maya 규칙으로 내보낸다. 참고 규모: Manny LOD0 약 9만 삼각형. 이 뷰어는 실시간이라 수십만 삼각형까지는 돌아가지만 내장 그래픽에서는 느려진다.
- 스케일·회전은 DCC에서 맞추고, UE 가져오기 창의 Uniform Scale/Rotation 보정은 쓰지 않는다(1.0, 0).

**확인 방법**: 가져온 메시를 더블클릭 → 메시 에디터에서 캐릭터가 Manny(`Content/Characters/Mannequins/Meshes/SKM_Manny_Simple`)와 **같은 방향**을 보고 있다. 빈 레벨(**File → New Level → Basic**, 저장 안 함)에 둘을 나란히 놓으면 키가 비슷하다(뷰어 레벨에서는 캐릭터가 카메라를 향하도록 이미 회전되어 있다).

**키가 180 cm와 많이 다르면** 카메라 구도를 키(H, cm)에 비례해 바꾼다(Manny 값에서 계산, FOV 그대로): Full Body Target Z ≈ 0.50 H, Distance ≈ 2.54 H / Upper Body ≈ 0.77 H, 1.27 H / Face ≈ 0.92 H, 0.99 H. Min Distance ≈ Distance × 0.45, Max Distance ≈ Distance × 2(단 2,500 미만).

## 2. 스켈레톤 — 새로 만들까, `SK_Mannequin`을 쓸까

| | `SK_Mannequin` 재사용 | 새 Skeleton |
| --- | --- | --- |
| 언제 | 리그가 **UE5 마네킹과 같은 본 이름·계층**(`root` → `pelvis` → `spine_01`… `head`, `clavicle_l`/`upperarm_l`/`lowerarm_l`/`hand_l`, `thigh_l`/`calf_l`/`foot_l`/`ball_l` …)일 때. 예: Manny FBX를 내보내 그 본에 스킨을 입힌 경우 | 본 이름·계층이 다를 때(머리카락·천 본 추가, 다른 리그, 비인간형) |
| 가져오기 | **Skeleton** 칸에 `/Game/Characters/Mannequins/Meshes/SK_Mannequin` 지정 | **Skeleton** 칸 비움 → `<메시>_Skeleton` 생성 |
| 얻는 것 | `MM_Idle`, `MF_Unarmed_Walk_Fwd`, `MF_Unarmed_Jog_Fwd`(뷰어), `ABP_Unarmed`(플레이 데모)가 **리타겟 없이** 바로 재생. `DA_Character_Manny`의 Parts 본 이름도 그대로 쓸 수 있다 | 자유로운 리그. 대신 애니메이션은 같은 리그로 직접 만들어 넣거나, IK Retargeter로 Manny 클립을 리타겟한 Sequence를 만든다 |
| 주의 | 몸 비율이 Manny와 많이 다르면 클립의 본 이동값 때문에 어색할 수 있다. 공용 에셋 `SK_Mannequin`의 설정은 바꾸지 않는다 | 계층이 Manny와 완전히 같으면 뷰어는 Manny 클립도 허용한다(본 계층 호환 검사, CHARACTER_VIEWER_SETUP.md 6.11절). 품질은 보장 안 됨 |

- Blender: 아마추어 오브젝트 자체가 최상위 본으로 내보내진다(오브젝트 이름이 `Armature`면 UE가 그 노드를 건너뛴다). 내보내기에서 **Add Leaf Bones 끄기**, **Only Deform Bones** 켜기(컨트롤 본 제외).

**확인 방법**: 메시 에디터 왼쪽 **Skeleton Tree**의 최상위가 `root`(재사용 시)이고, 불필요한 `_end` 본이 없다. 재사용이면 메시를 열었을 때 상단에 Skeleton이 `SK_Mannequin`으로 표시된다.

## 3. Physics Asset (파츠 클릭에 필수)

- 가져오기 옵션 **Create Physics Asset** 켬(기본) → `<메시>_PhysicsAsset`. 잊었으면 메시 우클릭 → **Create → Physics Asset**, 그리고 메시 에디터 → Asset Details → Physics → **Physics Asset** 칸에 지정.
- 뷰어의 파츠 클릭은 화면에서 **Physics Asset의 바디(캡슐/박스)** 에 맞은 본을 찾는다(삼각형이 아님). 그래서 Data Asset **Parts → Bone Names**에 적는 본(또는 그 자식 본)에 **바디가 있어야** 한다. 클릭된 본이 목록에 없으면 부모 쪽으로 최대 10단계 올라가며 찾는다(손가락 → `hand_l` → 왼팔).
- 권장 바디: `pelvis`, `spine_*`, `head`, 위·아래팔, 손, 허벅지, 종아리, 발(Manny의 `PA_Mannequin`과 같은 구성). 머리카락·망토처럼 바디 밖으로 튀어나온 부분은 클릭이 안 맞을 수 있다 → Physics Asset 에디터에서 해당 본에 Shape를 추가하거나 크기를 키운다.

**확인 방법**: Physics Asset을 열어 Skeleton Tree에서 바디가 붙은 본 이름을 확인 → Parts의 Bone Names와 **대소문자까지 같게**. 뷰어에서 I(Inspection) → 각 부위 클릭 → 패널에 파츠 이름.

## 4. Morph Target (표정)

- Blender Shape Key / Maya blendShape로 만든다. 기본 메시 = 무표정(Neutral), 각 Morph는 가중치 0이면 변화 없음, 1이면 완전 적용. Blender는 Armature 외 모디파이어를 적용한 뒤에야 Shape Key가 내보내진다.
- 가져오기 옵션 **Import Morph Targets** 켬.
- 이름: 영문·숫자·밑줄만, 공백 없이, 좌우는 `_L`/`_R`(예: `Smile_L`, `Blink_R`, `JawOpen`). 가져온 뒤 이름을 바꾸지 않는다(Expression의 **Morph Name**과 글자 그대로 일치해야 한다).
- Data Asset **Expressions**: Neutral 행은 Morphs 비움, 나머지는 **Morph Name** + **Weight**(0~1) 조합.

**확인 방법**: 메시 에디터 → **Morph Target Previewer**(Window 메뉴) 목록에 이름이 있고 슬라이더로 얼굴이 바뀐다. 뷰어 EXPRESSION 버튼으로 표정이 바뀐다.

## 5. Material Slot 이름 = 파츠

- DCC의 재질(Blender Material / Maya Shader) 하나가 UE의 **Material Slot** 하나가 되고, 슬롯 이름은 그 재질 이름이다. **파츠별로 재질을 나누고 이름을 붙인다**: 예 `M_Head`, `M_Body`, `M_Hair`, `M_Cloth`, `M_Eyes`.
- Data Asset **Parts**의 **Material Slot Names**에 그 이름(예 Head 파츠 = `M_Head`)을 적으면 ① 선택 시 정확히 그 슬롯만 마젠타로 칠해지고 ② 그 파츠의 삼각형 수·재질·텍스처를 뷰어가 직접 잰다. 슬롯이 1개뿐이면 Bone Names의 관절 위치에 지름 8 cm 마젠타 구로만 표시된다.
- Material Variant의 **Slot Name**도 같은 이름을 쓴다. 이름은 영문·고유·고정(나중에 바꾸면 Parts/Variants가 끊긴다). 보통 3~8개(슬롯마다 드로우 콜이 늘어난다).

**확인 방법**: 메시 에디터 → Asset Details → **Material Slots** 목록의 이름이 의도한 파츠 이름과 같다. 뷰어에서 파츠 클릭 시 그 부위 전체가 마젠타.

## 6. 텍스처

- 크기는 2의 거듭제곱(1024 / **2048** / **4096**), 정사각형 권장. 얼굴·몸 같은 주 파츠 4096, 작은 파츠 2048 이하. PNG/TGA(8비트), 높이·HDR이 필요하면 EXR.
- **sRGB 켬**: Base Color(Albedo), Emissive. **sRGB 끔**: Normal, Roughness/Metallic/AO, 이들을 한 장에 묶은 ORM/MRA 마스크.
- Normal: Compression **Normalmap**, **DirectX(Y-) 방식**(Substance 내보내기에서 DirectX 선택). 마스크(ORM/MRA): Compression **Masks (no sRGB)**.
- 이름 예(Manny 관례): `T_<캐릭터>_<파츠>_D`(색), `_N` 또는 `_BN`(노멀), `_MRA`(Metallic·Roughness·AO).

**확인 방법**: 텍스처 더블클릭 → Details의 **sRGB**와 **Compression Settings**가 위 규칙과 같다. 노멀이 뒤집혀 보이면(요철이 반대) Green 채널 방향(OpenGL/DirectX)을 확인한다.

## 7. LOD (선택)

뷰어는 가까이서 보므로 LOD0만 있어도 된다. 필요하면 DCC에서 `_LOD1…`을 만들어 함께 가져오거나 메시 에디터 **LOD Settings → Number of LODs**로 자동 생성한다. 뷰어는 LOD 개수를 INSPECTION에 보여 주고, **L** 키(`LOD: Auto (L)`)로 Auto → LOD0 → LOD1 → … 를 강제로 표시해 LOD별 모양과 삼각형 수를 확인할 수 있다(9절).

## 8. 애니메이션 가져오기

- 메시와 **같은 Skeleton**으로 가져온다(FBX를 끌어다 놓을 때 Skeleton 지정, 애니메이션만 있는 FBX면 메시 없이 Animation만 생성). DCC에서 Bake Animation 켬, 30 fps 권장.
- 뷰어용 클립은 **제자리**여야 한다: AnimSequence 더블클릭 → Asset Details → Root Motion → **Enable Root Motion 끔**. 클립이 루트를 앞으로 옮기면 **Force Root Lock** 켬(Manny의 Walk/Jog 클립도 제자리 재생). 그렇지 않으면 캐릭터가 화면 밖으로 걸어 나간다.
- 포즈 한 장은 1프레임 클립으로 넣고 Data Asset Animations 행에서 **Is Pose** + **Pose Time**.

**확인 방법**: AnimSequence 미리보기에서 캐릭터가 제자리에서 움직인다. 뷰어 ANIMATION 버튼으로 재생된다(Skeleton이 맞지 않으면 그 클립은 재생되지 않고 로그에 경고).

## 9. 뷰어가 메시에서 자동으로 재는 값

손으로 적지 않아도 메시에서 측정하는 값(`GetMeshStats` / `GetSlotStats` / `GetPartMeasuredStats`): **삼각형·정점 수**(기본 LOD0, L로 강제한 LOD가 있으면 그 LOD 기준), **본 수**, **Material Slot 수**와 슬롯별 삼각형·재질 이름·텍스처 해상도 요약, **LOD 수**, **Morph 수**, **키(Height, Import 경계 높이 cm)**, Skeleton/Physics Asset 이름. 파츠 수치는 그 파츠의 **Material Slot Names**를 합산한다(5절이 중요한 이유).
Inspection(I)을 켜면 INSPECTION 패널이 이 측정값(메시 전체 요약 + 슬롯별 줄)을 항상 보여 준다. 선택한 파츠는 Material Slot Names가 메시 슬롯과 맞으면 `Measured: …`(실측), 아니면 Parts에 손으로 적은 메모(**Triangle Count / Material Name / Texture Resolution**)를 `Authored: …`로 구분해 보여 준다([CHARACTER_VIEWER_SETUP.md](CHARACTER_VIEWER_SETUP.md) 2절 ⑦). 메모를 쓴다면 placeholder의 숫자를 남겨 두지 말고 내 캐릭터 기준으로 다시 적는다.

## 10. 커밋 전 체크리스트

- [ ] `git lfs install`을 한 번 했다. `.uasset`/`.umap`은 `.gitattributes`에 의해 LFS로 올라간다 — `git add` 후 `git lfs status`에 내 에셋이 LFS로 보인다.
- [ ] **원본 제작 파일은 저장소에 넣지 않는다.** `.gitignore`가 `*.ztl *.zpr *.ma *.mb *.spp *.psd *.psb *.blend *.blend1 *.fbx *.obj *.abc *.tif *.tiff *.exr *.sbs *.sbsar`를 막지만 원본 PNG/TGA는 막지 않는다 → 원본은 전부 저장소 밖 폴더(예: `D:\Art\<캐릭터>\`)에 둔다. 가져온 데이터는 `.uasset` 안에 들어 있으므로 FBX가 없어도 프로젝트는 열린다.
- [ ] `Saved/`, `Intermediate/`, `Binaries/`, `DerivedDataCache/`는 커밋하지 않는다(이미 무시됨).
- [ ] `Tools\ValidateProfiles.bat` → 내 에셋 **E=0**.
- [ ] `git status`에 의도한 파일만 있다: `Content/Portfolio/Characters/<캐릭터>/…`, `Content/Portfolio/Data/DA_<캐릭터>.uasset`, (등록했다면) `Content/Portfolio/Blueprints/BP_CharacterViewerGameMode.uasset`, `Content/Portfolio/Maps/LV_Portfolio.umap`. `.uasset`/`.umap`은 병합이 안 되므로 GameMode·레벨을 다른 사람과 동시에 고치지 않는다.

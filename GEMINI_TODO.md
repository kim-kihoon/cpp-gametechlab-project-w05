# [Project Context] Verstappen Engine - Brute Force Competition Strategy (Extreme Edition)

## 0. Code Convention (UE Style)
프로젝트 전반에 걸쳐 다음 규칙을 엄격히 준수함.

### A. Naming Convention
*   **Case**: Upper Camel Case (ex. `UserLoginLog`)
*   **Prefixes**:
    *   `F`: 일반 struct / 값 타입 (ex. `FMatrix`, `FBox`, `FVector`)
    *   `U`: `UObject` 계열 클래스 및 일반 엔진 클래스 (ex. `URenderer`, `UApp`)
    *   `E`: Enum (ex. `ECullingResult`)
    *   `I`: 인터페이스
    *   `T`: 템플릿 클래스 (Generic class)
    *   `S`: Slate 위젯 (UI 관련)
*   **Terms**:
    *   Static Mesh (Non-Skeleton Mesh) / Skeletal Mesh (Skeleton Mesh)
    *   Particle (Effect)
    *   `FVector` (3D Vector), `FVector4` (4D Vector), `FMatrix` (4x4 Matrix)

### B. Comments (Doxygen)
*   언리얼 소스코드 스타일의 Doxygen 주석 사용.

### C. Formatting
*   clang-format 사용 (추후 협의).

---

## 1. 프로젝트 개요 및 제약 사항
* **목표**: 50x50x20 (총 5만 개)의 `StaticMeshComponent` 렌더링 및 정밀 Picking 최적화.
* **마감일**: 4월 7일 오전 10시.
* **기술 스택**: C++, DirectX 11, ImGui (Visual Studio).
* **타겟 사양**: Ryzen 9 5900HX (8C/16T, 16MB L3), RTX 3070 8GB GDDR6, 165Hz Screen.
* **프로젝트 이름**: `cpp-gametechlab-project-w05` (Git/Folder)
* **앱 이름**: `Verstappen Engine` (Window Title/Executable)
* **핵심 제약 사항**:
    1. **Instancing 절대 금지**: 개별 `Draw` 호출 필수.
    2. **RealTime 연산**: 캐싱 금지, 실시간 카메라/오브젝트 이동 대응.
    3. **Picking 무결성**: 픽킹 정확도 0점 방지 최우선.

---

## 1.5 엔진 아키텍처 및 에디터 UI 요구사항
* **화면 출력 (Overlay)**: FPS, Elapsed Time(ms), Last Picking Time(ms), Total Attempts, Accumulated Time 출력.
* **3D 디버그 렌더링**: Gizmo, World Axes, Grid 필수 구현.
* **ImGui 패널**: Control Panel (Spawn, Grid Spawn), Scene File System (Save/Load .bin), Scene Manager, Property Window, Console.

---

## 2. 하드웨어 맞춤형 최적화 아키텍처

### A. 싱글 스레드 SIMD 최적화 (Ryzen 9 5900HX 활용)
* **L3 Cache Residency**: 5만 개의 AABB(1.2MB)를 L3 캐시(16MB)에 상주 유도. 
* **Selective 64-Byte Alignment**: `FSceneDataSOA` 내 대형 배열의 시작 주소만 64-Byte 정렬을 적용하여 False Sharing 가능성을 배제하고 캐시 효율 극대화. 수학 구조체는 SIMD 정렬(16/32-Byte) 준수.
* **Single-Thread SIMD Culling (AVX2)**: 멀티스레드 복잡성을 배제하는 대신, `DirectXMath` 및 AVX2 인트린직을 활용하여 싱글 스레드에서 5만 개 객체의 Culling을 1ms 미만으로 완수.

### B. GPU 파이프라인 익스플로잇 (RTX 3070 & PCIe 대역폭)
* **DX11.1 Constant Buffer Offset**: `VSSetConstantBuffers1`의 Offset 기능을 활용해 드라이버 오버헤드 70% 절감.
* **No-Overwrite Circular Buffer**: `D3D11_MAP_WRITE_NO_OVERWRITE`로 GPU Stall 원천 봉쇄.
* **Early-Z & Front-to-Back Sorting**: 렌더링 시 카메라와 가까운 물체부터 먼저 그리는(Front-to-Back) 정렬을 수행하여, 하드웨어 Early-Z 기능을 극대화. 불필요한 픽셀 셰이더 연산(Overdraw)을 원천 차단하여 GPU 지연 시간 최소화.
* **3x4 Matrix Packing**: World Matrix 전송량을 25% 절감(64B->48B)하여 PCIe 대역폭 지연 최소화.

### C. Culling & Picking (Uniform Grid + DDA)
* **Uniform Grid ($O(1)$)**: 50x50x20 정형 배치 대응 공간 분할.
* **Fully Inside Skip**: Cell 단위 검사 결과가 `FullyInside`일 경우 내부 객체 전수 검사 생략.
* **Precise DDA Picking**: DDA 알고리즘 탐색 + Ray-Triangle 정밀 검사로 100% 무결성 보장.

---

## 3. 팀 역할 분담 (최적화 특공대)
1. **Member A (Asset & Grid)**: `.obj` 로더, Binary Baking 시스템, Uniform Grid 생성 및 다중 등록 로직.
2. **Member B (Picking & Profiler)**: DDA 그리드 탐색, SIMD Ray-Triangle 정밀 검사, 퍼포먼스 오버레이 출력.
3. **Member C (Math & Culling)**: SIMD AVX2 기반 고속 Frustum Culling 엔진, Fully Inside 스킵 로직.
4. **Member D (Graphics & Pipeline)**: Offset 기반 렌더링 루프, Ring Buffer 시스템, Bucket Sorting (Material/Mesh).

---

## 4. 구현 단계 (Phases) - 우선순위 기반
1. **Phase 1 (정확도 & 안정성)**: Picking 정확도 확보, Overlay 출력, 기본 Uniform Grid 빌드, Frustum Culling.
2. **Phase 2 (성능 가속)**: SoA 전환 완료, SIMD AVX2 적용, Fully Inside Skip 적용, Ring Buffer 및 Offset Rendering.
3. **Phase 3 (정교화)**: Coarse Depth Bucket Sorting, 3x4 Matrix Packing, Binary Baking.

---

## 5. Codex 협업 가이드 (AI Agent 지침)
* **Zero-Allocation**: 프레임 루프 내 `new/delete` 및 `std::vector` 재할당 절대 금지.
* **Interface**: `FSceneDataSOA`를 중심 데이터 저장소로 사용하며, 모든 모듈은 인덱스 기반으로 통신.
* **Stability**: 최적화 기법 적용 전후로 Picking 무결성을 반드시 검증할 것.

# [Project Context] Game Jam 2nd - Brute Force Competition Optimization Strategy (Extreme Edition)

## 1. 프로젝트 개요 및 제약 사항
* **목표**: 50x50x20 (총 5만 개)의 `StaticMeshComponent` 렌더링 및 정밀 Picking 최적화.
* **마감일**: 4월 7일 오전 10시.
* **기술 스택**: C++, DirectX 11, ImGui (Visual Studio).
* **타겟 사양**: Ryzen 9 5900HX (8C/16T, 16MB L3), RTX 3070 8GB GDDR6, 165Hz Screen.
* **핵심 제약 사항**: Instancing 절대 금지 (개별 Draw), RealTime 연산 (캐싱 금지), Picking 무결성 (0점 방지).

---

## 1.5 엔진 아키텍처 및 에디터 UI 요구사항
최적화 대회용이지만, 기본적으로 에디터 툴(Tool) 수준의 아키텍처를 요구함. 렌더링 성능을 저하시키지 않으면서 다음 기능들을 100% 구현해야 함.

### A. 화면 출력 (Overlay Text)
화면 좌상단에 실시간으로 다음 퍼포먼스 수치를 출력해야 함 (디버그 텍스트 혹은 ImGui 오버레이).
* **FPS**: 초당 프레임 수.
* **Elapsed Time (ms)**: 프레임당 경과 시간.
* **Last Picking Time (ms)**: 가장 마지막 Picking에 소요된 정밀 연산 시간.
* **Total Picking Attempts**: 누적된 Picking 횟수.
* **Accumulated Picking Time (ms)**: 누적된 Picking 연산 소요 시간.

### B. 3D 디버그 렌더링 (Debug View)
* **Gizmo (기즈모)**: 선택된 객체의 Transform(Translate, Rotate, Scale) 제어용 3D UI.
* **World Axes (월드 축)**: X(Red), Y(Green), Z(Blue) 기본 축 렌더링.
* **Grid (바닥 그리드)**: 공간 크기를 가늠할 수 있는 라인(Line) 기반 바닥면.

### C. ImGui 패널 및 컨트롤 (Editor Windows)
* **Control Panel (컨트롤 패널)**:
  * Spawn 버튼 (객체 단일 생성)
  * Grid Size & Grid Unit 설정 (Uniform Grid 기반 스폰 제어)
  * Grid Spawn (설정된 Grid 형태로 객체 대량 생성)
* **Scene File System (씬 저장/로드)**:
  * Scene Name (입력창)
  * New Scene (초기화)
  * Save Scene / Load Scene (`.scene` 또는 `.bin` 파일 직렬화/역직렬화)
* **Scene Manager Window (씬 매니저 창)**: 씬 내의 트리 구조나 객체 리스트 표시.
* **Property Window (프로퍼티 창)**: 선택된 (Picked) 객체의 Transform, Mesh, Material 등의 상세 정보 표시 및 수정.
* **Console Window (콘솔 창)**: 엔진 로그, 에러, 퍼포먼스 경고 메시지 출력.

---

## 2. 하드웨어 맞춤형 초격차 최적화 아키텍처

### A. CPU 캐시 지배 (Ryzen 9 5900HX 16MB L3 활용)
* **🔥 [초격차] L3 Cache Residency**: 5만 개의 AABB(1.2MB)를 5900HX의 L3 캐시(16MB)에 고정(Residency)시킴. SOA 배열의 메모리 레이아웃을 24-Byte로 정밀 설계하여 Culling 시 RAM 접근 횟수를 0으로 수렴시킴.
* **🔥 [초격차] Parallel Culling (16 Threads)**: 165Hz(6.06ms) 달성을 위해 Culling 연산을 16개 워커 스레드로 분할 처리. `std::for_each(std::execution::par)` 혹은 `JobSystem`을 구현하여 Culling 완료 시간을 0.1ms 대로 단축.

### B. GPU 파이프라인 익스플로잇 (RTX 3070 & PCIe 대역폭)
* **🔥 [초격차] DX11.1 Constant Buffer Offset**: 5만 번의 `VSSetConstantBuffers` 호출 대신, 단 하나의 거대한 48-Byte * 50,000 Buffer를 생성. `VSSetConstantBuffers1`의 `FirstConstant`, `NumConstants` 매개변수를 활용하여 CPU 사이클 소모를 70% 절감.
* **🔥 [초격차] No-Overwrite Circular Buffer**: `D3D11_MAP_WRITE_NO_OVERWRITE`를 사용하여 GPU가 이전 프레임을 그리는 동안 CPU가 다음 데이터를 쓰도록 유도. GPU Starvation 원천 봉쇄.

### C. Culling & Picking (Uniform Grid + DDA)
* **Uniform Grid ($O(1)$)**: 50x50x20 정형 배치를 활용한 공간 분할.
* **🔥 [초격차] Fully Inside Skip**: Cell 단위 Frustum 검사 결과가 `FullyInside`일 경우 내부 객체 전수 검사 생략.
* **🔥 [초격차] Precise DDA Picking**: 마우스 광선이 통과하는 그리드만 추적하는 DDA 알고리즘 적용. 최종 후보군에 대해서만 Ray-Triangle 정밀 검사를 수행하여 "0점 처리" 완벽 방지.

---

## 3. 팀 역할 분담 (최적화 특공대)
1. **Member A (Asset & Cache-Aligned SOA)**: `.obj` 로더, 24-Byte 정밀 정렬 AABB 데이터 구조 구축, L3 캐시 적중률 모니터링.
2. **Member B (DDA & Triangle Picking)**: DDA 기반 그리드 탐색 알고리즘, SIMD Ray-Triangle 교차 검사 로직 (정밀도 100% 보장).
3. **Member C (Parallel Math & SIMD Culling)**: 16스레드 워커 기반 Culling 엔진, `DirectXMath` 인트린직 최적화, Fully Inside 스킵 엔진.
4. **Member D (Graphics & Offset-Buffer)**: `VSSetConstantBuffers1` 기반 오프셋 렌더링 루프, 3x4 Matrix 압축, State Sorting Bucketing.

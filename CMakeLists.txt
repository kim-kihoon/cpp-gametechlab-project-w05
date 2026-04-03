cmake_minimum_required(VERSION 3.15)
project(cpp-gametechlab-project-w05)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 소스 파일 목록
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "include/*.h"
)

add_executable(VerstappenEngine WIN32 ${SOURCES})

# 헤더 경로 설정
target_include_directories(VerstappenEngine PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# 비주얼 스튜디오 빌드 플래그 (구성별 분리)
if(MSVC)
    # 공통 옵션 (디버그/릴리즈 공통)
    target_compile_definitions(VerstappenEngine PRIVATE UNICODE _UNICODE)
    target_compile_options(VerstappenEngine PRIVATE
        /MP                # 멀티 프로세서 빌드
        /W4                # 경고 수준
        /permissive-       # 표준 준수
    )

    # [수정] 릴리즈(Release) 모드에서만 최적화 적용
    target_compile_options(VerstappenEngine PRIVATE
        $<$<CONFIG:Release>:
            /Ox                # 최대한 최적화
            /Ob2               # 인라인 확장
            /Oi                # 내장 함수 사용
            /Ot                # 속도 우선
            /GL                # 전체 프로그램 최적화
            /fp:fast           # [핵심] 부동 소수점 고속 모드
            /arch:AVX2         # [핵심] Ryzen 5900HX AVX2 명령어 사용
        >
    )
    
    # 링크 단계 최적화 (릴리즈 전용)
    set_target_properties(VerstappenEngine PROPERTIES 
        LINK_FLAGS_RELEASE "/LTCG"
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()

# 필수 시스템 라이브러리 링크
target_link_libraries(VerstappenEngine PRIVATE
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
)

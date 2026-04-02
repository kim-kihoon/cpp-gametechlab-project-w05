cmake_minimum_required(VERSION 3.15)
project(VerstappenEngine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 소스 파일 목록 (자동 스캔 방식)
# [VerstappenEngine] 새로운 파일 추가 시 이 부분을 저장하면 자동으로 리스트에 추가됩니다.
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "include/*.h"
)

add_executable(${PROJECT_NAME} WIN32 ${SOURCES})

# 헤더 경로 설정
target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# [초격차] 비주얼 스튜디오 빌드 플래그 (구성별 분리)
if(MSVC)
    # 공통 옵션 (디버그/릴리즈 공통)
    target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /MP                # 멀티 프로세서 빌드
        /W4                # 경고 수준
        /permissive-       # 표준 준수
    )

    # [수정] 릴리즈(Release) 모드에서만 최적화 적용
    # 이 설정이 되어야 Debug 모드에서 에러 없이 디버깅이 가능합니다.
    target_compile_options(${PROJECT_NAME} PRIVATE
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
    set_target_properties(${PROJECT_NAME} PROPERTIES 
        LINK_FLAGS_RELEASE "/LTCG"
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()

# 필수 시스템 라이브러리 링크
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
)

cmake_minimum_required(VERSION 3.15)
project(cpp-gametechlab-project-w05)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "include/*.h"
)

add_executable(VerstappenEngine WIN32 ${SOURCES})

target_include_directories(VerstappenEngine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_definitions(VerstappenEngine PRIVATE UNICODE _UNICODE)
    target_compile_options(VerstappenEngine PRIVATE
        /MP
        /W4
        /utf-8
        /permissive-
    )

    target_compile_options(VerstappenEngine PRIVATE
        $<$<CONFIG:Release>:
            /Ox
            /Ob2
            /Oi
            /Ot
            /GL
            /fp:fast
            /arch:AVX2
        >
    )

    set_target_properties(VerstappenEngine PROPERTIES
        LINK_FLAGS_RELEASE "/LTCG"
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    )
endif()

target_link_libraries(VerstappenEngine PRIVATE
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
)

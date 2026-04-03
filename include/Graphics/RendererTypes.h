#pragma once

namespace Graphics
{
    /**
     * 디버그 렌더링 토글 상태를 저장하는 구조체.
     */
    struct FDebugRenderSettings
    {
        bool bDrawGizmo = true;
        bool bDrawWorldAxes = true;
        bool bDrawGrid = true;
    };
}

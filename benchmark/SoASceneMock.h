#pragma once
#include "BenchmarkCommon.h"

// SoA: 속성별로 분리된 배열
// 메모리: [x0 x1 x2...][y0 y1 y2...][visible0 visible1 visible2...]
// visible 같이 자주 확인하는 속성만 순회할 때 캐시 효율 극대화

struct SoAScene
{
    // transform 속성 배열
    std::vector<float> tx, ty, tz;
    std::vector<float> yaw, pitch;

    // render object 속성 배열
    std::vector<uint32> transformIndex;
    std::vector<uint32> materialID;
    std::vector<uint32> meshID;
    std::vector<uint8>  visible; // bool 대신 uint8 — vector<bool>의 비트 압축 회피

    void Setup(uint32 count)
    {
        tx.resize(count); ty.resize(count); tz.resize(count);
        yaw.resize(count); pitch.resize(count);
        transformIndex.resize(count);
        materialID.resize(count);
        meshID.resize(count);
        visible.resize(count);

        for (uint32 i = 0; i < count; ++i)
        {
            tx[i] = ty[i] = tz[i] = float(i);
            yaw[i] = pitch[i] = 0.f;
            transformIndex[i] = i;
            materialID[i]     = i % 16;
            meshID[i]         = i % 8;
            visible[i]        = (i % 2 == 0) ? 1 : 0;
        }
    }
};

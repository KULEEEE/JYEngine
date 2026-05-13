#pragma once
#include "BenchmarkCommon.h"

// AoS: 객체 단위로 묶어 연속 배열에 저장 — 현재 JYEngine JPool과 동일한 방식
// 메모리: [obj0_전체][obj1_전체][obj2_전체]...

struct AoSTransform
{
    float x = 0.f, y = 0.f, z = 0.f;
    float yaw = 0.f, pitch = 0.f;
};

struct AoSRenderObject
{
    uint32 transformIndex = 0; // 포인터 대신 index로 참조
    uint32 materialID     = 0;
    uint32 meshID         = 0;
    bool   visible        = false;
};

struct AoSScene
{
    std::vector<AoSTransform>    transforms;
    std::vector<AoSRenderObject> objects;

    void Setup(uint32 count)
    {
        transforms.resize(count);
        objects.resize(count);

        for (uint32 i = 0; i < count; ++i)
        {
            transforms[i] = { float(i), float(i), float(i), 0.f, 0.f };
            objects[i]    = { i, i % 16, i % 8, (i % 2 == 0) };
        }
    }
};

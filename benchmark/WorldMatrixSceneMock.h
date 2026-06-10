#pragma once
#include "BenchmarkCommon.h"
#include "MatrixMath.h"

// JScene의 transform 풀을 모사한 SoA 구조.
// tx/ty/tz, rx/ry/rz, sx/sy/sz 입력 → worldMatrices[] 출력.
// JRenderServer::SyncScene의 transform 루프와 동일한 데이터 패턴.

struct WorldMatrixScene
{
    std::vector<float> tx, ty, tz;
    std::vector<float> rx, ry, rz;
    std::vector<float> sx, sy, sz;
    std::vector<Mat4>  worldMatrices;

    void Setup(uint32 count)
    {
        tx.resize(count); ty.resize(count); tz.resize(count);
        rx.resize(count); ry.resize(count); rz.resize(count);
        sx.resize(count); sy.resize(count); sz.resize(count);
        worldMatrices.resize(count);

        for (uint32 i = 0; i < count; ++i)
        {
            tx[i] = float(i) * 0.1f;
            ty[i] = float(i) * 0.05f;
            tz[i] = float(i) * 0.2f;
            rx[i] = float(i) * 0.01f;
            ry[i] = float(i) * 0.02f;
            rz[i] = float(i) * 0.015f;
            sx[i] = sy[i] = sz[i] = 1.0f;
        }
    }
};

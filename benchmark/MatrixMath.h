#pragma once
#include <cmath>

// 엔진의 makeWorldMatrix와 동일한 연산량을 재현하는 경량 행렬 라이브러리.
// DirectX 없이 macOS/Windows 모두에서 동작한다 (벤치마크 전용).

struct Mat4
{
    float m[16] = {};
};

// row-major 4x4 곱셈
inline Mat4 mat4Mul(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
        {
            float sum = 0.f;
            for (int k = 0; k < 4; ++k)
                sum += a.m[row * 4 + k] * b.m[k * 4 + col];
            r.m[row * 4 + col] = sum;
        }
    return r;
}

inline Mat4 mat4Scale(float sx, float sy, float sz)
{
    Mat4 r;
    r.m[0] = sx; r.m[5] = sy; r.m[10] = sz; r.m[15] = 1.f;
    return r;
}

// XMMatrixRotationRollPitchYaw(rx, ry, rz) 동등 연산 — sinf/cosf 6회 호출
inline Mat4 mat4RotateRPY(float rx, float ry, float rz)
{
    const float cx = cosf(rx), sx = sinf(rx);
    const float cy = cosf(ry), sy = sinf(ry);
    const float cz = cosf(rz), sz = sinf(rz);

    Mat4 r;
    r.m[0]  =  cy * cz + sx * sy * sz;
    r.m[1]  =  cx * sz;
    r.m[2]  = -sy * cz + sx * cy * sz;
    r.m[4]  = -cy * sz + sx * sy * cz;
    r.m[5]  =  cx * cz;
    r.m[6]  =  sy * sz + sx * cy * cz;
    r.m[8]  =  cx * sy;
    r.m[9]  = -sx;
    r.m[10] =  cx * cy;
    r.m[15] =  1.f;
    return r;
}

inline Mat4 mat4Translate(float tx, float ty, float tz)
{
    Mat4 r;
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
    r.m[12] = tx; r.m[13] = ty; r.m[14] = tz;
    return r;
}

// JRenderServer::makeWorldMatrix와 동일: scale * rotation * translation
inline Mat4 mat4World(
    float tx, float ty, float tz,
    float rx, float ry, float rz,
    float sx, float sy, float sz)
{
    return mat4Mul(mat4Mul(mat4Scale(sx, sy, sz), mat4RotateRPY(rx, ry, rz)),
                   mat4Translate(tx, ty, tz));
}

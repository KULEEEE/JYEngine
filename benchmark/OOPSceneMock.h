#pragma once
#include "BenchmarkCommon.h"

// OOP: 각 객체가 힙에 개별 할당되고, transform은 포인터로 참조
// 실제 OOP 코드베이스의 pointer chasing 구조를 흉내냄

struct OOPTransform
{
    float x = 0.f, y = 0.f, z = 0.f;
    float yaw = 0.f, pitch = 0.f;
};

struct OOPRenderObject
{
    OOPTransform* transform = nullptr;  // 포인터 참조 — 접근 시 점프 발생
    uint32        materialID = 0;
    uint32        meshID     = 0;
    bool          visible    = false;
};

class OOPScene
{
public:
    std::vector<OOPRenderObject*> objects;
    std::vector<OOPTransform*>    transforms;

    OOPScene() = default;
    ~OOPScene() { Clear(); }
    OOPScene(const OOPScene&)            = delete;
    OOPScene& operator=(const OOPScene&) = delete;

    void Setup(uint32 count)
    {
        Clear();
        objects.reserve(count);
        transforms.reserve(count);

        for (uint32 i = 0; i < count; ++i)
        {
            auto* t        = new OOPTransform{ float(i), float(i), float(i), 0.f, 0.f };
            auto* obj      = new OOPRenderObject;
            obj->transform  = t;
            obj->materialID = i % 16;
            obj->meshID     = i % 8;
            obj->visible    = (i % 2 == 0); // 약 50%가 visible
            transforms.push_back(t);
            objects.push_back(obj);
        }
    }

private:
    void Clear()
    {
        for (auto* p : objects)    delete p;
        for (auto* p : transforms) delete p;
        objects.clear();
        transforms.clear();
    }
};

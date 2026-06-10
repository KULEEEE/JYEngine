#include <iostream>
#include <atomic>
#include "BenchmarkCommon.h"
#include "OOPSceneMock.h"
#include "AoSSceneMock.h"
#include "SoASceneMock.h"
#include "WorldMatrixSceneMock.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

// -----------------------------------------------------------------------
// Render Item Gather
// visible 오브젝트를 추려 transform position + material/mesh 정보를 수집
// -----------------------------------------------------------------------

static void GatherOOP(const OOPScene& scene, std::vector<GatherResult>& out)
{
    out.clear();
    for (const auto* obj : scene.objects)
    {
        if (obj->visible)
            out.push_back({ obj->transform->x, obj->transform->y, obj->transform->z,
                            obj->materialID, obj->meshID });
    }
}

static void GatherAoS(const AoSScene& scene, std::vector<GatherResult>& out)
{
    out.clear();
    for (const auto& obj : scene.objects)
    {
        if (obj.visible)
        {
            const auto& t = scene.transforms[obj.transformIndex];
            out.push_back({ t.x, t.y, t.z, obj.materialID, obj.meshID });
        }
    }
}

static void GatherSoA(const SoAScene& scene, std::vector<GatherResult>& out)
{
    out.clear();
    const uint32 count = static_cast<uint32>(scene.visible.size());
    for (uint32 i = 0; i < count; ++i)
    {
        if (scene.visible[i])
        {
            const uint32 ti = scene.transformIndex[i];
            out.push_back({ scene.tx[ti], scene.ty[ti], scene.tz[ti],
                            scene.materialID[i], scene.meshID[i] });
        }
    }
}

// 슬롯을 미리 할당하고 atomic 인덱스로 충돌 없이 병렬 수집
static void GatherSoA_TBB(const SoAScene& scene, std::vector<GatherResult>& out)
{
    const uint32 total = static_cast<uint32>(scene.visible.size());
    out.resize(total);
    std::atomic<uint32> writeIdx{ 0 };

    tbb::parallel_for(tbb::blocked_range<uint32>(0, total),
        [&](const tbb::blocked_range<uint32>& r)
        {
            for (uint32 i = r.begin(); i < r.end(); ++i)
            {
                if (scene.visible[i])
                {
                    const uint32 ti   = scene.transformIndex[i];
                    const uint32 slot = writeIdx.fetch_add(1, std::memory_order_relaxed);
                    out[slot] = { scene.tx[ti], scene.ty[ti], scene.tz[ti],
                                  scene.materialID[i], scene.meshID[i] };
                }
            }
        });

    out.resize(writeIdx.load(std::memory_order_relaxed));
}

// -----------------------------------------------------------------------
// Transform Update
// 모든 transform의 yaw에 delta를 더함
// -----------------------------------------------------------------------

static void UpdateOOP(OOPScene& scene, float delta)
{
    for (auto* t : scene.transforms)
        t->yaw += delta;
}

static void UpdateAoS(AoSScene& scene, float delta)
{
    for (auto& t : scene.transforms)
        t.yaw += delta;
}

static void UpdateSoA(SoAScene& scene, float delta)
{
    for (auto& y : scene.yaw)
        y += delta;
}

static void UpdateSoA_TBB(SoAScene& scene, float delta)
{
    const uint32 count = static_cast<uint32>(scene.yaw.size());
    tbb::parallel_for(tbb::blocked_range<uint32>(0, count),
        [&](const tbb::blocked_range<uint32>& r)
        {
            for (uint32 i = r.begin(); i < r.end(); ++i)
                scene.yaw[i] += delta;
        });
}

// -----------------------------------------------------------------------
// Benchmark runners
// -----------------------------------------------------------------------

static void RunGatherBenchmark(std::vector<BenchmarkRecord>& records)
{
    std::cout << "\n[Render Item Gather]\n";

    for (uint32 count : OBJECT_COUNTS)
    {
        OOPScene oop; oop.Setup(count);
        AoSScene aos; aos.Setup(count);
        SoAScene soa; soa.Setup(count);

        std::vector<GatherResult> results;
        results.reserve(count);

        volatile uint32 sink = 0;

        double msOOP = MeasureMs([&]() {
            GatherOOP(oop, results);
            sink += static_cast<uint32>(results.size());
        });

        double msAoS = MeasureMs([&]() {
            GatherAoS(aos, results);
            sink += static_cast<uint32>(results.size());
        });

        double msSoA = MeasureMs([&]() {
            GatherSoA(soa, results);
            sink += static_cast<uint32>(results.size());
        });

        double msTBB = MeasureMs([&]() {
            GatherSoA_TBB(soa, results);
            sink += static_cast<uint32>(results.size());
        });

        records.push_back({ "GatherOOP",     count, msOOP });
        records.push_back({ "GatherAoS",     count, msAoS });
        records.push_back({ "GatherSoA",     count, msSoA });
        records.push_back({ "GatherSoA_TBB", count, msTBB });

        std::cout << "  objects=" << count
                  << "  OOP=" << std::fixed << std::setprecision(4) << msOOP << "ms"
                  << "  AoS=" << msAoS << "ms"
                  << "  SoA=" << msSoA << "ms"
                  << "  TBB=" << msTBB << "ms\n";
    }
}

static void RunTransformUpdateBenchmark(std::vector<BenchmarkRecord>& records)
{
    std::cout << "\n[Transform Update]\n";

    constexpr float delta = 0.01f;

    for (uint32 count : OBJECT_COUNTS)
    {
        OOPScene oop; oop.Setup(count);
        AoSScene aos; aos.Setup(count);
        SoAScene soa; soa.Setup(count);

        double msOOP = MeasureMs([&]() { UpdateOOP(oop, delta); });
        double msAoS = MeasureMs([&]() { UpdateAoS(aos, delta); });
        double msSoA = MeasureMs([&]() { UpdateSoA(soa, delta); });
        double msTBB = MeasureMs([&]() { UpdateSoA_TBB(soa, delta); });

        records.push_back({ "UpdateOOP",     count, msOOP });
        records.push_back({ "UpdateAoS",     count, msAoS });
        records.push_back({ "UpdateSoA",     count, msSoA });
        records.push_back({ "UpdateSoA_TBB", count, msTBB });

        std::cout << "  objects=" << count
                  << "  OOP=" << std::fixed << std::setprecision(4) << msOOP << "ms"
                  << "  AoS=" << msAoS << "ms"
                  << "  SoA=" << msSoA << "ms"
                  << "  TBB=" << msTBB << "ms\n";
    }
}

// -----------------------------------------------------------------------
// World Matrix Update
// JRenderServer::SyncScene의 transform 루프와 동일한 연산:
//   makeWorldMatrix = scale * rotateRPY * translate
// -----------------------------------------------------------------------

static void UpdateWorldMatrix_Sequential(WorldMatrixScene& scene)
{
    const uint32 count = static_cast<uint32>(scene.tx.size());
    for (uint32 i = 0; i < count; ++i)
    {
        scene.worldMatrices[i] = mat4World(
            scene.tx[i], scene.ty[i], scene.tz[i],
            scene.rx[i], scene.ry[i], scene.rz[i],
            scene.sx[i], scene.sy[i], scene.sz[i]);
    }
}

static void UpdateWorldMatrix_TBB(WorldMatrixScene& scene)
{
    const uint32 count = static_cast<uint32>(scene.tx.size());
    tbb::parallel_for(tbb::blocked_range<uint32>(0, count),
        [&](const tbb::blocked_range<uint32>& r)
        {
            for (uint32 i = r.begin(); i < r.end(); ++i)
            {
                scene.worldMatrices[i] = mat4World(
                    scene.tx[i], scene.ty[i], scene.tz[i],
                    scene.rx[i], scene.ry[i], scene.rz[i],
                    scene.sx[i], scene.sy[i], scene.sz[i]);
            }
        });
}

static void RunWorldMatrixBenchmark(std::vector<BenchmarkRecord>& records)
{
    std::cout << "\n[World Matrix Update  (scale * rotRPY * translate, sinf/cosf x6 + mat4Mul x2)]\n";

    for (uint32 count : OBJECT_COUNTS)
    {
        WorldMatrixScene scene;
        scene.Setup(count);

        volatile float sink = 0.f;

        double msSeq = MeasureMs([&]() {
            UpdateWorldMatrix_Sequential(scene);
            sink += scene.worldMatrices[0].m[0];
        });

        double msTBB = MeasureMs([&]() {
            UpdateWorldMatrix_TBB(scene);
            sink += scene.worldMatrices[0].m[0];
        });

        const double speedup = msSeq / msTBB;

        records.push_back({ "WorldMatrix_Sequential", count, msSeq });
        records.push_back({ "WorldMatrix_TBB",        count, msTBB });

        std::cout << "  objects=" << count
                  << "  Sequential=" << std::fixed << std::setprecision(4) << msSeq << "ms"
                  << "  TBB=" << msTBB << "ms"
                  << "  speedup=" << std::setprecision(2) << speedup << "x\n";
    }
}

// -----------------------------------------------------------------------
// Entry
// -----------------------------------------------------------------------

int main()
{
    std::cout << "=== JYEngine DOD Benchmark ===\n";
    std::cout << "repeat=" << REPEAT_COUNT << ", objects=";
    for (uint32 c : OBJECT_COUNTS) std::cout << c << " ";
    std::cout << "\n";

    std::vector<BenchmarkRecord> records;
    RunGatherBenchmark(records);
    RunTransformUpdateBenchmark(records);
    RunWorldMatrixBenchmark(records);

    PrintResults(records);
    WriteCSV("benchmark_results.csv", records);

    return 0;
}

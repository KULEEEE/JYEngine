#include <iostream>
#include "BenchmarkCommon.h"
#include "OOPSceneMock.h"
#include "AoSSceneMock.h"
#include "SoASceneMock.h"

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

        records.push_back({ "GatherOOP", count, msOOP });
        records.push_back({ "GatherAoS", count, msAoS });
        records.push_back({ "GatherSoA", count, msSoA });

        std::cout << "  objects=" << count
                  << "  OOP=" << std::fixed << std::setprecision(4) << msOOP << "ms"
                  << "  AoS=" << msAoS << "ms"
                  << "  SoA=" << msSoA << "ms\n";
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

        records.push_back({ "UpdateOOP", count, msOOP });
        records.push_back({ "UpdateAoS", count, msAoS });
        records.push_back({ "UpdateSoA", count, msSoA });

        std::cout << "  objects=" << count
                  << "  OOP=" << std::fixed << std::setprecision(4) << msOOP << "ms"
                  << "  AoS=" << msAoS << "ms"
                  << "  SoA=" << msSoA << "ms\n";
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

    PrintResults(records);
    WriteCSV("benchmark_results.csv", records);

    return 0;
}

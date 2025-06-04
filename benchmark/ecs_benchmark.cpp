#include <benchmark/benchmark.h>
#include <entt/entt.hpp>
#include "RTTM/Entity.hpp"
#include <vector>
#include <random>

// ================================
// RTTM ECS 组件定义
// ================================

// 位置组件
class Position : public RTTM::Component<Position>
{
public:
    float x, y, z;

    Position(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}

    std::string GetTypeName() const override { return "Position"; }
};

// 速度组件
class Velocity : public RTTM::Component<Velocity>
{
public:
    float dx, dy, dz;

    Velocity(float dx = 0.0f, float dy = 0.0f, float dz = 0.0f) : dx(dx), dy(dy), dz(dz) {}

    std::string GetTypeName() const override { return "Velocity"; }
};

// 生命值组件
class HealthComp : public RTTM::Component<HealthComp>
{
public:
    int hp, maxHp;

    HealthComp(int hp = 100, int maxHp = 100) : hp(hp), maxHp(maxHp) {}

    std::string GetTypeName() const override { return "HealthComp"; }
};

// RTTM 实体类 - 移除自动组件附加，保持公平性
class GameEntity : public RTTM::Entity
{
public:
    void Update(float deltaTime)
    {
        try
        {
            auto& pos = GetComponent<Position>();
            auto& vel = GetComponent<Velocity>();

            pos.x += vel.dx * deltaTime;
            pos.y += vel.dy * deltaTime;
            pos.z += vel.dz * deltaTime;
        }
        catch (...)
        {
            // 错误处理
        }
    }

    void TakeDamage(int damage)
    {
        try
        {
            auto& health = GetComponent<HealthComp>();
            health.hp = std::max(0, health.hp - damage);
        }
        catch (...)
        {
            // 错误处理
        }
    }
};

// ================================
// EnTT 组件定义（保持一致的数据结构）
// ================================

struct EnttPosition
{
    float x, y, z;
    EnttPosition(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
};

struct EnttVelocity
{
    float dx, dy, dz;
    EnttVelocity(float dx = 0.0f, float dy = 0.0f, float dz = 0.0f) : dx(dx), dy(dy), dz(dz) {}
};

struct EnttHealth
{
    int hp, maxHp;
    EnttHealth(int hp = 100, int maxHp = 100) : hp(hp), maxHp(maxHp) {}
};

// ================================
// 基准测试工具函数
// ================================

// 生成随机数据
std::vector<float> GenerateRandomFloats(size_t count)
{
    std::vector<float> values;
    values.reserve(count);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-100.0f, 100.0f);

    for (size_t i = 0; i < count; ++i)
    {
        values.push_back(dis(gen));
    }

    return values;
}

// ================================
// RTTM ECS 基准测试
// ================================

// RTTM 实体创建测试 - 现在手动添加组件确保公平
static void BM_RTTM_EntityCreation(benchmark::State& state)
{
    for (auto _ : state)
    {
        std::vector<GameEntity> entities;
        entities.reserve(state.range(0));

        for (int i = 0; i < state.range(0); ++i)
        {
            GameEntity entity;
            // 手动添加每个组件，与EnTT保持一致
            entity.AddComponent<Position>(i * 1.0f, i * 2.0f, i * 3.0f);
            entity.AddComponent<Velocity>(i * 0.1f, i * 0.2f, i * 0.3f);
            entity.AddComponent<HealthComp>(100, 100);
            entities.emplace_back(std::move(entity));
        }

        benchmark::DoNotOptimize(entities);
    }
}

// RTTM 组件访问测试
static void BM_RTTM_ComponentAccess(benchmark::State& state)
{
    std::vector<GameEntity> entities;
    entities.reserve(state.range(0));

    // 预先创建实体，确保所有实体都有相同的组件
    for (int i = 0; i < state.range(0); ++i)
    {
        GameEntity entity;
        entity.AddComponent<Position>(i * 1.0f, i * 2.0f, i * 3.0f);
        entity.AddComponent<Velocity>(i * 0.1f, i * 0.2f, i * 0.3f);
        entity.AddComponent<HealthComp>(100, 100);
        entities.emplace_back(std::move(entity));
    }

    for (auto _ : state)
    {
        float sum = 0.0f;
        for (auto& entity : entities)
        {
            try
            {
                // 只访问Position组件，与EnTT测试保持一致
                auto& pos = entity.GetComponent<Position>();
                sum += pos.x + pos.y + pos.z;
            }
            catch (...)
            {
                // 错误处理
            }
        }
        benchmark::DoNotOptimize(sum);
    }
}

// RTTM 系统更新测试
static void BM_RTTM_SystemUpdate(benchmark::State& state)
{
    std::vector<GameEntity> entities;
    entities.reserve(state.range(0));

    // 预先创建实体
    for (int i = 0; i < state.range(0); ++i)
    {
        GameEntity entity;
        entity.AddComponent<Position>(i * 1.0f, i * 2.0f, i * 3.0f);
        entity.AddComponent<Velocity>(i * 0.1f, i * 0.2f, i * 0.3f);
        entity.AddComponent<HealthComp>(100, 100);
        entities.emplace_back(std::move(entity));
    }

    const float deltaTime = 0.016f; // 60 FPS

    for (auto _ : state)
    {
        // 使用相同的更新逻辑
        for (auto& entity : entities)
        {
            entity.Update(deltaTime);
        }
    }
}

// ================================
// EnTT ECS 基准测试
// ================================

// EnTT 实体创建测试 - 添加相同数量的组件
static void BM_EnTT_EntityCreation(benchmark::State& state)
{
    for (auto _ : state)
    {
        entt::registry registry;

        for (int i = 0; i < state.range(0); ++i)
        {
            auto entity = registry.create();
            // 添加相同的三个组件
            registry.emplace<EnttPosition>(entity, i * 1.0f, i * 2.0f, i * 3.0f);
            registry.emplace<EnttVelocity>(entity, i * 0.1f, i * 0.2f, i * 0.3f);
            registry.emplace<EnttHealth>(entity, 100, 100);
        }

        benchmark::DoNotOptimize(registry);
    }
}

// EnTT 组件访问测试
static void BM_EnTT_ComponentAccess(benchmark::State& state)
{
    entt::registry registry;

    // 预先创建实体，确保所有实体都有相同的组件
    for (int i = 0; i < state.range(0); ++i)
    {
        auto entity = registry.create();
        registry.emplace<EnttPosition>(entity, i * 1.0f, i * 2.0f, i * 3.0f);
        registry.emplace<EnttVelocity>(entity, i * 0.1f, i * 0.2f, i * 0.3f);
        registry.emplace<EnttHealth>(entity, 100, 100);
    }

    for (auto _ : state)
    {
        float sum = 0.0f;
        // 只访问Position组件，与RTTM测试保持一致
        auto view = registry.view<EnttPosition>();
        for (auto entity : view)
        {
            auto& pos = view.get<EnttPosition>(entity);
            sum += pos.x + pos.y + pos.z;
        }
        benchmark::DoNotOptimize(sum);
    }
}

// EnTT 系统更新测试
static void BM_EnTT_SystemUpdate(benchmark::State& state)
{
    entt::registry registry;

    // 预先创建实体，确保所有实体都有相同的组件
    for (int i = 0; i < state.range(0); ++i)
    {
        auto entity = registry.create();
        registry.emplace<EnttPosition>(entity, i * 1.0f, i * 2.0f, i * 3.0f);
        registry.emplace<EnttVelocity>(entity, i * 0.1f, i * 0.2f, i * 0.3f);
        registry.emplace<EnttHealth>(entity, 100, 100);
    }

    const float deltaTime = 0.016f; // 60 FPS

    for (auto _ : state)
    {
        // 使用相同的更新逻辑
        auto view = registry.view<EnttPosition, EnttVelocity>();
        for (auto entity : view)
        {
            auto& pos = view.get<EnttPosition>(entity);
            auto& vel = view.get<EnttVelocity>(entity);

            pos.x += vel.dx * deltaTime;
            pos.y += vel.dy * deltaTime;
            pos.z += vel.dz * deltaTime;
        }
    }
}

// ================================
// 复杂场景测试
// ================================

// RTTM 复杂场景测试
static void BM_RTTM_ComplexScene(benchmark::State& state)
{
    std::vector<GameEntity> entities;
    entities.reserve(state.range(0));

    // 创建实体，确保所有实体都有完整的组件
    for (int i = 0; i < state.range(0); ++i)
    {
        GameEntity entity;
        entity.AddComponent<Position>(i * 1.0f, i * 2.0f, i * 3.0f);
        entity.AddComponent<Velocity>(i * 0.1f, i * 0.2f, i * 0.3f);
        entity.AddComponent<HealthComp>(100, 100);
        entities.emplace_back(std::move(entity));
    }

    const float deltaTime = 0.016f;

    for (auto _ : state)
    {
        // 更新位置
        for (auto& entity : entities)
        {
            entity.Update(deltaTime);
        }

        // 处理伤害（每10个实体）
        for (size_t i = 0; i < entities.size(); i += 10)
        {
            entities[i].TakeDamage(5);
        }
    }
}

// EnTT 复杂场景测试
static void BM_EnTT_ComplexScene(benchmark::State& state)
{
    entt::registry registry;

    // 创建实体，确保所有实体都有完整的组件
    for (int i = 0; i < state.range(0); ++i)
    {
        auto entity = registry.create();
        registry.emplace<EnttPosition>(entity, i * 1.0f, i * 2.0f, i * 3.0f);
        registry.emplace<EnttVelocity>(entity, i * 0.1f, i * 0.2f, i * 0.3f);
        registry.emplace<EnttHealth>(entity, 100, 100);
    }

    const float deltaTime = 0.016f;

    for (auto _ : state)
    {
        // 更新位置
        auto moveView = registry.view<EnttPosition, EnttVelocity>();
        for (auto entity : moveView)
        {
            auto& pos = moveView.get<EnttPosition>(entity);
            auto& vel = moveView.get<EnttVelocity>(entity);

            pos.x += vel.dx * deltaTime;
            pos.y += vel.dy * deltaTime;
            pos.z += vel.dz * deltaTime;
        }

        // 处理伤害（每10个实体）
        auto healthView = registry.view<EnttHealth>();
        int count = 0;
        for (auto entity : healthView)
        {
            if (count % 10 == 0)
            {
                auto& health = healthView.get<EnttHealth>(entity);
                health.hp = std::max(0, health.hp - 5);
            }
            count++;
        }
    }
}

// ================================
// 新增：公平的迭代性能测试
// ================================

// RTTM 迭代性能测试 - 测试遍历所有实体的性能
static void BM_RTTM_Iteration(benchmark::State& state)
{
    std::vector<GameEntity> entities;
    entities.reserve(state.range(0));

    // 创建实体
    for (int i = 0; i < state.range(0); ++i)
    {
        GameEntity entity;
        entity.AddComponent<Position>(i * 1.0f, i * 2.0f, i * 3.0f);
        entity.AddComponent<Velocity>(i * 0.1f, i * 0.2f, i * 0.3f);
        entity.AddComponent<HealthComp>(100, 100);
        entities.emplace_back(std::move(entity));
    }

    for (auto _ : state)
    {
        int processedCount = 0;
        for (const auto& entity : entities)
        {
            // 模拟处理逻辑
            processedCount++;
        }
        benchmark::DoNotOptimize(processedCount);
    }
}

// EnTT 迭代性能测试 - 测试view遍历的性能
static void BM_EnTT_Iteration(benchmark::State& state)
{
    entt::registry registry;

    // 创建实体
    for (int i = 0; i < state.range(0); ++i)
    {
        auto entity = registry.create();
        registry.emplace<EnttPosition>(entity, i * 1.0f, i * 2.0f, i * 3.0f);
        registry.emplace<EnttVelocity>(entity, i * 0.1f, i * 0.2f, i * 0.3f);
        registry.emplace<EnttHealth>(entity, 100, 100);
    }

    for (auto _ : state)
    {
        int processedCount = 0;
        // 使用view遍历所有拥有三个组件的实体
        auto view = registry.view<EnttPosition, EnttVelocity, EnttHealth>();
        for (auto entity : view)
        {
            // 模拟处理逻辑
            processedCount++;
        }
        benchmark::DoNotOptimize(processedCount);
    }
}

// ================================
// 基准测试注册
// ================================

// 注册基准测试，测试不同数量的实体
BENCHMARK(BM_RTTM_EntityCreation)->Range(100, 10000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_EnTT_EntityCreation)->Range(100, 10000)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_RTTM_ComponentAccess)->Range(100, 10000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_EnTT_ComponentAccess)->Range(100, 10000)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_RTTM_SystemUpdate)->Range(100, 10000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_EnTT_SystemUpdate)->Range(100, 10000)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_RTTM_ComplexScene)->Range(100, 10000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_EnTT_ComplexScene)->Range(100, 10000)->Unit(benchmark::kMicrosecond);

BENCHMARK(BM_RTTM_Iteration)->Range(100, 10000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_EnTT_Iteration)->Range(100, 10000)->Unit(benchmark::kMicrosecond);

// 主函数 - 处理参数过滤
int main(int argc, char** argv) {
    // 过滤 gtest 参数，避免冲突
    std::vector<char*> filtered_args;
    filtered_args.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg.find("--gtest") == std::string::npos) {
            filtered_args.push_back(argv[i]);
        }
    }

    int filtered_argc = static_cast<int>(filtered_args.size());
    char** filtered_argv = filtered_args.data();

    benchmark::Initialize(&filtered_argc, filtered_argv);
    if (benchmark::ReportUnrecognizedArguments(filtered_argc, filtered_argv)) {
        return 1;
    }

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
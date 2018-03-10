#include <chrono>
#include <thread>


#include "../NESL/NESL.h"

#pragma region States

using GEntities = ESL::Entities;

struct GFrame { uint64_t total; };
GLOBAL_STATE(GFrame);

struct GCanvas { };
GLOBAL_STATE(GCanvas);

struct EAppearance { char v; }; //显示的字符
ENTITY_STATE(EAppearance, Vec);

struct ELocation { int32_t x, y; }; //位置
ENTITY_STATE(ELocation, Vec);

struct EVelocity { int32_t x, y; }; //移动速度
ENTITY_STATE(EVelocity, Vec);

struct EGravity { int32_t g; }; //移动速度
ENTITY_STATE(EGravity, Vec);

struct ELifeTime { int32_t n; }; //计时死亡
ENTITY_STATE(ELifeTime, Vec);

struct ESpawner { int32_t life; }; //生成计时死亡的实体
ENTITY_STATE(ESpawner, Hash);

#pragma endregion


#pragma region Logics

auto Logic_Draw = [](const ELocation& loc, const EAppearance& ap, const GCanvas& canvas)
{
	std::cout << loc.x << " " << loc.y << " " << ap.v << "\n";
};

auto Logic_Move = [](ELocation& loc, EVelocity& vel, const GCanvas& canvas)
{
	loc.x += vel.x;
	loc.y += vel.y;
	if (loc.x >= 20)
	{
		vel.x = -std::abs(vel.x);
		loc.x = 40 - loc.x;
	}
	if (loc.y >= 20) 
	{
		vel.y = -std::abs(vel.y);
		loc.y = 40 - loc.y;
	}
	if (loc.x <= 0) 
	{
		vel.x = std::abs(vel.x);
		loc.x = 0 - loc.x;
	}
	if (loc.y <= 0) 
	{
		vel.y = std::abs(vel.y);
		loc.y = 0 - loc.y;
	}
};

auto Logic_Gravity = [](EVelocity& vel, const EGravity& gravity)
{
	vel.y -= gravity.g;
};

auto Logic_LifeTime(ELifeTime& life, ESL::Entity self, const GEntities& entities)
{
	if (--life.n < 0)
		entities.Kill(self);
}

auto Logic_Spawn(const ESpawner& sp, const ELocation& loc, GEntities& entities,
	ESL::State<ELifeTime>& lifetimes, ESL::State<ELocation>& locations,
	ESL::State<EAppearance>& appearances)
{
	auto res = entities.TrySpawn();
	if (res.has_value())
	{
		auto e = res.value();
		lifetimes.Create(e, sp.life);
		locations.Create(e, loc.x, loc.y);
		appearances.Create(e, '*');
	}
}

#pragma endregion

auto Init(GEntities& entities, ESL::State<ESpawner>& spawners, ESL::State<ELocation>& locations,
	ESL::State<EVelocity>& velocities, ESL::State<EGravity>& gravities, 
	ESL::State<EAppearance>& appearances)
{
	auto res = entities.TrySpawn();
	if (res.has_value())
	{
		auto e = res.value();
		locations.Create(e, 5, 5);
		appearances.Create(e, '@');
		gravities.Create(e, 1);
		velocities.Create(e, 5, 0);
		spawners.Create(e, 1);
	}
}


int main()
{
#pragma region ESL

	ESL::States st;
	st.Create<EVelocity>();
	st.Create<ELocation>();
	st.Create<ELifeTime>();
	st.Create<EGravity>();
	st.Create<EAppearance>();
	st.Create<ESpawner>();
	GFrame &frame = st.Create<GFrame>().Raw();
	GCanvas &canvas = st.Create<GCanvas>().Raw();
	ESL::Dispatch(st, Init);

	ESL::LogicGraphBuilder graphBuilder(st);
	graphBuilder.Create(Logic_Draw, "Draw");
	graphBuilder.Create(Logic_Move, "Move").Depends("Draw", "Spawn");
	graphBuilder.Create(Logic_LifeTime, "LifeTime").Depends("Spawn");
	graphBuilder.Create(Logic_Gravity, "Gravity").Depends("Move");
	graphBuilder.Create(Logic_Spawn, "Spawn").Depends("Draw");
	graphBuilder.Compile();
	graphBuilder.ExportGraphviz("test.gv");
	ESL::LogicGraph logicGraph;
	graphBuilder.Build(logicGraph);
#pragma endregion

#pragma region GameLoop



	while (true) {
		using namespace std::chrono;
		logicGraph.Flow();
		st.Tick();
		std::this_thread::sleep_for(1000ms);
	}

#pragma endregion

	return 0;
}
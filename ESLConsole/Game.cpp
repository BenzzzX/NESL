#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <thread>
#include "Declare.h"
#include <io.h>

#pragma region Logics

auto Logic_Draw(const ELocation& loc, const EAppearance& ap, const GCanvas& canvas)
{
	std::cout << loc.x << " " << loc.y << " " << ap.v << "\n";
}

auto Logic_Move(ELocation& loc, EVelocity& vel, const GCanvas& canvas)
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
}

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
	ESL::State<EVelocity>& velocities, ESL::State<EAppearance>& appearances)
{
	auto res = entities.TrySpawn();
	if (res.has_value())
	{
		auto e = res.value();
		locations.Create(e, 5, 5);
		appearances.Create(e, '@');
		velocities.Create(e, 5, 0);
		spawners.Create(e, 1);
	}
}

void Register(ESL::States& st, ESL::LogicGraphBuilder& graphBuilder)
{
	st.Create<EVelocity>();
	st.Create<ELocation>();
	st.Create<ELifeTime>();
	st.Create<EAppearance>();
	st.Create<ESpawner>();
	graphBuilder.Create(Logic_Draw, "Draw");
	graphBuilder.Create(Logic_Move, "Move").Depends("Draw", "Spawn");
	graphBuilder.Create(Logic_LifeTime, "LifeTime").Depends("Spawn");
	graphBuilder.Create(Logic_Spawn, "Spawn").Depends("Draw");
}

using fRegister = void(*)(ESL::States&, ESL::LogicGraphBuilder&);

void RegisterPlugins(ESL::States& st, ESL::LogicGraphBuilder& graphBuilder)
{
	char findDir[] = "plugins/*.dll";
	char pluginDir[100] = "plugins/";
	intptr_t handle;
	_finddata_t findData;

	handle = _findfirst(findDir, &findData);
	if (handle == -1) return;
	do 
	{
		strcpy(pluginDir + 8, findData.name);
		std::cout << pluginDir << "\n";
		HMODULE hModule = LoadLibrary(pluginDir);
		fRegister reg = reinterpret_cast<fRegister>(GetProcAddress(hModule, "Register"));
		reg(st, graphBuilder);
		//FreeLibrary(hModule);
	} while (_findnext(handle, &findData) == 0);
	_findclose(handle);
}


int main()
{
#pragma region ESL

	ESL::States st;
	ESL::LogicGraphBuilder graphBuilder(st);
	GFrame &frame = st.Create<GFrame>().Raw();
	GCanvas &canvas = st.Create<GCanvas>().Raw();

	Register(st, graphBuilder);
	ESL::Dispatch(st, Init);
	RegisterPlugins(st, graphBuilder);

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
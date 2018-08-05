#define _CRT_SECURE_NO_WARNINGS
#include <chrono>
#include <thread>
#include "Declare.h"
#include <io.h>

template<int32_t min, int32_t max>
int32_t map(int32_t val)
{
	return val >= 0 ? min + val % (max - min + 1) : max - (-val) % (max - min + 1);
}

#pragma region Logics
auto Logic_Draw(const ELocation& loc, const EAppearance& ap, GCanvas& canvas)
{
	SetConsoleCursorPosition(canvas.handle, { (SHORT)map<0,70>(loc.x), (SHORT)(29 - map<3,29>(loc.y) + 3) });
	std::cout << ap.v;
}

auto Logic_Move(ELocation& loc, EVelocity& vel)
{
	loc.x += vel.x;
	loc.y += vel.y;
}

auto Logic_Clear(const ELifeTime& life, EAppearance& ap)
{
	if (life.n == 0) //清理残影图像
		ap.v = ' ';
}

auto Logic_LifeTime(ELifeTime& life, ESL::Entity self, const GEntities& entities)
{
	if (--life.n < 0) //清理残影
		entities.Kill(self);
}

auto Logic_Spawn(
	const ELength& sp, 
	const ELocation& loc, 
	GEntities& entities,
	ESL::State<ELifeTime>& lifetimes, 
	ESL::State<ELocation>& locations,
	ESL::State<EAppearance>& appearances)
{
	auto res = entities.TrySpawn();
	if (res.has_value()) //创建残影
	{
		auto e = res.value();
		lifetimes.Create(e.id, { sp.life });
		locations.Create(e.id, { loc.x, loc.y });
		appearances.Create(e.id, { '*' });
	}
}
#pragma endregion

void Register(ESL::States& st, ESL::LogicGraphBuilder& graph)
{
	auto e = st.SpawnEntity();
	st.Entities().Grow(200);
	st.CreateState<EVelocity>().Create(e.id, { 1,0 });
	st.CreateState<ELocation>().Create(e.id, { 5,20 });
	st.CreateState<ELifeTime>();
	st.CreateState<EAppearance>().Create(e.id, { '@' });
	st.CreateState<ELength>().Create(e.id, { 80 });
	st.CreateState<GCanvas>(GetStdHandle(STD_OUTPUT_HANDLE));

	graph.Schedule(Logic_Draw, "Draw");
	graph.Schedule(Logic_Move, "Move").After("Draw", "Spawn");
	graph.Schedule(Logic_LifeTime, "LifeTime").After("Spawn");
	graph.Schedule(Logic_Clear, "Clear").After("LifeTime");
	graph.Schedule(Logic_Spawn, "Spawn").After("Draw");
}

void HideCursor()
{
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO CursorInfo;
	GetConsoleCursorInfo(handle, &CursorInfo);//获取控制台光标信息
	CursorInfo.bVisible = false; //隐藏控制台光标
	SetConsoleCursorInfo(handle, &CursorInfo);//设置控制台光标状态
}

void RegisterPlugins(ESL::States& st, ESL::LogicGraphBuilder& graphBuilder)
{
	char findDir[] = "plugins/*.dll";
	char pluginDir[100] = "plugins/";
	intptr_t handle;
	_finddata_t findData;
	std::cout << "Loading Plugins:\n";
	handle = _findfirst(findDir, &findData);
	if (handle == -1) return;
	do 
	{
		strcpy(pluginDir + 8, findData.name);
		std::cout << "    " << pluginDir << "\n";
		HMODULE hModule = LoadLibrary(pluginDir);
		auto reg = reinterpret_cast<void(*)(ESL::States&, ESL::LogicGraphBuilder&)>(GetProcAddress(hModule, "Register"));
		reg(st, graphBuilder);
		//FreeLibrary(hModule);
	} while (_findnext(handle, &findData) == 0);
	_findclose(handle);
}


int main()
{
	HideCursor();
#pragma region ESL
	ESL::States st;
	ESL::LogicGraphBuilder graph(st);

	Register(st, graph);
	RegisterPlugins(st, graph);

	graph.Compile();
	graph.ExportGraphviz("test.gv");
	ESL::LogicGraph logicGraph;
	graph.Build(logicGraph);

#pragma endregion

#pragma region GameLoop

	while (true) {
		using namespace std::chrono;
		logicGraph.Flow();
		st.Tick();
		std::this_thread::sleep_for(100ms);
	}

#pragma endregion

	return 0;
}
#include "NESL.h"
#include "States.h"
#include "LogicGraph.h"
#include <iostream>

#define ENTITY_STATE(name, container) \
namespace ESL \
{ \
	template<> \
	struct TState<name> { using type = EntityState<container<name>>; }; \
}

struct EAppearance { char v; }; //显示的字符
ENTITY_STATE(EAppearance, Vec);

struct ELocation { int x, y; }; //位置
ENTITY_STATE(ELocation, Vec);

struct EVelocity { int x, y; }; //移动速度
ENTITY_STATE(EVelocity, Vec);

struct ELifeTime { int n; }; //计时死亡
ENTITY_STATE(ELifeTime, Vec);

struct ESpawner { int life; }; //生成计时死亡的实体
ENTITY_STATE(ESpawner, Hash);


auto Logic_Draw = [](const ELocation& loc, const EAppearance& ap) {};

auto Logic_Move = [](ELocation& loc, const EVelocity& vel) {};

auto Logic_Input = [](EVelocity& vel) {};

auto Logic_LifeTime(ELifeTime& life, ESL::Entity self, ESL::GEntities& entities) {}

auto Logic_Spawn(const ESpawner& sp, const ELocation& loc, ESL::GEntities& entities,
	ESL::State<ELifeTime>& lifetimes, ESL::State<ELocation>& locations,
	ESL::State<EAppearance>& appearances) {}

int main()
{
	ESL::States st;

	st.Create<EVelocity>();
	st.Create<EVelocity>();
	st.Create<ELocation>();
	st.Create<EAppearance>();
	st.Create<ESpawner>();
	
	ESL::LogicGraphBuilder graphBuilder(st);
	graphBuilder.Create(Logic_Draw, "Draw").Depends("Move", "Spawn");
	graphBuilder.Create(Logic_Move, "Move").Depends("Input");
	graphBuilder.Create(Logic_LifeTime, "LifeTime").Depends("Spawn");
	graphBuilder.Create(Logic_Input, "Input");
	graphBuilder.Create(Logic_Spawn, "Spawn");
	// .dependencies.push_back("Show suffix");
	graphBuilder.Compile();
	graphBuilder.ExportGraphviz("test.gv");
	//ESL::LogicGraph logicGraph;
	//graphBuilder.Build(logicGraph);
	
	//logicGraph.Flow();
    return 0;
}
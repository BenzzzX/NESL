#include <chrono>
#include <iostream>
#include "LogicGraph.h"


class TimerBlock {
	std::chrono::high_resolution_clock::time_point start;
	const char *str;
public:
	TimerBlock(const char *str)
		: start(std::chrono::high_resolution_clock::now()), str(str) {
		std::cout << str << " start... \n";
	}
	~TimerBlock() {
		auto end = std::chrono::high_resolution_clock::now();
		std::cout << str << " finished, time used: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end -
				start)
			.count()
			<< "ms\n\n";
	}
};


class Timer {
	std::chrono::high_resolution_clock::time_point start;
	const char *str;
public:
	void begin(const char *str)
	{
		this->start = std::chrono::high_resolution_clock::now();
		this->str = str;
		std::cout << str << " start... \n";
	}
	void finish() 
	{
		auto end = std::chrono::high_resolution_clock::now();
		std::cout << str << " finished, time used: "
			<< std::chrono::duration_cast<std::chrono::milliseconds>(end -
				start)
			.count()
			<< "ms\n\n";
	}
};

struct location { float x, y; };
ENTITY_STATE(location, Vec);
struct velocity { float x, y; };
ENTITY_STATE(velocity, SparseVec);

constexpr std::size_t Count = 10'000'000u;

void BenchMark_NESL()
{
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	{
		TimerBlock timer("spawn 10m entity");
		states.CreateState<location>();
		states.CreateState<velocity>();
		states.BatchSpawnEntity(Count, location{ 0,0 }, velocity{ 1,1 });
	}
	graph.ScheduleParallel([](const velocity& vel, location& loc)
	{
		loc.x += vel.x;
		loc.y += vel.y;
	}, "Mover");
	graph.Schedule([](ESL::Entity self, const ESL::Entities& entities)
	{
		entities.Kill(self);
	}, "Killer");
	graph.Compile();
	ESL::LogicGraph logicGraph;
	graph.Build(logicGraph);
	{
		TimerBlock timer("move and kill 10m entity");
		logicGraph.Flow();
		states.Tick();
	}
}


void BenchMark_LogicGraph()
{
	Timer timer;

	timer.begin("create 10m entity");
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	std::pair<ESL::index_t, ESL::index_t> range;
	auto& locations = states.CreateState<location>();
	auto& velocities = states.CreateState<velocity>();
	range = states.BatchSpawnEntity(Count, location{ 0,0 }, velocity{ 1,1 });
	timer.finish();

	auto move = [](const velocity& vel, location& loc)
	{
		loc.x += vel.x;
		loc.y += vel.y;
	};
	graph.ScheduleParallel(move, "Move");
	ESL::LogicGraph logicGraph;
	graph.Compile();
	graph.Build(logicGraph);

	timer.begin("move 10m entity");
	logicGraph.Flow();
	timer.finish();
}

int main()
{
	
	std::cout << "NESL:\n";
	BenchMark_NESL();
	/*
	std::cout << "\nLogicGraph:\n";
	BenchMark_LogicGraph();

	std::cout << "\nLogicGraph Again:\n";
	BenchMark_LogicGraph();
	*/
	
	return 0;
}
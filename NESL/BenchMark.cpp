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

struct location { float x, y; };
ENTITY_STATE(location, Vec);
struct velocity { float x, y; };
ENTITY_STATE(velocity, Vec);

void BenchMark_NESL()
{
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	{
		TimerBlock timer("create 10m entity");
		auto& locations = states.CreateState<location>();
		auto& velocities = states.CreateState<velocity>();
		for (size_t i = 0u; i < 10'000'000; i++)
		{
			auto e = states.Spawn();
			locations.Create(e, location{ 0, 0 });
			velocities.Create(e, velocity{ 1, 1 });
		}
	}
	auto move = [](const velocity& vel, location& loc)
	{
		auto _vel = vel;
		auto _loc = loc;
		for (int i = 0; i < 100; i++)
		{
			_loc.x *= _vel.x;
			_loc.y *= _vel.y;
		}
		loc = _loc;
	};
	graph.Schedule(move, "Move");
	ESL::LogicGraph logicGraph;
	graph.Compile();
	graph.Build(logicGraph);
	{
		TimerBlock timer("move 10m entity");
		logicGraph.Flow();
	}
}


void BenchMark_LogicGraph()
{
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	{
		TimerBlock timer("create 10m entity");
		auto& locations = states.CreateState<location>();
		auto& velocities = states.CreateState<velocity>();
		for (size_t i = 0u; i < 10'000'000; i++)
		{
			auto e = states.Spawn();
			locations.Create(e, location{ 0, 0 });
			velocities.Create(e, velocity{ 1, 1 });
		}
	}
	auto move = [](const velocity& vel, location& loc)
	{
		auto _vel = vel;
		auto _loc = loc;
		for (int i = 0; i < 100; i++)
		{
			_loc.x *= _vel.x;
			_loc.y *= _vel.y;
		}
		loc = _loc;
	};
	graph.ScheduleParallel(move, "Move");
	ESL::LogicGraph logicGraph;
	graph.Compile();
	graph.Build(logicGraph);
	{
		TimerBlock timer("move 10m entity");
		logicGraph.Flow();
	}
}

int main()
{
	std::cout << "NESL:\n";
	BenchMark_NESL();

	std::cout << "LogicGraph:\n";
	BenchMark_LogicGraph();
	return 0;
}
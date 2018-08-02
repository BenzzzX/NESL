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
ENTITY_STATE(velocity, Vec);

constexpr std::size_t Count = 10'000'000u;

void BenchMark_NESL()
{
	ESL::States states(Count, true);
	ESL::LogicGraphBuilder graph(states);
	{
		TimerBlock timer("create 10m entity");
		auto& locations = states.CreateState<location>(Count);
		auto& velocities = states.CreateState<velocity>(Count);
		for (size_t i = 0u; i < Count; i++)
		{
			auto e = ESL::Entity{ HBV::index_t(i), 0 };
			locations.Create(e, { 0, 0 });
			velocities.Create(e, { 1, 1 });
		}
	}
	auto move = [](const velocity& vel, location& loc)
	{
		loc.x += vel.x;
		loc.y += vel.y;
	};
	graph.Schedule(move, "Move");
	ESL::LogicGraph logicGraph;
	graph.Compile();
	graph.Build(logicGraph);
	{
		TimerBlock timer("move 10m entity");
		for(int i=0;i<100;i++)
			logicGraph.Flow();
	}
}


void BenchMark_LogicGraph()
{
	Timer timer;

	timer.begin("create 10m entity");
	ESL::States states(Count, true);
	ESL::LogicGraphBuilder graph(states);
	auto& locations = states.CreateState<location>(Count);
	auto& velocities = states.CreateState<velocity>(Count);
	for (size_t i = 0u; i < Count; i++)
	{
		auto e = ESL::Entity{HBV::index_t(i), 0};
		locations.Create(e, { 0, 0 });
		velocities.Create(e, { 1, 1 });
	}
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
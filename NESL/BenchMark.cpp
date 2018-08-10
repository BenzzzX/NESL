#include <chrono>
#include <iostream>
#include "LogicGraph.h"
#include "Flatten.h"


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


struct velocity { float x, y; };
//定义velocity为组件,以SparseVec为容器,并且附加一个[创建]事件的追踪器
ENTITY_STATE(velocity, SharedVec, ESL::Create);
struct location { float x, y; };
//定义location为组件,以Vec为容器
ENTITY_STATE(location, Vec);
struct mesh {/*some data*/ };
ENTITY_STATE(mesh, UniqueVec);

constexpr std::size_t Count = 10'000'000u;
void DrawInstanced(const mesh&, const lni::vector<location>&) {/*some code*/}

void UniqueVecShowCase()
{
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	//注册组件
	states.CreateState<location>();
	states.CreateState<mesh>();
	//创建1kw个对象,分别使用两个模型
	states.BatchSpawnEntity(Count / 2, location{ 0,0 }, *(new mesh{})); //Mesh1
	states.BatchSpawnEntity(Count / 2, location{ 0,0 }, *(new mesh{})); //Mesh2
	//使用graph来安全的多线程化
	graph.Schedule([]
	(const ESL::State<location>& locs, ESL::State<mesh>& meshs)
	{
		//取得mesh的数量
		auto size = meshs.UniqueSize();
		//用于数据的打包
		lni::vector<location> buffer;
		buffer.reserve(Count / 2);
		for (auto i = 0; i < size; ++i)
		{
			//获得一个mesh,并设置为filter,为接下来的匹配做准备
			const mesh& toDraw = meshs.SetUnique(i);
			//打包所有使用这个mesh的对象的位置
			ESL::Dispatch(std::tie(locs, meshs),[&buffer]
			(const location& location, FHas(mesh))
			{
				buffer.push_back(location);
			});
			//进行绘制
			DrawInstanced(toDraw, buffer);
			buffer.clear();
		}
	}, "DrawInstancedMesh");
}

void BenchMark_NESL()
{
	ESL::States states;
	{
		TimerBlock timer("create 10m entity");
		//注册组件
		states.CreateState<location>();
		states.CreateState<velocity>();
		//批量创建Entity
		states.BatchSpawnEntity(Count, location{ 0,0 }, velocity{ 1,1 });
	}
	{
		TimerBlock timer("update 10m entity");
		ESL::Dispatch(states, 
		//   [有velocity组件]   [有location组件]  [刚创建velocity] 的Entity将被匹配
		[](const velocity& vel, location& loc, FCreated(velocity))
		{
			loc.x += vel.x;
			loc.y += vel.y;
		});
		//重置所有追踪器, 如[刚创建velocity]将被重置
		states.ResetTracers();
	}
}


void BenchMark_LogicGraph()
{
	Timer timer;

	timer.begin("create 10m entity");
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	auto& locations = states.CreateState<location>();
	auto& velocities = states.CreateState<velocity>();
	states.BatchSpawnEntity(Count, location{ 0,0 }, velocity{ 1,1 });
	timer.finish();

	graph.ScheduleParallel([](const velocity& vel, location& loc)
	{
		loc.x += vel.x;
		loc.y += vel.y;
	}, "Move");
	ESL::LogicGraph logicGraph;
	graph.Compile();
	graph.Build(logicGraph);

	timer.begin("update 10m entity");
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
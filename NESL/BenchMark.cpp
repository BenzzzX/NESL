#include <chrono>
#include <iostream>
#include "TbbGraph.h"
#include "Flatten.h"
#include <thread>


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
//����velocityΪ���,��SparseVecΪ����,���Ҹ���һ��[����]�¼���׷����
ENTITY_STATE(velocity, SharedVec, ESL::Create);
struct location { float x, y; };
//����locationΪ���,��VecΪ����
ENTITY_STATE(location, Vec);
//����meshΪ���,��UniqueVecΪ����
struct mesh {/*some data*/ };
ENTITY_STATE(mesh, UniqueVec);

constexpr std::size_t Count = 400'000u;
void DrawInstanced(const mesh&, const lni::vector<location>&) {/*some code*/}

void GameShowCase()
{
	ESL::States states;
	ESL::TbbGraph renderLogic;
	ESL::TbbGraph gameLogic;
	{
		//ע�����
		states.CreateState<location>();
		states.CreateState<mesh>();
		//����1kw������,�ֱ�ʹ������ģ��
		states.BatchSpawnEntity(Count / 2, location{ 0,0 }, *(new mesh{})); //Mesh1
		states.BatchSpawnEntity(Count / 2, location{ 0,0 }, *(new mesh{})); //Mesh2
	}
	{
		ESL::LogicGraph graph(states);
		//ʹ������ͼ����ȫ�Ķ��̻߳�
		graph.Schedule([]
		(const ESL::State<location>& locs, ESL::State<mesh>& meshs)
		{
			//ȡ��ģ�͵�����
			auto size = meshs.UniqueSize();
			//�������ݴ���Ļ���
			lni::vector<location> buffer;
			buffer.reserve(Count / 2);
			for (auto i = 0; i < size; ++i)
			{
				//UniqueVec��������ӿ�
				//���һ��ģ��,������Ϊ������,Ϊ��������ƥ����׼��
				const mesh& toDraw = meshs.GetUniqueAsFilter(i);
				//ƥ�䲢�������ʹ�����ģ�͵Ķ����λ��
				ESL::Dispatch(std::tie(locs, meshs), [&buffer]
				(const location& location, FHas(mesh)/**/)
				{
					buffer.push_back(location);
				});
				//���л���
				DrawInstanced(toDraw, buffer);
				buffer.clear();
			}
			//ȡ��������
			meshs.ReleaseFilter();
		}, "DrawInstancedMesh");
		graph.Build(renderLogic);
	}
	{
		ESL::LogicGraph graph(states);
		graph.Schedule<ESL::ParallelDispatcher>([](const velocity& vel, location& loc)
		{
			loc.x += vel.x;
			loc.y += vel.y;
		}, "Move");
		graph.Build(gameLogic);
	}
	using namespace std::chrono;
	auto time = std::chrono::high_resolution_clock::now();
	auto deltaT = 0ms;
	while (1)
	{
		auto now = std::chrono::high_resolution_clock::now();
		deltaT += std::chrono::duration_cast<std::chrono::milliseconds>(now - time);
		time = now;
		
		while (deltaT.count() > 0)
		{
			gameLogic.RunOnce();
			deltaT -= 5ms;
		}
		renderLogic.RunOnce();
		
		std::this_thread::sleep_for(1ms);
	}
}

void BenchMark_NESL()
{
	ESL::States states;
	{
		TimerBlock timer("create 10m entity");
		//ע�����
		states.CreateState<location>();
		states.CreateState<velocity>();
		//��������Entity
		states.BatchSpawnEntity(Count, location{ 0,0 }, velocity{ 1,1 });
	}
	{
		TimerBlock timer("update 10m entity");
		ESL::Dispatch(states, 
		//   [��velocity���]   [��location���]  [�մ���velocity] ��Entity����ƥ��
		[](const velocity& vel, location& loc, FCreated(velocity))
		{
			loc.x += vel.x;
			loc.y += vel.y;
		});
		//��������׷����, ��[�մ���velocity]��������
		states.ResetTracers();
	}
}


void BenchMark_LogicGraph()
{
	Timer timer;

	timer.begin("create 10m entity");
	ESL::States states;
	ESL::LogicGraph graph(states);
	auto& locations = states.CreateState<location>();
	auto& velocities = states.CreateState<velocity>();
	states.BatchSpawnEntity(Count, location{ 0,0 }, velocity{ 1,1 });
	timer.finish();

	graph.Schedule<ESL::ParallelDispatcher>([](const velocity& vel, location& loc)
	{
		loc.x += vel.x;
		loc.y += vel.y;
	}, "Move");
	ESL::TbbGraph flowGraph;
	graph.Build(flowGraph);

	timer.begin("update 10m entity");
	flowGraph.RunOnce();
	timer.finish();
}

int main()
{
	
	std::cout << "NESL:\n";
	BenchMark_LogicGraph();
	/*
	std::cout << "\nLogicGraph:\n";
	BenchMark_LogicGraph();

	std::cout << "\nLogicGraph Again:\n";
	BenchMark_LogicGraph();
	*/
	
	return 0;
}
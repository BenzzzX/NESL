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

void UniqueVecShowCase()
{
	ESL::States states;
	ESL::LogicGraphBuilder graph(states);
	Timer timer;
	//ע�����
	states.CreateState<location>();
	states.CreateState<mesh>();
	timer.begin("create 400k entity");
	//����1kw������,�ֱ�ʹ������ģ��
	states.BatchSpawnEntity(Count / 2, location{ 0,0 }, *(new mesh{})); //Mesh1
	states.BatchSpawnEntity(Count / 2, location{ 0,0 }, *(new mesh{})); //Mesh2
	timer.finish();
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
			ESL::Dispatch(std::tie(locs, meshs),[&buffer]
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
	ESL::LogicGraph logicGraph;
	graph.Compile();
	graph.Build(logicGraph);
	timer.begin("update 400k entity");
	for(int i=0;i<100;i++)
	logicGraph.Flow();
	timer.finish();
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
	UniqueVecShowCase();
	/*
	std::cout << "\nLogicGraph:\n";
	BenchMark_LogicGraph();

	std::cout << "\nLogicGraph Again:\n";
	BenchMark_LogicGraph();
	*/
	
	return 0;
}
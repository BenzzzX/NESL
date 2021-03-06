#include <Windows.h>
#include <iostream>

#include "../ESLConsole/Declare.h" //导入原游戏声明

struct EChangeDir { int32_t n, i; };
ENTITY_STATE(EChangeDir, Vec);


auto Logic_ChangeDirection(EVelocity& vel, EChangeDir& cd)
{
	if (--cd.i < 0)
	{
		cd.i = cd.n;
		
		if (vel.x == 0)
		{
			vel.x = std::abs(vel.y);
			vel.y = 0;
		}
		else
		{
			int rand = std::rand() % 2;
			vel.y = (rand * 2 - 1) * vel.x;
			vel.x = 0;
		}
	}
}

auto Init(ESL::Entity e, ELength&/*as tag*/,
	ESL::State<EChangeDir>& gravities) //给带有Spawn状态的实体的加上状态
{
	gravities.Create(e, { 5, 5 });
}

extern "C" _declspec(dllexport) 
void Register(ESL::States& st, ESL::LogicGraph& graph) //注册插件逻辑
{
	st.CreateState<EChangeDir>();
	graph.Schedule(Logic_ChangeDirection, "ChangeDirection", "Move");
	ESL::Dispatch(st, Init);
}

BOOL APIENTRY DllMain(HMODULE, DWORD ul_reason_for_call, LPVOID)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}


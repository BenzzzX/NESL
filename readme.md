# ESL

[![Join the chat at https://gitter.im/ImmediateCode/Lobby](https://badges.gitter.im/ImmediateCode/Lobby.svg)](https://gitter.im/ImmediateCode/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)  
ESL 框架全称 Entity State Logic ,是一个基于 ECS 模型的 C++17 通用框架.
***
## 目录
* [前置需求](#前置需求)
* [如何编译](#如何编译)
* [核心特征](#核心特征)
* [基本目标](#基本目标)
* [ESL的设计与组成](#esl的设计与组成)
* [简单的示例](#简单示例)
***
### 前置需求
* M$VC
* Clang
* 任何其他支持C++17标准的编译器(未测试)
* 不依赖于任何第三方库
***
### 如何编译
* V$用户直接打开项目文件(.sln)编译即可
***
### 核心特征
* ECS
* 数据驱动
    * 缓存友好
    * 测试友好
    * 表格友好
* 函数式编程
    * 自动并行
    * 副作用集中
    * 低耦合
* 模板元
    * 高抽象隐藏细节
    * 编译期代价
    * 开源(逃
***
### 基本目标

虽然硬件性能已经有了巨大提升,但是在游戏开发中,依然需要压榨机器的性能,根据现在的硬件特性,内存成为短板,并行成为长处,要压榨性能肯定需要从这方面下手,也就是Cache&Parallel.
* 这意味着需要对内存排布和读写拥有强大的控制能力.
* 同时,我们还希望能不损失过多的解耦能力.

### 简单的示例
这个实例是一个极简的游戏,玩家控制一条在控制台移动的蛇;  
为了写起来方便,定义一个宏,用于申明一个实体状态
```C++
ENTITY_STATE(name, container) 
namespace ESL 
{ 
    template<> 
    struct TState<name> { using type = EntityState<container<name>>; };
} 
```
首先我们实现显示的逻辑,即在屏幕上 entity 对应的位置打印出其对应的字符
```C++
struct EAppearance { char v; }; //显示的字符
ENTITY_STATE(EApperance, Vec);
struct ELocation { int x, y; };
ENTITY_STATE(ELocation, Vec);
auto draw_entity = [](const ELocation& loc, const EAppearance& ap) { renderer.draw(loc.x, loc.y, ap.v); }; 
```
easy  
接下来实现 entity 的移动,根据速度移动位置即可(还要防止跑出屏幕)
```C++
struct EVelocity { int x, y; }; 
ENTITY_STATE(EVelocity , Vec);
auto move_entity = [](ELocation& loc, const EVelocity& vel)
{
    loc.x = clamp(loc.x + vel.x, 48); 
    loc.y = clamp(loc.y + vel.y, 28);
};
```
接下来让它根据玩家的操作动起来,通过wasd来改变方向,且不能掉头,只能转向
```C++
auto move_input = [](EVelocity& vel) { 
    EVelocity newVel = vel;
    char input = last_input();
    switch (in)
    {
        case 'a': newVel = { -1, 0 }; break;
        case 'd': newVel = { 1, 0 }; break;
        case 'w': newVel = { 0, -1 }; break;
        case 's': newVel = { 0, 1 }; break;
    }
    if ((vel.x * newVel.x + vel.y * newVel.y) == 0) vel = newVel; //只能转向
};
```
点乘判断垂直 
这时我们的蛇还只有头没有身体,来加上身体  
**这里的实现思路比较有意思了,我们不把蛇身当做'蛇身',而是当做残影形成的拖尾,残影持续的时间越长,蛇就越长.** 
那么首先实现残影的消散, 即倒计时到零的时候杀死实体
```C++
struct ELifeTime { int n; }; //计时死亡
ENTITYSTATE(ELifeTime , Vec);
auto life_time(ELifeTime& life, Entity self, GEntities& entities)
{
    if (--life.n < 0) entities.Kill(self);
}
```
easy  
最后一个逻辑是生成残影,残影需要三个状态,位置,样子,持续时间  
注意生产器比较少,所以用Hash储存节省空间  
这里通过State<Xxx>来取得状态本身
```C++
struct ESpawner { int life; };
ENTITYSTATE(ESpawner , Hash);
auto spawn(const ESpawner& sp, const ELocation& loc, GEntities& entities, 
            State<ELifeTime>& lifetimes, State<ELocation>& locations,
            State<EAppearance>& appearances)
{
    auto e = entities.Create();
    lifetimes.Create(e, sp.life);
    locations.Create(e, loc.x, loc.y);
    apperances.Create(e, '*');
}
```
**记得声明实体的创建**  
还有一点需要注意的是,残影会持续 lifetime+1 帧,因为删除会延后一帧  
最后,放置一个"蛇头",它可以根据输入移动(EVelocity,ELocation),它可以显示(EAppearance),它可以生成残影(ESpawner).
```C++
void spawn_snake(GEntities& entities, State<EVelocity>& velocities,
                State<ESpawner>& spawners, State<ELocation>& locations,
                State<EAppearance>& appearances)
{
    auto e = entities.Create();
    velocities.Create(e, 0, 0);
    spawners.Create(e, 5);
    locations.Create(e,15, 8);
    apperances.Create(e, 'o');
}
```

至此,所有的逻辑都已经实现完成了,是时候让他们运作起来了  
第一步,构建世界,创建所有的状态
```C++
ESL::States states;
states.Create<EVelocity>();
states.Create<EVelocity>();
states.Create<ELocation>();
states.Create<EAppearance>();
states.Create<ESpawner>();
```
还有, 放一个蛇头
```C++
ESL::Dispatch(states, spawn_snake);
```

第二步,拼接组建我们的逻辑  
生成残影要在移动之前,残影消散要在生成残影之后,移动输入需要在移动之前,渲染需要在所有动作之后,嗯,一切都是如此清晰.
```C++
ESL::LogicGraphBuilder graphBuilder(states);
graphBuilder.Create(spawn, "spawn");
graphBuilder.Create(life_time, "life_time").dependencies.insert("spawn"); //残影消散要在生成残影之后
graphBuilder.Create(move_input , "move_input ");
graphBuilder.Create(move_entity , "move_entity ").dependencies.insert("move_input"); //移动输入需要在移动之前
graphBuilder.Create(draw_entity, "draw_entity").dependencies.insert("move_entity ", "spawn"); //渲染需要在所有动作之后

ESL::LogicGraph logicGraph;
graphBuilder.Build(logicGraph);
```
最后, 直接循环跑起来就皆大欢喜了.
```C++
while (1) //帧循环
{
	std::this_thread::sleep_for(std::chrono::milliseconds{ 1000 / 20 });
	logicGraph.Flow();
	//...renderer.swapchain();
}
```

***

从上面可以看出来ESL有着足够的灵活性  
**最后祝你.身体健康.**

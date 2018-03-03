# ESL
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
* 同时,我们还希望能有很好的解耦能力.


***
### ESL的设计与组成

#### 对于并行

最大的问题是**数据竞争**,什么时候会发生数据竞争？
1. 操作同一个状态
2. 操作同时在两个线程中执行
3. 操作不是两个读操作
4. 操作不是异步的

为了避免数据竞争,需要破坏掉其中至少一个条件,一般有如下对应方案
1. 当其他线程在读时, 创建一个新的副本
2. 线性执行, 放弃多线程(对于发生竞争的线程)
3. 延迟写操作
4. 锁,原子操作

ESL默认选择了破坏第二个条件,如果可能出现竞争,则直接放弃这些竞争线程的并行  
根据上面列出来的竞争条件,要检测出现竞争的可能性,这需要两个信息：
1. 操作状态
2. 读(或)写

在一般的C系程序中比较难做到这一点,原因是我们的很多函数都不是纯函数
所以ESL把函数限制到了“纯函数”,称为逻辑

这样就可以把一个逻辑分解成两个部分：拿取,分派
拿取指拿取一个逻辑所需要的状态,这样第一个信息(操作状态)就到手了

对于第二个信息,在资源上加上限定符 const 即可

**这便引入了一个状态管理器,在 ESL 里即 ESL::States**

在获得了足够的休息之后,就可以计算竞争了  
但是需要注意的是竞争的逻辑之间的顺序是模棱两可的(谁先谁后),这需要人为检查是否正确并修正   
在全部计算后后,能得到一个依赖图(有向无环图)来描述如何并行

**这便引入了一个图构建器和一个流图,在 ESL 里即 ESL::LogicGraphBuilder 和 ESL::LogicGraph**

#### 对于缓存友好


主要的问题是**缓存失效**,一般有如下情景
1. 调用虚函数
2. 访问大对象却只使用少数的几个属性

针对游戏来说,60%以上的类都会有一个以上的实例,整个逻辑里面会有大量的遍历更新,  
简单来讲,就是需要把每次遍历**需要访问的数据放在一起**,那么主要的思路就是拆开大对象  
一般的思路是把 **AoS(大对象数组)改成 SoA(属性数组对象)**  
这样在遍历的时候,就只会缓存我们需要的数据,如果遍历的连续性足够强,则会有很高的缓存命中率  
同时,顾及到上文对并行的设计,**属性容器也应该是一个状态**,所以需要(从对象中拆出来)放入管理器  
这种被完全击碎了的对象被ESL称为实体(Entity)

**这便引入了一个属于实体的状态, 在 ESL 里即 ESL::EntityState**  
作为区分,其他状态在 ESL 里称为 ESL::GlobalState

#### 对于解耦
主要的问题在于**分派**(也就是程序中动态的部分)

在上文中,对象被完全击碎了,但是即使它失去了数据的操作权  
它依然有所有权,依然是有意义的东西,称之为实体  
那么应该怎么应对这种实体呢?参考下面的对比:   
```
OOP 使用一种基于数据面向代码的模式
即参数确定,代码进行分派
```
```
ESL 使用一种基于代码面向数据的模式
即代码确定,参数进行分派
```
这里的参数指实参  
于是要做的就是将实体分派给逻辑(根据实体状态和逻辑形参进行匹配分派)

**这便引入了一个分派器,在 ESL 里即　ESL::Dispatcher**
***

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

从上面可以看出来ESL有着足够的灵活性与高性能  
**最后祝你.身体健康.**
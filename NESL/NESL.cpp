#include "NESL.h"
#include "States.h"
#include "LogicGraph.h"
#include <iostream>

struct EName { const char* _; };
struct GSuffix { const char* _; };
namespace ESL
{
	template<>
	struct TState<EName> { using type = EntityState<Vec<EName>>; };

	template<>
	struct TState<GSuffix> { using type = GlobalState<GSuffix>; };




}

//                  get state,dispatch to every entity witch has EName      
void Logic_ShowName(const EName& name);








//                    get global state     , write state
void Logic_ShowSuffix(const GSuffix& suffix, const EName& name);


int main()
{
	ESL::States st;
	auto& entities = st.Entities();
	ESL::Entity e = entities.TrySpawn().value();
	ESL::Entity anotherE = entities.TrySpawn().value();

	st.Create<GSuffix>("Sucks");
	auto& names = st.Create<EName>();
	names.Create(e, "BenzzZX");
	names.Create(anotherE, "Asshole");
	
	ESL::LogicGraphBuilder graphBuilder(st);
	graphBuilder.Create(Logic_ShowSuffix, "Show suffix");
	graphBuilder.Create(Logic_ShowName, "Show name");
	graphBuilder.Compile();
	graphBuilder.ExportGraphviz("test.gv");
	ESL::LogicGraph logicGraph;
	graphBuilder.Build(logicGraph);
	
	logicGraph.Flow();
    return 0;
}
  
void Logic_ShowName(const EName& name) { std::cout << name._ << std::endl; }


void Logic_ShowSuffix(const GSuffix& suffix, const EName& name) { std::cout << suffix._ << std::endl; }
#include "NESL.h"
#include "States.h"
#include "LogicGraph.h"
#include <iostream>

struct EName
{
	const char* _;
};

struct GSuffix
{
	const char* _;
};

namespace ESL
{
	template<>
	struct TState<EName> { using type = EntityState<Vec<EName>>; };

	template<>
	struct TState<GSuffix> { using type = GlobalState<GSuffix>; };
}
//                  get state        
void Logic_ShowName(const EName& name)
{
	std::cout << name._ << std::endl;
}

void Logic_ShowSuffix(const GSuffix& suffix)
{
	std::cout << suffix._ << std::endl;
}

int main()
{
	ESL::States st;
	auto& entities = st.Get<ESL::GEntities>()->Raw();
	ESL::Entity e = entities.TrySpawn().value();
	ESL::Entity anotherE = entities.TrySpawn().value();

	st.Create<GSuffix>("Sucks");
	auto& names = st.Create<EName>();
	names.Create(e, "BenzzZX");
	names.Create(anotherE, "Asshole");
	
	ESL::LogicGraphBuilder graphBuilder(st);
	graphBuilder.Create(Logic_ShowSuffix, "Show suffix");
	graphBuilder.Create(Logic_ShowName, "Show name");
	ESL::LogicGraph logicGraph;
	graphBuilder.Build(logicGraph);
	
	logicGraph.Flow();
    return 0;
}


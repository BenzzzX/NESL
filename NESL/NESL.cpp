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
	struct TState<EName> { using type = EntityState<Hash<EName>>; };

	template<>
	struct TState<GSuffix> { using type = GlobalState<GSuffix>; };
}
//                  get state        , get global state     , get state container        , get entity   , get entities
void Logic_ShowName(const EName& name, const GSuffix& suffix, ESL::Hash<EName>& nameState, ESL::Entity e, ESL::GEntities& entities)
{
	//nameState.Create(e);
	std::cout << name._ << " " << suffix._ << std::endl;
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

	auto fetchedState = ESL::FetchFor(st, Logic_ShowName);

	ESL::Dispatch(fetchedState, Logic_ShowName);
    return 0;
}


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

void Logic_ShowName(const EName& name, const GSuffix& suffix)
{
	std::cout << name._ << " " << suffix._ << std::endl;
}

int main()
{
	ESL::States st;
	auto& entities = *st.Get<ESL::GEntities>();
	ESL::Entity e = entities.TrySpawn().value();
	ESL::Entity anotherE = entities.TrySpawn().value();

	st.Create<GSuffix>("Sucks");
	auto& names = st.Create<EName>();
	names.Create(e, "BenzzZX");
	names.Create(anotherE, "Asshole");

	ESL::Dispatch(st, Logic_ShowName);
    return 0;
}


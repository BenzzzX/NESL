#pragma once
#include <tbb\tbb.h>
#include "HBV.h"
#include "Dispather.h"
#include "vector.h"
#include <iostream>

namespace HBV
{
	template<typename T, typename F>
	void for_each_paralell(const T& vec, const F& f)
	{
		//just bufferring indices and do it parallel
		//lazy growing static thread local buffer
		std::vector<index_t> IndicesBuffer{};
		index_t size = HBV::last<2>(vec) + 1;
		IndicesBuffer.reserve(size);
		for_each<2>(vec, [&IndicesBuffer](index_t id)
		{
			IndicesBuffer.push_back(id);
		});
		tbb::parallel_for_each(std::begin(IndicesBuffer), std::end(IndicesBuffer), [&f, &vec](index_t id)
		{
			HBV::flag_t node = vec.layer3(id);
			index_t prefix = id << HBV::BitsPerLayer;
			do
			{
				index_t low = HBV::lowbit_pos(node);
				node &= ~(HBV::flag_t(1) << low);
				f(prefix | low);
			} while (node); 
		});
	}
}

namespace ESL
{
	template<typename F, typename S>
	void DispatchParallel(S states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		using DecayArgument = MPL::map_t<std::decay_t, Argument>;
		using States = MPL::fliter_t<IsState, DecayArgument>;
		using EntityStates = MPL::map_t<State, MPL::fliter_t<IsRawEntityState, States>>;

		if constexpr(MPL::size<EntityStates>{} == 0 && !MPL::contain_v<Entity, DecayArgument>) //不进行分派
		{
			MPL::rewrap_t<Dispatcher::DispatchHelper, Argument>::Dispatch(states, logic);
		}
		else
		{
			const auto available = MPL::rewrap_t<Dispatcher::ComposeHelper, EntityStates>::ComposeBitVector(states);
			HBV::for_each_paralell(available, [&states, &logic](index_t i) //分派
			{
				MPL::rewrap_t<Dispatcher::EntityDispatchHelper, Argument>::Dispatch(states, i, logic);
			});
		}
	}
}


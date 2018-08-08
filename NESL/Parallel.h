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
		lni::vector<index_t> IndicesBuffer;
		IndicesBuffer.reserve(64u);
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
		using States = MPL::filter_t<IsState, DecayArgument>;
		using RawEntityStates = MPL::filter_t<IsRawEntityState, States>;

		using ExplictFilters = MPL::filter_t<is_filter, DecayArgument>;
		using ImplictFilters = MPL::map_t<Has, RawEntityStates>;
		typename Dispatcher::CheckFilters<ExplictFilters>::type checker; (void)checker;
		using Filters = typename Dispatcher::FixFilters<ExplictFilters, ImplictFilters>::type;

		if constexpr(MPL::size<Filters>{} == 0 && !MPL::contain_v<Entity, DecayArgument>) //不进行分派
		{
			MPL::rewrap_t<Dispatcher::DispatchHelper, DecayArgument>::Dispatch(states, logic);
		}
		else
		{
			const auto available = MPL::rewrap_t<Dispatcher::ComposeHelper, Filters>::ComposeBitVector(states);
			HBV::for_each_paralell(available, [&states, &logic](index_t i) //分派
			{
				MPL::rewrap_t<Dispatcher::EntityDispatchHelper, DecayArgument>::Dispatch(states, i, logic);
			});
		}
	}
}


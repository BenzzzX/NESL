#pragma once
#include "HBV.h"
#include "Dispather.h"
#include "vector.h"
#include <iostream>

namespace ESL
{
	namespace Dispatcher
	{
		template<typename... Ts>
		struct FlattenDispatchHelper
		{
			template<typename T, typename S, typename A>
			static decltype(auto) Take(S &states, index_t id, A &datas, std::true_type)
			{
				return std::get<T*>(datas)[id];
			}

			template<typename T, typename S, typename A>
			static decltype(auto) Take(S &states, index_t id, A &datas, std::false_type)
			{
				if constexpr(std::is_same<T, Entity>{})
				{
					return std::get<Entity*>(datas)[id];
				}
				else if constexpr(is_filter<T>::value)
				{
					return T{};
				}
				else
				{
					auto &state = MPL::nonstrict_get<const T&>(states);
					if constexpr(IsRawState<T>{}) //GlobalState自动解包
					{
						return state.Raw();
					}
					else 
					{
						return state;
					}
				}
			}


			template<typename F, typename S, typename A>
			static void Dispatch(S &states, index_t id, A &datas, F&& f)
			{
				f(Take<Ts>(states, id, datas, IsRawEntityState<Ts>{})...);
			}
		};
	}

	//慢得伤心, Flatten的代价很高
	template<typename F, typename S>
	void DispatchFlatten(S states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		using DecayArgument = MPL::map_t<std::decay_t, Argument>;
		using States = MPL::filter_t<IsState, DecayArgument>;
		using RawEntityState = MPL::filter_t<IsRawEntityState, States>;

		using ExplictFilters = MPL::filter_t<is_filter, DecayArgument>;
		using ImplictFilters = MPL::map_t<Has, RawEntityStates>;
		typename Dispatcher::CheckFilters<ExplictFilters>::type checker; (void)checker;
		using Filters = typename Dispatcher::FixFilters<ExplictFilters, ImplictFilters>::type;
		
		using PerEntityData = MPL::concat_t<MPL::typelist<Entity>, RawEntityState>;
		static_assert(MPL::size<Filters>{} != 0 || MPL::contain_v<Entity, DecayArgument>, "wrong parameter");
		
		const auto available = MPL::rewrap_t<Dispatcher::ComposeHelper, Filters>::ComposeBitVector(states);
		using DataArrays = MPL::map_t<std::add_pointer_t, PerEntityData>;
		MPL::rewrap_t<std::tuple, DataArrays> dataArrays;
		lni::vector<index_t> indexArray;
		indexArray.reserve(64u);
		HBV::for_each(available, [&indexArray](index_t i)
		{
			indexArray.push_back(i);
		});
		auto size = indexArray.size();
		MPL::for_tuple(dataArrays, [size, &states, &indexArray](auto& point)
		{
			using type = std::remove_pointer_t<std::remove_reference_t<decltype(point)>>;
			if constexpr(!std::is_same_v<type, Entity>)
			{
				auto state = MPL::nonstrict_get<const State<type>&>(states);
				point = (type*)malloc(sizeof(type)*size); //分配数组
				for (int i = 0; i < size; ++i)
					point[i] = state.Get(indexArray[i]); //取出数据,整理到数组
			}
			else
			{
				auto state = MPL::nonstrict_get<const State<Entities>&>(states).Raw();
				point = (Entity*)malloc(sizeof(Entity)*size);
				for (int i = 0; i < size; ++i)
					point[i] = state.Get(indexArray[i]);
			}
		});

		for (int i = 0; i < size; ++i) //在数组上执行函数
			MPL::rewrap_t<Dispatcher::FlattenDispatchHelper, DecayArgument>::Dispatch(states, i, dataArrays, logic);

		MPL::for_tuple(dataArrays, [size, &states, &indexArray](auto point)
		{
			using type = std::remove_pointer_t<decltype(point)>;
			if constexpr(!std::is_same_v<type, Entity>)
				if constexpr(MPL::contain_v<State<type>&, MPL::rewrap_t<MPL::typelist, S>>)
				{
					auto state = std::get<State<type>&>(states);
					for (int i = 0; i < size; ++i)
						state.Get(indexArray[i]) = point[i]; //写回非const数据
				}
			free(point);
		});
	}
}
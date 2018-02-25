#pragma once
#include "States.h"
#include "MPL.h"

namespace ESL
{
	class LogicGraph
	{

	};

	class Dispatcher
	{
		template<typename T>
		struct IsEntityState : std::false_type {};

		template<typename T>
		struct IsEntityState<EntityState<T>> : std::true_type {};

		template<typename T>
		using IsRawEntityState = IsEntityState<State<T>>;

		template<typename T>
		struct RawEntityState { using type = T; };

		template<template<typename V> class C, typename T>
		struct RawEntityState<EntityState<C<T>>> { using type = T; };

		template<typename T>
		struct IsGlobalState : std::false_type {};

		template<typename T>
		struct IsGlobalState<GlobalState<T>> : std::true_type {};

		template<typename... Ts>
		struct ComposeHelper
		{
			static auto ComposeBitVector(States &states)
			{
				return HBV::compose(HBV::and_op, states.Get<GEntities>()->Available(), states.Get<Ts>()->Available()...);
			}
		};

		template<typename... Ts>
		struct EntityDispatchHelper
		{


			template<typename T>
			static decltype(auto) Take(States &states, index_t id, std::true_type)
			{
				using decayT = std::decay_t<T>;
				Entity e = states.Get<GEntities>()->Get(id);
				return states.Get<decayT>()->Get(e);
			}

			template<typename T>
			static decltype(auto) Take(States &states, index_t id, std::false_type)
			{
				using decayT = std::decay_t<T>;
				
				if constexpr(IsEntityState<decayT>{})
				{
					return *states.Get<typename RawEntityState<decayT>::type>();
				}

				else if constexpr(std::is_same<T, Entity>{})
				{
					return states.Get<GEntities>()->Get(id);
				}
				else //if constexpr(IsGlobalState<T>{})
				{
					return *states.Get<decayT>();
				}
			}

			template<typename F>
			static void Dispatch(States &states, index_t id, F&& f)
			{
				f(Take<Ts>(states, id, IsRawEntityState<std::decay_t<Ts>>{})...);
			}
		};

		template<typename... Ts>
		struct DispatchHelper
		{
			template<typename T>
			static auto& Take(States &states)
			{
				using decayT = std::decay_t<T>;
				if constexpr(IsEntityState<decayT>{})
				{
					return *states.Get<typename RawEntityState<decayT>::type>();
				}

				else //if constexpr(IsGlobalState<T>{})
				{
					return *states.Get<decayT>();
				}
			}

			template<typename F>
			static void Dispatch(States &states, F&& f)
			{
				f(Take<Ts>(states)...);
			}
		};
	public:
		template<typename F>
		friend void Dispatch(States &states, F&& logic)
		{
			using Trait = MPL::generic_function_trait<std::decay_t<F>>;

			using Argument = typename Trait::argument_type;

			using DecayArgument = MPL::map_t<std::decay_t, Argument>;

			using RawEntityStates = MPL::fliter_t<Dispatcher::IsRawEntityState, DecayArgument>;

			if constexpr(MPL::size<RawEntityStates>{} == 0)
			{
				MPL::rewrap_t<Dispatcher::DispatchHelper, Argument>::Dispatch(states, logic);
			}
			else
			{
				const auto available = MPL::rewrap_t<Dispatcher::ComposeHelper, RawEntityStates>::ComposeBitVector(states);
				HBV::for_each(available, [&states, &logic](index_t i)
				{
					MPL::rewrap_t<Dispatcher::EntityDispatchHelper, Argument>::Dispatch(states, i, logic);
				});
			}
		}
	};

	template<typename F>
	void Dispatch(States &states, F&& logic);
	

	class LogicGraphBuilder
	{
	};

		
}
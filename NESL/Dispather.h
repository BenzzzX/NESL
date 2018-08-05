#pragma once
#include "States.h"
#include "MPL.h"

namespace ESL
{

	template<typename T>
	struct IsRawState : std::is_same<typename TStateTrait<StateNonstrict<T>>::Raw, T> {};

	

	template<typename T>
	struct IsRawEntityState : std::conjunction<IsRawState<T>, IsEntityState<T>> {};

	class Dispatcher
	{
		template<typename... Ts>
		struct ComposeHelper;

		template<typename U,typename... Ts>
		struct ComposeHelper<U, Ts...>
		{
			template<typename S>
			static auto ComposeBitVector(S &states)
			{
				return HBV::compose(HBV::and_op, MPL::nonstrict_get<const U&>(states).Available(), MPL::nonstrict_get<const Ts&>(states).Available()...);
			}
		};

		template<>
		struct ComposeHelper<>
		{
			template<typename S>
			static auto ComposeBitVector(S &states)
			{
				return MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Available();
			}
		};


		template<typename... Ts>
		struct EntityDispatchHelper
		{
			template<typename T, typename S>
			static decltype(auto) Take(S &states, index_t id, std::true_type)
			{
				auto &state = MPL::nonstrict_get<StateStrict<T>&>(states);

				Entity e = MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Get(id);
				return *state.Get(e);
			}

			template<typename T, typename S>
			static decltype(auto) Take(S &states, index_t id, std::false_type)
			{
				if constexpr(std::is_same<T, Entity>{})
				{
					return MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Get(id);
				}
				else
				{
					auto &state = MPL::nonstrict_get<StateStrict<T>&>(states);
					if constexpr(IsRawState<std::decay_t<T>>{}) //GlobalState自动解包
					{
						return state.Raw();
					}
					else 
					{
						return state;
					}
				}
			}

			template<typename F, typename S>
			static void Dispatch(S &states, index_t id, F&& f)
			{
				f(Take<Ts>(states, id, IsRawEntityState<std::decay_t<Ts>>{})...);
			}
		};

		template<typename... Ts>
		struct DispatchHelper
		{
			template<typename T, typename S>
			static auto& Take(S &states)
			{
				auto& state = MPL::nonstrict_get<StateStrict<T>&>(states);
				if constexpr(IsRawState<std::decay_t<T>>{})
				{
					return state.Raw();
				}
				else
					return state;
			}

			template<typename F, typename S>
			static void Dispatch(S &states, F&& f)
			{
				f(Take<Ts>(states)...);
			}
		};

		template<typename... Ts>
		struct FetchHelper
		{
			template<typename T>
			static T& Take(States &states)
			{
				using Raw = typename TStateTrait<std::decay_t<T>>::Raw;
				return *states.GetState<Raw>();
			}

			static auto Fetch(States &states)
			{
				return std::tie(Take<Ts>(states)...);
			}
		};

		template<typename S>
		struct FilterMutable
		{
			template<typename T>
			using Cond = std::negation<std::conjunction<MPL::contain<MPL::remove_const_t<T>, S>, MPL::is_const<T>>>;

			using type = MPL::fliter_t<Cond, S>;
		};
	public:
		template<typename F>
		friend auto FetchFor(States &states, F&& logic);


		template<typename F, typename S>
		friend void Dispatch(S states, F&& logic);


		template<typename F, typename S>
		friend void DispatchParallel(S states, F&& logic);

		template<typename F, typename S>
		friend void DispatchEntity(S states, F&& logic, Entity e);
		
	};

	template<typename F>
	auto FetchFor(States &states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		using TrueArgument = MPL::concat_t<MPL::typelist<const Entities>, Argument>; //添加上Entities得到真正的参数

		using NeedStates = MPL::map_t<StateStrict, TrueArgument>; //获得需要的States
		using FetchStates = typename Dispatcher::FilterMutable<MPL::unique_t<NeedStates>>::type; //去重,读写保留写

		return MPL::rewrap_t<Dispatcher::FetchHelper, FetchStates>::Fetch(states); //拿取资源
	}


	template<typename F, typename S>
	void Dispatch(S states, F&& logic)
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
			HBV::for_each(available, [&states, &logic](index_t i) //分派
			{
				MPL::rewrap_t<Dispatcher::EntityDispatchHelper, Argument>::Dispatch(states, i, logic);
			});
		}
	}

	template<typename F, typename S>
	void DispatchEntity(S states, F&& logic, Entity e)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		if (MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Alive(e))
		{
			MPL::rewrap_t<Dispatcher::EntityDispatchHelper, Argument>::Dispatch(states, e.id, logic);
		}
	}


	template<typename F>
	auto Dispatch(States &states, F&& logic)
	{
		Dispatch(FetchFor(states, logic), logic);
	}

	template<typename F>
	auto DispatchEntity(States &states, F&& logic, Entity e)
	{
		DispatchEntity(FetchFor(states, logic), logic, e);
	}

}
#pragma once
#include "States.h"
#include "MPL.h"

namespace ESL
{

	template<typename T>
	struct IsRawState : std::is_same<typename TStateTrait<StateNonstrict<T>>::Raw, T> {};

	template<typename T>
	struct IsRawEntityState : std::conjunction<std::negation<is_filter<T>>, IsRawState<T>, IsEntityState<T>> {};

	namespace Dispatcher
	{
		template<typename... Ts>
		struct ComposeHelper;

		template<typename U,typename... Ts>
		struct ComposeHelper<U, Ts...>
		{
			template<typename S, typename F, Trace type>
			__forceinline static decltype(auto) Take(S &states, Filter_t<F, type>)
			{
				return MPL::nonstrict_get<const State<F>&>(states).template Available<type>();
			}

			template<typename S>
			__forceinline static decltype(auto) ComposeBitVector(S &states)
			{
				return HBV::compose(HBV::and_op, Take(states, U{}), Take(states, Ts{})...);
			}
		};

		template<>
		struct ComposeHelper<>
		{
			template<typename S>
			__forceinline static decltype(auto) ComposeBitVector(S &states)
			{
				return MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Available();
			}
		};


		template<typename... Ts>
		struct EntityDispatchHelper
		{
			template<typename T, typename S>
			__forceinline static decltype(auto) Take(S &states, index_t id, std::true_type)
			{
				return MPL::nonstrict_get<const State<T>&>(states).Get(id);
			}

			template<typename T, typename S>
			__forceinline static decltype(auto) Take(S &states, index_t id, std::false_type)
			{
				if constexpr(std::is_same<T, Entity>{})
				{
					return MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Get(id);
				}
				else if constexpr(is_filter<T>::value)
				{
					return T{};
				}
				else
				{
					
					if constexpr(IsRawState<T>{}) //GlobalState自动解包
					{
						auto &state = MPL::nonstrict_get<const State<T>&>(states);
						return state.Raw();
					}
					else 
					{
						auto &state = MPL::nonstrict_get<const T&>(states);
						return state;
					}
				}
			}

			template<typename F, typename S>
			__forceinline static void Dispatch(S &states, index_t id, F&& f)
			{
				f(Take<Ts>(states, id, IsRawEntityState<Ts>{})...);
			}
		};

		template<typename... Ts>
		struct DispatchHelper
		{
			template<typename T, typename S>
			__forceinline static decltype(auto) Take(S &states)
			{
				auto& state = MPL::nonstrict_get<const State<T>&>(states);
				if constexpr(IsRawState<T>{})
				{
					return state.Raw();
				}
				else
					return state;
			}

			template<typename F, typename S>
			__forceinline static void Dispatch(S &states, F&& f)
			{
				f(Take<Ts>(states)...);
			}
		};

		template<typename... Ts>
		struct FetchHelper
		{
			template<typename T>
			__forceinline static T& Take(States &states)
			{
				using Raw = typename TStateTrait<std::decay_t<T>>::Raw;
				return *states.GetState<Raw>();
			}

			__forceinline static auto Fetch(States &states)
			{
				return std::tie(Take<Ts>(states)...);
			}
		};

		template<typename S>
		struct KeepMutable
		{
			template<typename T>
			using Cond = std::negation<std::conjunction<MPL::contain<MPL::remove_const_t<T>, S>, MPL::is_const<T>>>;

			using type = MPL::filter_t<Cond, S>;
		};

		template<typename T, typename... Ts>
		struct MergeFilter
		{
			template<typename... Ts>
			static constexpr Trace GetType()
			{
				return (Trace)((Ts::type) | ...);
			}
			using type = Filter_t<typename T::target, GetType<T, Ts...>() > ;
		};
		
		template<typename L>
		struct CheckFilters
		{
			template<typename T>
			struct Checker
			{
				template<typename T>
				struct SameTarget : std::false_type {};

				template<Trace type>
				struct SameTarget<Filter_t<typename T::target, type>> : std::true_type {};

				static_assert(!((T::type & Has) && (T::type & Remove)), "Has filter conflict with Remove filter");
				static_assert(!((T::type & Has) && (T::type & HasNot)), "Has filter conflict with HasNot filter");
				static_assert(!((T::type & HasNot) && (T::type & Create)), "HasNot filter conflict with Create filter");
				static_assert(MPL::size<MPL::filter_t<SameTarget, L>>{} == 1, "Multiple filter with same target is not allowd");
			};
			using type = MPL::rewrap_t<std::tuple, MPL::map_t<Checker, L>>;
		};

		template<typename L, typename S>
		struct FixFilters
		{
			template<typename T>
			struct ShouldAdd
			{
				template<typename T>
				struct SameTarget : std::false_type {};

				template<Trace type>
				struct SameTarget<Filter_t<typename T::target, type>> : std::true_type {};

				static constexpr auto value = MPL::size<MPL::filter_t<SameTarget, L>>{} == 0;
			};
			using ToAdd = MPL::filter_t<ShouldAdd, S>;
			using type = MPL::concat_t<L, ToAdd>;
		};
	};

	template<typename F>
	auto FetchFor(States &states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		using TrueArgument = MPL::concat_t<MPL::typelist<const Entities>, Argument>; //添加上Entities得到真正的参数

		using NeedStates = MPL::map_t<StateStrict, TrueArgument>; //获得需要的States
		using FetchStates = typename Dispatcher::KeepMutable<MPL::unique_t<NeedStates>>::type; //去重,读写保留写

		return MPL::rewrap_t<Dispatcher::FetchHelper, FetchStates>::Fetch(states); //拿取资源
	}
	template<typename T>
	using DefaultFilter = Filter_t<T, ESL::Trace::Has>;

	template<typename F, typename S>
	void Dispatch(S states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		using DecayArgument = MPL::map_t<std::decay_t, Argument>;
		using States = MPL::filter_t<IsState, DecayArgument>;
		using RawEntityStates = MPL::filter_t<IsRawEntityState, States>;

		using ExplictFilters = MPL::filter_t<is_filter, DecayArgument>;
		using ImplictFilters = MPL::map_t<DefaultFilter, RawEntityStates>;
		typename Dispatcher::CheckFilters<ExplictFilters>::type checker; (void)checker;
		using Filters = typename Dispatcher::FixFilters<ExplictFilters, ImplictFilters>::type;
		
		if constexpr(MPL::size<Filters>{} == 0 && !MPL::contain_v<Entity, DecayArgument>) //不进行分派
		{
			MPL::rewrap_t<Dispatcher::DispatchHelper, DecayArgument>::Dispatch(states, logic);
			
		}
		else
		{
			const auto available{ MPL::rewrap_t<Dispatcher::ComposeHelper, Filters>::ComposeBitVector(states) };
			HBV::for_each(available, [&states, &logic](index_t i) //分派
			{
				MPL::rewrap_t<Dispatcher::EntityDispatchHelper, DecayArgument>::Dispatch(states, i, logic);
			});
			return (void)available;
		}
	}

	template<typename F, typename S>
	void DispatchEntity(S states, F&& logic, Entity e)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		if (MPL::nonstrict_get<const GlobalState<Entities>&>(states).Raw().Alive(e))
		{
			MPL::rewrap_t<Dispatcher::EntityDispatchHelper, DecayArgument>::Dispatch(states, e.id, logic);
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
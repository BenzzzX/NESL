#pragma once
#include "States.h"
#include "MPL.h"

namespace ESL
{
	class Dispatcher
	{
		template<typename T>
		using unwrap2_t = MPL::unwrap_t<MPL::unwrap_t<T>>;

		template<typename T>
		struct IsEntityState : std::is_same<T, EntityState<MPL::unwrap_t<T>>> {};

		template<typename T>
		using IsRawEntityState = IsEntityState<State<T>>;

		template<typename T>
		struct IsGlobalState : std::is_same<T, GlobalState<MPL::unwrap_t<T>>> {};

		template<typename T>
		using IsRawGlobalState = std::conjunction<IsGlobalState<State<T>>>;

		template<typename... Ts>
		struct ComposeHelper
		{
			template<typename S>
			static auto ComposeBitVector(S &states)
			{
				return HBV::compose(HBV::and_op, MPL::nonstrict_get<const GlobalState<GEntities>&>(states).Raw().Available(), MPL::nonstrict_get<const Ts&>(states).Available()...);
			}
		};
		template<typename T>
		using WrapStateNostrict = State<unwrap2_t<std::decay_t<T>>>;

		template<typename T>
		using WrapState = std::conditional_t<MPL::is_const_v<T>, MPL::add_const_t<WrapStateNostrict<T>>, WrapStateNostrict<T>>;

		template<typename... Ts>
		struct EntityDispatchHelper
		{
			template<typename T, typename S>
			static decltype(auto) Take(S &states, index_t id, std::true_type)
			{
				using WrapT = WrapState<T>;
				auto &state = MPL::nonstrict_get<WrapT&>(states);

				Entity e = MPL::nonstrict_get<const GlobalState<GEntities>&>(states).Raw().Get(id);
				return *state.Get(e);
			}

			template<typename T, typename S>
			static decltype(auto) Take(S &states, index_t id, std::false_type)
			{
				using WrapT = WrapState<T>;
				using DecayT = std::decay_t<T>;

				if constexpr(std::is_same<T, Entity>{})
				{
					return MPL::nonstrict_get<const GlobalState<GEntities>&>(states).Raw().Get(id);
				}
				else
				{
					auto &state = MPL::nonstrict_get<WrapT&>(states);

					if constexpr(IsEntityState<DecayT>{})
					{
						return state;
					}
					else if constexpr(IsRawGlobalState<DecayT>{})
					{
						return state.Raw();
					}
					else //if constexpr(IsGlobalState<DecayT>{})
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
				auto& state = MPL::nonstrict_get<WrapState<T>&>(states);
				if constexpr(IsRawGlobalState<std::decay_t<T>>{})
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


		//TODO:
		//  Fetch with const
		template<typename... Ts>
		struct FetchHelper
		{
			template<typename T>
			static T& Take(States &states)
			{
				using Raw = unwrap2_t<std::decay_t<T>>;
				return *states.Get<Raw>();
			}

			static auto Fetch(States &states)
			{
				return std::tie(Take<Ts>(states)...);
			}
		};

		template<typename T, typename... Ts>
		struct FxxkMSVC
		{
			using type = MPL::concat_t<MPL::typelist<WrapState<T>>, typename FxxkMSVC<Ts...>::type>;
		};

		template<typename T>
		struct FxxkMSVC<T>
		{
			using type = MPL::typelist<WrapState<T>, const GlobalState<GEntities>>;
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
		
	};

	template<typename F>
	auto FetchFor(States &states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		//using FitchStates = MPL::unique_t<MPL::map_t<Dispatcher::WrapState, Argument>>;
		using FetchStates = typename Dispatcher::FilterMutable<MPL::unique_t<typename MPL::rewrap_t<Dispatcher::FxxkMSVC, Argument>::type>>::type;

		return MPL::rewrap_t<Dispatcher::FetchHelper, FetchStates>::Fetch(states);
	}

	template<typename F, typename S>
	void Dispatch(S states, F&& logic)
	{
		using Trait = MPL::generic_function_trait<std::decay_t<F>>;
		using Argument = typename Trait::argument_type;
		using DecayArgument = MPL::map_t<std::decay_t, Argument>;

		using EntityStates = MPL::map_t<State, MPL::fliter_t<Dispatcher::IsRawEntityState, DecayArgument>>;

		if constexpr(MPL::size<EntityStates>{} == 0)
		{
			MPL::rewrap_t<Dispatcher::DispatchHelper, Argument>::Dispatch(states, logic);
		}
		else
		{
			const auto available = MPL::rewrap_t<Dispatcher::ComposeHelper, EntityStates>::ComposeBitVector(states);
			HBV::for_each(available, [&states, &logic](index_t i)
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
		if (MPL::nonstrict_get<const GlobalState<GEntities>&>(states).Raw().Alive(e))
		{
			MPL::rewrap_t<Dispatcher::EntityDispatchHelper, Argument>::Dispatch(states, e.id, logic);
		}
	}


	template<typename F>
	auto Dispatch(States &states, F&& logic)
	{
		Dispatch(FetchFor(states, logic), logic);
	}

}
#pragma once
#include <any>
#include <unordered_map>
#include <functional>
#include <bitset>
#include <atomic>
#include "GlobalState.h"
#include "Entity.h"
#include "EntityState.h"
#include "MPL.h"

namespace ESL
{
	template<typename T>
	struct TState;

	template<typename T>
	using State = typename TState<T>::type;

	template<>
	struct TState<Entities> { using type = GlobalState<Entities>; };

#define ENTITY_STATE(name, container, ...) \
namespace ESL \
{ \
	template<> \
	struct TState<name> { using type = EntityState<container<name>, __VA_ARGS__>; }; \
}

#define GLOBAL_STATE(name) \
namespace ESL \
{ \
	template<> \
	struct TState<name> { using type = GlobalState<name>; }; \
}

	template<typename T>
	struct IsState : MPL::is_complete<TState<T>> {};

	template<typename T>
	struct TStateTrait;

	struct TEntityState;
	struct TGlobalState;
	

	template<template<typename> class V, typename T, Trace... types>
	struct TStateTrait<EntityState<V<T>, types...>>
	{
		using Raw = T;
		using Type = TEntityState;
	};

	template<typename T>
	struct TStateTrait<GlobalState<T>>
	{
		using Raw = T;
		using Type = TGlobalState;
	};



	template<typename T>
	struct TStateNonstric
	{
		using State = State<T>;
		using Raw = T;
	};

	template<>
	struct TStateNonstric<Entity>
	{
		using State = GlobalState<Entities>;
		using Raw = Entities;
	};

	template<template<typename> class V, typename T, Trace... types>
	struct TStateNonstric<EntityState<V<T>, types...>> 
	{
		using State = EntityState<V<T>>;
		using Raw = T;
	};

	template<typename T>
	struct TStateNonstric<GlobalState<T>>
	{
		using State = GlobalState<T>;
		using Raw = T;
	};

	template<typename T>
	using StateNonstrict = typename TStateNonstric<std::decay_t<T>>::State;

	template<typename T>
	using RawNonstrict = typename TStateNonstric<T>::Raw;

	template<typename T>
	struct TStateStrict
	{
		using type = std::conditional_t<MPL::is_const_v<T>, MPL::add_const_t<StateNonstrict<T>>, StateNonstrict<T>>;
	};

	template<typename T, Trace t>
	struct TStateStrict<Filter<T, t>>
	{
		using type = const State<T>;
	};

	template<>
	struct TStateStrict<Entity>
	{
		using type = const GlobalState<Entities>;
	};

	template<typename T>
	using StateStrict = typename TStateStrict<T>::type;

	template<typename T>
	struct IsEntityState : std::is_same<typename TStateTrait<StateNonstrict<T>>::Type, TEntityState> {};

	class States
	{
		std::unordered_map<std::size_t, std::any> _states;
		std::vector<std::function<void(const HBV::bit_vector&)>> _onDoKill;
		std::vector<std::function<void()>> _onResetTracers;
		GlobalState<ESL::Entities>& _entities;
		
	public:
		States() : _entities(CreateState<ESL::Entities>()) {}

		void Tick()
		{
			auto& entities = _entities.Raw();
			for (auto &f : _onDoKill)
				f(entities._killed);
			entities.DoKill();
		}

		void ResetTracers()
		{
			for (auto &f : _onResetTracers)
				f();
		}

		auto& Entities()
		{
			return _entities.Raw();
		}

		template<typename T>
		auto* GetState() noexcept
		{
			if constexpr(std::is_same_v<ESL::Entities, T>)
			{
				return &_entities;
			}
			else
			{
				using ST = State<T>;
				auto it = _states.find(typeid(ST).hash_code());
				return it != _states.end() ? &std::any_cast<ST&>(it->second) : nullptr;
			}
		}

	private:

		template<typename T>
		void RegisterCallback(T& state)
		{
			_onDoKill.emplace_back([&state](const HBV::bit_vector& remove)
			{
				state.BatchRemove(remove);
			});
			_onResetTracers.emplace_back([&state]()
			{
				state.ResetTracers();
			});
		}
	public:
		template<typename T, std::enable_if_t<IsEntityState<State<T>>::value, int> = 0>
		auto &CreateState() noexcept
		{
			using ST = State<T>;
			auto &state = std::any_cast<ST&>(_states.insert({ typeid(ST).hash_code(), std::make_any<ST>()}).first->second);
			RegisterCallback(state);
			return state;
		}

		template<typename T, typename... Ts>
		auto &CreateState(Ts&&... args) noexcept
		{
			using ST = State<T>;
			auto &state = std::any_cast<ST&>(_states.insert({ typeid(ST).hash_code(), std::make_any<ST>(args...) }).first->second);
			return state;
		}

	private:
		
		template<typename T>
		void BatchSpawnComponent(std::pair<index_t, index_t> es, const T& arg)
		{
			auto state = GetState<T>();
			state->BatchCreate(es.first, es.second, arg);
		}

	public:

		Entity SpawnEntity()
		{
			auto e = _entities.Raw().ForceSpawn();
			return e;
		}

		template<typename... Ts>
		std::pair<index_t, index_t> BatchSpawnEntity(index_t n, const Ts&... args)
		{
			std::pair<index_t, index_t> es = _entities.Raw().BatchSpawn(n);
			std::initializer_list<int> _{ (BatchSpawnComponent(es, args),0)... };
			return es;
		}

		Entity GetEntity(index_t n)
		{
			return _entities.Raw().Get(n);
		}


	};
}


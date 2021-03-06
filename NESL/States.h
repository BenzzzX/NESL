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

	struct TEntityState;
	struct TGlobalState;
	



	template<typename T>
	struct TStateNonstrict
	{
		using State = State<T>;
		using Raw = T;
	};

	template<>
	struct TStateNonstrict<Entity>
	{
		using State = GlobalState<Entities>;
		using Raw = Entities;
	};

	template<template<typename> class V, typename T, Trace... types>
	struct TStateNonstrict<EntityState<V<T>, types...>> 
	{
		using State = EntityState<V<T>>;
		using Type = TEntityState;
		using Raw = T;
	};

	template<typename T>
	struct TStateNonstrict<GlobalState<T>>
	{
		using State = GlobalState<T>;
		using Type = TGlobalState;
		using Raw = T;
	};

	template<typename T>
	using StateNonstrict = typename TStateNonstrict<std::decay_t<T>>::State;

	template<typename T>
	using RawNonstrict = typename TStateNonstrict<T>::Raw;

	template<typename T>
	struct TStateStrict
	{
		using type = std::conditional_t<MPL::is_const_v<T>, MPL::add_const_t<StateNonstrict<T>>, StateNonstrict<T>>;
	};

	template<typename T, Trace t>
	struct TStateStrict<Filter_t<T, t>>
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
	struct IsEntityState : std::is_same<typename TStateNonstrict<StateNonstrict<T>>::Type, TEntityState> {};


	class States
	{
		std::unordered_map<std::size_t, std::any> _states;
		std::vector<EntityStateBase*> _entityStates;
		GlobalState<ESL::Entities>& _entities;
		
	public:
		States() : _entities(CreateState<ESL::Entities>()) {}

		void Tick(index_t growThreshold = 10u)
		{
			auto& entities = _entities.Raw();
			for (auto &e : _entityStates)
				e->BatchRemove(entities._killed);
			entities.DoKill();
			if (entities._freeCount <= growThreshold)
				entities.Grow();
		}

		void ResetTracers()
		{
			for (auto &e : _entityStates)
				e->ResetTracers();
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

	public:
		template<typename T, std::enable_if_t<IsEntityState<State<T>>::value, int> = 0>
		auto &CreateState() noexcept
		{
			using ST = State<T>;
			auto &state = std::any_cast<ST&>(_states.insert({ typeid(ST).hash_code(), std::make_any<ST>() }).first->second);
			_entityStates.emplace_back(&state);
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

		//注意:这里会遍历_entityStates,可能造成性能问题
		template<typename... Ts>
		Entity InstantiateEntity(index_t prototype)
		{
			Entity e = SpawnEntity();
			for (auto &s : _entityStates)
				if (s.Contain(prototype))
					s.Instantiate(e, prototype);
			return e;
		}

		template<typename... Ts>
		std::pair<index_t, index_t> BatchSpawnEntity(index_t n, const Ts&... args)
		{
			std::pair<index_t, index_t> es = _entities.Raw().BatchSpawn(n);
			std::initializer_list<int> _{ (BatchSpawnComponent(es, args),0)... };
			return es;
		}

		template<typename... Ts>
		std::pair<index_t, index_t> BatchInstantiateEntity(index_t n, index_t prototype)
		{
			std::pair<index_t, index_t> es = _entities.Raw().BatchSpawn(n);
			for (auto &e : _entityStates)
				if (e.Contain(prototype))
					e.BatchInstantiate(es.first, es.second, prototype);
			return es;
		}

		Entity GetEntity(index_t n)
		{
			return _entities.Raw().Get(n);
		}


	};
}


#pragma once
#include <any>
#include <unordered_map>
#include <functional>
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


#define ENTITY_STATE(name, container) \
namespace ESL \
{ \
	template<> \
	struct TState<name> { using type = EntityState<container<name>>; }; \
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
	

	template<template<typename> class V, typename T>
	struct TStateTrait<EntityState<V<T>>>
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

	template<template<typename> class V, typename T>
	struct TStateNonstric<EntityState<V<T>>>
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

	template<>
	struct TStateStrict<Entity>
	{
		using type = const GlobalState<Entities>;
	};

	template<typename T>
	using StateStrict = typename TStateStrict<T>::type;

	class States
	{
		std::unordered_map<std::size_t, std::any> _states;
		std::vector<std::function<void(Entity)>> _onEntityDie;
		GlobalState<ESL::Entities>& _entities;

		
		
	public:
		States(HBV::index_t size = 10u, bool spawn = false) : _entities(CreateState<ESL::Entities>(size, spawn)) {}

		void Tick()
		{
			_entities.Raw().MergeWith([this](Entity e)
			{
				for (auto &f : _onEntityDie)
					f(e);
				return true;
			});
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
		void RegisterEntityDie(T& any) {}

		template<typename T>
		void RegisterEntityDie(EntityState<T>& state)
		{
			_onEntityDie.emplace_back([&state](Entity e)
			{
				state.Remove(e);
			});
		}
	public:
		template<typename T, typename... Ts>
		auto &CreateState(Ts... args) noexcept
		{
			using ST = State<T>;
			auto &state = std::any_cast<ST&>(_states.insert({ typeid(ST).hash_code(), std::any{ ST{args...} } }).first->second);
			RegisterEntityDie(state);
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

	/*
	template<typename... Ts>
	class SubStates
	{
		using arg_types = MPL::map_t<State, MPL::typelist<Ts...>>; 
		template<typename T>
		struct Checker {
			static_assert(IsEntityState<T>{}, "must be entity state!");
		};
		using _ = MPL::map_t<Checker, arg_types>;
		template<typename T>
		using ref_t = T & ;
	public:
		using types = MPL::concat_t<MPL::typelist<const Entities>, arg_types>;


	private:
		using ref_types = MPL::map_t<ref_t, types>;
		using tuple_type = MPL::rewrap_t<std::tuple, ref_types>;
		tuple_type _refs;
	};
	*/
}


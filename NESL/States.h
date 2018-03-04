#pragma once
#include <any>
#include <unordered_map>
#include <functional>
#include "GlobalState.h"
#include "Entity.h"
#include "EntityState.h"

namespace ESL
{
	template<typename T>
	struct TState { using type = GlobalState<T>; };

	template<typename T>
	using State = typename TState<T>::type;

	template<>
	struct TState<Entity> { using type = GlobalState<GEntities>; };

	class States
	{
		std::unordered_map<std::size_t, std::any> _states;
		lni::vector<std::function<void(Entity)>> _onEntityDie;
		GlobalState<GEntities>& _entities;
		
	public:
		States() : _entities(Create<GEntities>()){}

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
		auto* Get() noexcept
		{
			if constexpr(std::is_same_v<GEntities, T>)
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
			_onEntityDie.push_back([&state](Entity e)
			{
				state.Remove(e);
			});
		}
	public:
		template<typename T, typename... Ts>
		auto &Create(Ts... args) noexcept
		{
			using ST = State<T>;
			auto &state = std::any_cast<ST&>(_states.insert({ typeid(ST).hash_code(), std::any{ ST{ args... } } }).first->second);
			RegisterEntityDie(state);
			return state;
		}
	};
}


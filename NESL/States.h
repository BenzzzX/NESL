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

	class States
	{
		std::unordered_map<std::size_t, std::any> _states;
		std::vector<std::function<void(Entity)>> _onEntityDie;
		
	public:
		States()
		{
			Create<GEntities>();
		}

		void Tick()
		{
			GEntities &entities = *Get<GEntities>();
			entities.MergeWith([this](Entity e)
			{
				for (auto &f : _onEntityDie)
					f(e);
				return true;
			});
		}

		template<typename T>
		auto* Get() noexcept
		{
			using ST = State<T>;
			auto it = _states.find(typeid(ST).hash_code());
			//for return type derivation
			auto ptr = &std::any_cast<ST&>(it->second).Raw();
			if (it != _states.end())
				return ptr;
			return (ptr = nullptr);
		}

		template<typename T>
		bool Valid() const noexcept
		{
			auto it = _states.find(typeid(T).hash_code());
			if (it == _states.end())
				return false;
			return true;
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
			auto &state = std::any_cast<ST&>(_states.insert({ typeid(ST).hash_code(), std::any{ ST{ args... } } }).first->second).Raw();
			RegisterEntityDie(state);
			return state;
		}
	};
}


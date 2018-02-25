#pragma once
#include "HBV.h"
#include "Entity.h"
#include <unordered_map>

namespace ESL
{
	template<typename T>
	class EntityState
	{
		HBV::bit_vector _entity;
		T _container;

	public:
		template<typename... Ts>
		EntityState(Ts... args) : _container(args...) {}

		HBV::bit_vector &Available()
		{
			return _entity;
		}

		EntityState &Raw()
		{
			return *this;
		}

		auto &Get(Entity e)
		{
			return _container.Get(e);
		}

		const auto &Get(Entity e) const
		{
			return _container.Get(e);
		}

		template<typename... Ts>
		auto &Create(Entity e, Ts&&... args)
		{
			if (_entity.size() <= e.id)
				_entity.grow(e.id + 1);
			_entity.set(e.id, true);
			return _container.Create(e, std::forward<Ts>(args)...);
		}

		bool Contain(Entity e)
		{
			return _entity.contain(e.id);
		}

		void Remove(Entity e)
		{
			_entity.set(e.id, false);
			_container.Remove(e);
		}
	};

	template<typename _Tp, typename _Alloc = std::allocator<_Tp>>
	class uvector : public std::vector<_Tp, _Alloc>
	{
		typedef std::vector<_Tp, _Alloc> parent;

	public:
		using parent::capacity;
		using parent::reserve;
		using parent::size;
		using typename parent::size_type;

		void resize(size_type sz)
		{
			if (sz <= size())
				parent::resize(sz);
			else
			{
				if (sz > capacity()) reserve(sz);
				parent::_Mylast() = parent::_Myfirst() + sz;
			}
		}
	};

	template<typename T>
	class Vec
	{
		uvector<T> _states;
	public:
		T &Get(Entity e)
		{
			return _states[e.id];
		}

		const T &Get(Entity e) const
		{
			return _states[e.id];
		}

		template<typename... Ts>
		T &Create(Entity e, Ts&&... args)
		{
			if (e.id >= _states.size())
				_states.resize(e.id + 1);
			
			return *(new(&_states[e.id]) T{ std::forward<Ts>(args)... });
		}

		void Remove(Entity e)
		{
			_states[e.id].~T();
		}
	};

	template<typename T>
	class Hash
	{
		std::unordered_map<index_t, T> _states;
	public:
		T &Get(Entity e)
		{
			return _states.at(e.id);
		}
		const T &Get(Entity e) const
		{
			return _states.at(e.id);
		}
		template<typename... Ts>
		T &Create(Entity e, Ts&&... args)
		{
			return _states.insert_or_assign(e.id, T{ std::forward<Ts>(args)... }).first->second;
		}

		void Remove(Entity e)
		{
			_states.erase(e.id);
		}
	};

	template<typename T>
	class DenseVec
	{
		uvector<T> _states;
		HBV::bit_vector _empty;
		std::vector<index_t> _redirector; 
		
		auto GetFree()
		{
			std::optional<index_t> id{};
			HBV::for_each<true>(_empty, [&id](index_t i)
			{
				id.emplace(i);
				return false;
			});
			return id;
		}
	public:
		DenseVec() {}
		T & Get(Entity e)
		{
			return _states[_redirector[e.id]];
		}

		const T &Get(Entity e) const
		{
			return _states[_redirector[e.id]];
		}

		template<typename... Ts>
		T &Create(Entity e, Ts&&... args)
		{
			auto free = GetFree();
			while (!free.has_value() || free.value() >= _states.size())
			{
				_states.resize(_states.size() + 10u);
				_empty.grow(_states.size(), true);
				free = GetFree();
			}
			_empty.set(free.value(), false);
			if (e.id >= _redirector.size())
				_redirector.resize(e.id + 1);
			_redirector[e.id] = free.value();
			return *(new(&_states[_redirector[e.id]]) T{ std::forward<Ts>(args)... });
		}

		void Remove(Entity e)
		{
			_empty.set(_redirector[e.id], true);
			_states[_redirector[e.id]].~T();
		}
	};
}

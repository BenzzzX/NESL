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

		template<typename T>
		struct value_type;

		template<template<typename> class V,typename T>
		struct value_type<V<T>> { using type = T; };

		using value_type_t = typename value_type<T>::type;
	public:
		EntityState(std::size_t sz = 10u) : _container(sz), _entity(sz) {}

		T& Raw()
		{
			return _container;
		}

		const T& Raw() const
		{
			return _container;
		}

		const HBV::bit_vector &Available() const
		{
			return _entity;
		}

		auto *Get(Entity e)
		{
			return Contain(e) ? &_container.Get(e) : nullptr;
		}

		const auto *Get(Entity e) const
		{
			return Contain(e) ? &_container.Get(e) : nullptr;
		}

		auto &Create(Entity e, const value_type_t& arg)
		{
			if (_entity.size() <= e.id)
				_entity.grow_to(e.id + 1);
			if (Contain(e))
				_container.Remove(e);
			else
				_entity.set(e.id, true);
			return _container.Create(e, arg);
		}

		bool Contain(Entity e) const
		{
			return _entity.contain(e.id);
		}

		void Remove(Entity e)
		{
			if (!Contain(e)) return;
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
				if (sz > capacity()) 
					reserve(sz + capacity());
				parent::_Mylast() = parent::_Myfirst() + sz;
			}
		}
	};

	template<typename T>
	class Vec
	{
		uvector<T> _states;
	public:

		Vec(std::size_t sz = 10u) 
		{
			_states.resize(sz);
		}
		T &Get(Entity e)
		{
			return _states[e.id];
		}

		const T &Get(Entity e) const
		{
			return _states[e.id];
		}

		T &Create(Entity e, const T& arg)
		{
			if (e.id >= _states.size())
			{
				_states.resize(e.id + 1u);
				return *(new(&_states[e.id]) T{ arg });
			}
			
			return *(new(&_states[e.id]) T{ arg });
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
		Hash(std::size_t sz = 10u)
		{
			_states.reserve(sz);
		}
		T &Get(Entity e)
		{
			return _states.at(e.id);
		}
		const T &Get(Entity e) const
		{
			return _states.at(e.id);
		}

		T &Create(Entity e, const T& arg)
		{
			return _states.insert_or_assign(e.id, T{ arg }).first->second;
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
		lni::vector<index_t> _redirector; 
		
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
		DenseVec(std::size_t sz = 10u) 
		{
			_states.resize(sz);
			_redirector.resize(sz);
			_empty.grow_to(sz, true);
		}

		T & Get(Entity e)
		{
			return _states[_redirector[e.id]];
		}

		const T &Get(Entity e) const
		{
			return _states[_redirector[e.id]];
		}

		T &Create(Entity e, const T& arg)
		{
			auto free = GetFree();
			while (!free.has_value() || free.value() >= _states.size())
			{
				_states.resize(_states.size() + 10u);
				_empty.grow_to(_states.size(), true);
				free = GetFree();
			}
			_empty.set(free.value(), false);
			if (e.id >= _redirector.size())
				_redirector.resize(e.id + 1);
			_redirector[e.id] = free.value();
			return *(new(&_states[_redirector[e.id]]) T{ arg });
		}

		void Remove(Entity e)
		{
			_empty.set(_redirector[e.id], true);
			_states[_redirector[e.id]].~T();
		}
	};
}

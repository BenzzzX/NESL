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
			return Contain(e) ? &_container.Get(e.id) : nullptr;
		}

		const auto *Get(Entity e) const
		{
			return Contain(e) ? &_container.Get(e.id) : nullptr;
		}

		template<typename T, typename = void>
		struct SupportBatch : std::false_type {};

		template<typename T>
		struct SupportBatch<T, std::void_t<decltype(&T::BatchCreate)>>
			: std::true_type {};

		void BatchCreate(index_t begin, index_t end, const value_type_t& arg)
		{
			if (_entity.size() <= end)
				_entity.grow_to(end);
			_entity.set_range(begin, end, true);
			if constexpr(SupportBatch<T>{})
			{
				_container.BatchCreate(begin, end, arg);
			}
			else
			{
				for (index_t i = begin; i < end; ++i)
					_container.Create(i, arg);
			}
		}

		auto &Create(Entity e, const value_type_t& arg)
		{
			if (_entity.size() <= e.id)
				_entity.grow_to(e.id + 1);
			if (Contain(e))
				_container.Remove(e.id);
			else
				_entity.set(e.id, true);
			return _container.Create(e.id, arg);
		}

		bool Contain(Entity e) const
		{
			return _entity.contain(e.id);
		}

		void Remove(Entity e)
		{
			if (!Contain(e)) return;
			_entity.set(e.id, false);
			_container.Remove(e.id);
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
		T &Get(index_t e)
		{
			return _states[e];
		}

		const T &Get(index_t e) const
		{
			return _states[e];
		}

		T &Create(index_t e, const T& arg)
		{
			if (e >= _states.size())
			{
				_states.resize(e + 1u);
				return *(new(&_states[e]) T{ arg });
			}
			
			return *(new(&_states[e]) T{ arg });
		}

		void Remove(index_t e)
		{
			_states[e].~T();
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
		T &Get(index_t e)
		{
			return _states.at(e);
		}
		const T &Get(index_t e) const
		{
			return _states.at(e);
		}

		T &Create(index_t e, const T& arg)
		{
			return _states.insert_or_assign(e, T{ arg }).first->second;
		}

		void Remove(index_t e)
		{
			_states.erase(e);
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

		T & Get(index_t e)
		{
			return _states[_redirector[e]];
		}

		const T &Get(index_t e) const
		{
			return _states[_redirector[e]];
		}

		T &Create(index_t e, const T& arg)
		{
			auto free = GetFree();
			while (!free.has_value() || free.value() >= _states.size())
			{
				_states.resize(_states.size() + 10u);
				_empty.grow_to(_states.size(), true);
				free = GetFree();
			}
			_empty.set(free.value(), false);
			if (e >= _redirector.size())
				_redirector.resize(e + 1);
			_redirector[e] = free.value();
			return *(new(&_states[_redirector[e]]) T{ arg });
		}

		void Remove(index_t e)
		{
			_empty.set(_redirector[e], true);
			_states[_redirector[e]].~T();
		}
	};
}

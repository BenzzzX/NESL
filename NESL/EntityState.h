#pragma once
#include "HBV.h"
#include "Entity.h"
#include <unordered_map>
#include "MPL.h"

namespace ESL
{
	template<typename T>
	class SparseVec;

	template<typename T>
	class Hash;

	template<typename T>
	class DenseVec;

	template<typename T>
	class Vec;

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

		template<template<typename> class container>
		static constexpr auto is_container_v = std::is_same_v<T, container<value_type_t>>;

		friend class States;

#define SupportFunctionTrait(name) \
		template<typename T, typename = void> \
		struct Support##name : std::false_type {}; \
		template<typename T> \
		struct Support##name<T, std::void_t<decltype(&T::name)>> \
			: std::true_type {}

		SupportFunctionTrait(BatchCreate);
#undef SupportFunctionTrait

		void BatchCreate(index_t begin, index_t end, const value_type_t& arg)
		{
			if (_entity.size() <= end)
				_entity.grow_to(end);
			_entity.set_range(begin, end, true);
			if constexpr(SupportBatchCreate<T>{})
			{
				_container.BatchCreate(begin, end, arg);
			}
			else
			{
				for (index_t i = begin; i < end; ++i)
					_container.Create(i, arg);
			}
		}

		void BatchRemove(const HBV::bit_vector& remove) 
		{ 
			if constexpr(is_container_v<Vec> && std::is_pod_v<value_type_t>) {}
			else if constexpr(is_container_v<SparseVec>)
			{
				if constexpr(!std::is_pod_v<value_type_t>)
				{
					HBV::for_each(HBV::compose(HBV::and_op, remove, _entity), [this](index_t i)
					{
						_container.SimpleRemove(i);
					});
				}
				_container.BatchRemove(HBV::compose(HBV::and_op, remove, _entity));
			}
			else
			{
				HBV::for_each(HBV::compose(HBV::and_op, remove, _entity), [this](index_t i)
				{
					_container.Remove(i);
				});
			}
			_entity.merge<true>(remove);
		}
		
	public:

		EntityState() : EntityState(MPL::type_t<T>{}) {}

		template<typename T>
		EntityState(MPL::type_t<T>) : _container(10u), _entity(10u) {}

		EntityState(MPL::type_t<SparseVec<value_type_t>>) : _entity(10u), _container(_entity) {}


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

		auto &Get(index_t e)
		{
			assert(Contain(e));
			return _container.Get(e);
		}

		const auto &Get(index_t e) const
		{
			assert(Contain(e));
			return _container.Get(e);
		}

		auto &Create(index_t e, const value_type_t& arg)
		{
			if (_entity.size() <= e)
				_entity.grow_to(e + 1);
			if (Contain(e))
				_container.Remove(e);
			else
				_entity.set(e, true);
			return _container.Create(e, arg);
		}

		bool Contain(index_t e) const
		{
			return _entity.contain(e);
		}

		void Remove(index_t e)
		{
			if (!Contain(e)) return;
			_entity.set(e, false);
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
		T &Get(index_t e)
		{
			return _states[e];
		}

		const T &Get(index_t e) const
		{
			return _states[e];
		}

		void BatchCreate(index_t begin, index_t end, const T& arg)
		{
			_states.resize(end);
			for (auto i = begin; i < end; ++i)
				new(&_states[i]) T{ arg };
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
			if constexpr(!std::is_pod_v<T>)
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
	class SparseVec
	{
		const HBV::bit_vector& _entity;
		lni::vector<T*> _states;
		static constexpr index_t Level = 2;
		static constexpr index_t BucketSize = 1 << ((HBV::LayerCount - Level)*HBV::BitsPerLayer);

	public:
		SparseVec(const HBV::bit_vector& entities) : _entity(entities), _states(10u, nullptr) {}

		T & Get(index_t e)
		{
			index_t bucket = e / BucketSize;
			index_t index = e % BucketSize;
			return _states[bucket][index];
		}

		const T &Get(index_t e) const
		{
			index_t bucket = e / BucketSize;
			index_t index = e % BucketSize;
			return _states[bucket][index];
		}

		T &Create(index_t e, const T& arg)
		{
			index_t bucket = e / BucketSize;
			if (_states.size() <= bucket)
				_states.resize(bucket + _states.size(), nullptr);
			if (_states[bucket] == nullptr)
				_states[bucket] = (T*)malloc(sizeof(T)*BucketSize);
			index_t index = e % BucketSize;
			new (_states[bucket] + index) T{ arg };
		}

		void BatchCreate(index_t begin, index_t end, const T& arg)
		{
			index_t first = begin / BucketSize;
			index_t last = (end-1) / BucketSize;
			if (_states.size() <= last)
				_states.resize(last + _states.size(), nullptr);
			for (index_t i = first; i <= last; ++i)
				if (_entity.layer(Level, i) && !_states[i])
					_states[i] = (T*)malloc(sizeof(T)*BucketSize);
			if constexpr(std::is_pod_v<T>)
			{
				for (index_t i = first + 1; i < last; ++i)
					std::fill_n(_states[i], BucketSize, arg);
				index_t firstIndex = begin % BucketSize;
				std::fill_n(_states[first] + firstIndex, BucketSize - firstIndex, arg);
				std::fill_n(_states[last], (end - 1) % BucketSize + 1, arg);
			}
			else
			{
				for (index_t i = begin; i < end; ++i)
				{
					index_t bucket = e / BucketSize;
					index_t index = e % BucketSize;
					new (_states[bucket] + index) T{ arg };
				}
			}
		}

		void Remove(index_t e)
		{
			index_t bucket = e / BucketSize;
			if constexpr(!std::is_pod_v<T>)
			{
				index_t index = e % BucketSize;
				_states[bucket][index].~T();
			}
			if (!_entity.layer(Level, bucket) && _states[bucket])
				free(_states[bucket]);
		}

		void SimpleRemove(index_t e)
		{
			index_t bucket = e / BucketSize;
			if constexpr(!std::is_pod_v<T>)
			{
				index_t index = e % BucketSize;
				_states[bucket][index].~T();
			}
		}

		using type = decltype(HBV::compose(HBV::and_op, _entity, _entity));
		void BatchRemove(const type& remove)
		{
			HBV::for_each<Level - 1>(remove, [this](index_t i)
			{
				if (_entity.layer(Level, i) && _states[i])
					free(_states[i]);
			});
		}
	};


	template<typename T>
	class DenseVec
	{
		uvector<T> _states;
		HBV::bit_vector _empty;
		lni::vector<int32_t> _redirector; 
		
		auto GetFree()
		{
			std::optional<index_t> id{};
			if(!_empty.empty())
				id = HBV::first(_empty);
			return id;
		}
	public:
		DenseVec(std::size_t sz = 10u) 
		{
			_states.resize(sz);
			_redirector.resize(sz, -1);
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

		void BatchCreate(index_t begin, index_t end, const T& arg)
		{
			if (end > _redirector.size())
				_redirector.resize(end, -1);
			index_t n = end - begin;
			index_t start;
			index_t last = _empty.last();
			HBV::flag_t fullValue = HBV::value_of<3>(last) -1 + HBV::value_of<3>(last);
			int32_t i = HBV::index_of<3>(last);
			HBV::flag_t value = _empty.layer3(i);
			if (value != fullValue)
			{
				value = (~value)&fullValue;
				start = (i << HBV::BitsPerLayer) + HBV::highbit_pos(value) + 1;
			}
			else
			{
				while (--i && (value = _empty.layer3(i)) == HBV::FullNode);
				start = (i << HBV::BitsPerLayer) + HBV::highbit_pos(~value) + 1;
			}
			if (start + n > _states.size())
			{
				_states.resize(start + n);
				_empty.grow_to(start + n);
			}
			_empty.set_range(start, start + n, false);
			for (i = 0; i < n; ++i)
			{
				_redirector[begin + i] = start + i;
				new(&_states[_redirector[begin + i]]) T{ arg };
			}
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
				_redirector.resize(e + _redirector.size() / 2);
			_redirector[e] = free.value();
			return *(new(&_states[_redirector[e]]) T{ arg });
		}

		void Remove(index_t e)
		{
			if constexpr(!std::is_pod_v<T>)
				_states[_redirector[e]].~T();
			_empty.set(_redirector[e], true);
		}
	};
}

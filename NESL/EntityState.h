#pragma once
#include "HBV.h"
#include "Entity.h"
#include <unordered_map>
#include "MPL.h"
#include "Trace.h"

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
	class Flag;

	template<typename T, typename = void>
	struct SupportBatchCreate : std::false_type {};
	template<typename T>
	struct SupportBatchCreate<T, std::void_t<decltype(&T::BatchCreate)>>
		: std::true_type {};

	template<typename T, typename = void>
	struct SupportBatchRemove : std::false_type {};
	template<typename T>
	struct SupportBatchRemove<T, std::void_t<decltype(&T::BatchRemove)>>
		: std::true_type {};

	using bit_vector_and2 = decltype(HBV::compose(HBV::and_op, HBV::bit_vector{}, HBV::bit_vector{}));

	template<typename T, Trace... types>
	class EntityState
	{
		HBV::bit_vector _entity;
		T _container;

		std::tuple<Tracer<types>...> _tracers;

		template<typename T>
		struct value_type;

		template<template<typename> class V,typename T>
		struct value_type<V<T>> { using type = T; };

		using value_type_t = typename value_type<T>::type;

		friend class States;

		void BatchCreate(index_t begin, index_t end, const value_type_t& arg)
		{
			MPL::for_tuple(_tracers, [begin, end](auto& tracer)
			{
				tracer.BatchCreate(begin, end);
			});
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
			auto vector = HBV::compose(HBV::and_op, remove, _entity);
			MPL::for_tuple(_tracers, [&vector](auto& tracer)
			{
				tracer.BatchRemove(vector);
			});
			if constexpr(SupportBatchCreate<T>{})
			{
				_container.BatchRemove(vector);
			}
			else
			{
				HBV::for_each(vector, [this](index_t i)
				{
					_container.Remove(i);
				});
			}
			_entity.merge<true>(remove);
		}
		
		template<typename T>
		EntityState(MPL::type_t<T>) : _container(10u), _entity(10u) {}

		template<typename T>
		EntityState(MPL::type_t<SparseVec<T>>) : _entity(10u), _container(_entity) {}

	public:

		EntityState() : EntityState(MPL::type_t<T>{}) {}

		T& Raw()
		{
			return _container;
		}

		const T& Raw() const
		{
			return _container;
		}

		template<Trace type>
		decltype(auto) Available() const
		{
			return ComposeTracer<type, types...>(_tracers, _entity);
		}

		template<Trace type>
		void ResetTracer()
		{
			
			std::get<Tracer<type>>(_tracers).Reset();
		}

		void ResetTracers()
		{
			MPL::for_tuple(_tracers, [](auto& tracer)
			{
				tracer.Reset();
			});
		}

		auto &Get(index_t e)
		{
			MPL::for_tuple(_tracers, [&e](auto& tracer)
			{
				tracer.Change(e);
			});
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
			MPL::for_tuple(_tracers, [&e](auto& tracer)
			{
				tracer.Create(e);
			});
			if (_entity.size() <= e)
				_entity.grow_to(e + 64*64);
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
			assert(Contain(e));
			MPL::for_tuple(_tracers, [](auto& tracer)
			{
				tracer.Remove(e);
			});
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
	class Placeholder
	{ 
	public:
		T &Get(index_t e)
		{
			return *(T*)nullptr;
		}

		const T &Get(index_t e) const
		{
			return *(T*)nullptr;
		}

		void BatchCreate(index_t begin, index_t end, const T& arg)
		{
		}

		T &Create(index_t e, const T& arg)
		{
			return *(T*)nullptr;
		}

		void BatchRemove(const bit_vector_and2& remove)
		{
		}

		void Remove(index_t e)
		{
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

		void BatchRemove(const bit_vector_and2& remove)
		{
			if constexpr(!std::is_pod_v<T>)
			{
				HBV::for_each(remove, [this](index_t i)
				{
					_states[i].~T();
				});
			}
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

		
		void BatchRemove(const bit_vector_and2& remove)
		{
			if constexpr(!std::is_pod_v<T>)
			{
				HBV::for_each(remove, [this](index_t i)
				{
					index_t bucket = e / BucketSize;
					index_t index = e % BucketSize;
					_states[bucket][index].~T();
				});
			}

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

#pragma once
#include "HBV.h"
#include "Entity.h"
#include <unordered_map>
#include "MPL.h"
#include "Trace.h"

namespace ESL
{
	template<typename T>
	using SupportBatchCreate = decltype(&T::BatchCreate);

	template<typename T>
	using SupportBatchRemove = decltype(&T::BatchRemove);

	template<typename T>
	using SupportInstantiate = decltype(&T::Instantiate);

	template<typename>
	class SparseVec;

	template<typename>
	class Hash;

	template<typename>
	class DenseVec;

	template<typename>
	class SharedVec;

	template<typename>
	class UniqueVec;

	template<typename>
	class Vec;

	template<typename>
	class Placeholder;

	using bit_vector_and2 = decltype(HBV::compose(HBV::and_op, HBV::bit_vector{}, HBV::bit_vector{}));

	struct EntityStateBase
	{
		virtual bool Contain(index_t e) const = 0;
		virtual void ResetTracers() = 0;
		virtual void Remove(index_t e) = 0;
		virtual void Instantiate(index_t e, index_t proto) = 0;
	protected:
		friend class States;
		virtual void BatchInstantiate(index_t begin, index_t end, index_t proto) = 0;
		virtual void BatchRemove(const HBV::bit_vector& remove) = 0;
	};

	template<typename T, Trace... types>
	class EntityState : public EntityStateBase
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

		void BatchCreate(index_t begin, index_t end, const value_type_t& arg) noexcept
		{
			MPL::for_tuple(_tracers, [begin, end](auto& tracer)
			{
				tracer.BatchCreate(begin, end);
			});
			if (_entity.size() <= end)
				_entity.grow_to(end);
			_entity.set_range(begin, end, true);
			if constexpr(MPL::is_detected<SupportBatchCreate, T>{})
			{
				_container.BatchCreate(begin, end, arg);
			}
			else
			{
				for (index_t i = begin; i < end; ++i)
					_container.Create(i, arg);
			}
		}

		void BatchInstantiate(index_t begin, index_t end, index_t proto) noexcept
		{
			const value_type_t& prototype = Get(proto);
			BatchCreate(begin, end, prototype);
		}

		//注意remove并不一定存在于容器中,需要一次compose
		void BatchRemove(const HBV::bit_vector& remove) noexcept
		{ 
			auto vector = HBV::compose(HBV::and_op, remove, _entity);
			MPL::for_tuple(_tracers, [&vector](auto& tracer)
			{
				tracer.BatchRemove(vector);
			});
			if constexpr(MPL::is_detected<SupportBatchRemove, T>{})
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
		EntityState(MPL::type_t<T>) noexcept : _container(10u), _entity(10u) {}

		//Hack!SparseVec复用了_entity
		template<typename T>
		EntityState(MPL::type_t<SparseVec<T>>) noexcept : _entity(10u), _container(_entity) {}

		//Hack!DenseVec复用了SparseVec
		template<typename T>
		EntityState(MPL::type_t<DenseVec<T>>) noexcept : _entity(10u), _container(_entity) {}

		//Hack!SharedVec复用了SparseVec
		template<typename T>
		EntityState(MPL::type_t<SharedVec<T>>) noexcept : _entity(10u), _container(_entity) {}

		//Hack!SharedVec复用了SparseVec
		template<typename T>
		EntityState(MPL::type_t<UniqueVec<T>>) noexcept : _entity(10u), _container(_entity) {}
	public:

		EntityState() noexcept : EntityState(MPL::type_t<T>{}) {}

		template<typename = std::enable_if_t<std::is_same_v<T, UniqueVec<value_type_t>>>>
		index_t UniqueSize() const
		{
			return _container.UniqueSize();
		}

		template<typename = std::enable_if_t<std::is_same_v<T, UniqueVec<value_type_t>>>>
		const auto& SetUnique(int32_t i)
		{
			_container.FilterId = i;
			//Hack!在filter状态下,返回和id无关,所以可以随便传一个数
			return _container.Get(0);
		}

		T& Raw() noexcept
		{
			return _container;
		}

		const T& Raw() const noexcept
		{
			return _container;
		}

		template<Trace type>
		decltype(auto) Available() const noexcept
		{
			if constexpr(std::is_same_v<T, UniqueVec<value_type_t>>)
			{
				static_assert(!(type & Borrow), "Borrow is not supported with UniqueVec!");
				if (_container.FilterId >= 0)
				{
					if constexpr(type == Trace::Has)
						return _container.Available();
					else 
						return HBV::compose(HBV::and_op, _container.Available(), ComposeTracer<type, types...>(_tracers, _entity));
				}
				else
				{
					return ComposeTracer<type, types...>(_tracers, _entity);
				}
			}
			else
			{
				return ComposeTracer<type, types...>(_tracers, _entity);
			}
			
		}

		template<Trace type>
		void ResetTracer()
		{
			
			std::get<Tracer<type>>(_tracers).Reset();
		}

		void ResetTracers() noexcept
		{
			MPL::for_tuple(_tracers, [](auto& tracer)
			{
				tracer.Reset();
			});
		}

		auto &Get(index_t e) noexcept
		{
			MPL::for_tuple(_tracers, [&e](auto& tracer)
			{
				tracer.Change(e);
			});
			assert(Contain(e));
			return _container.Get(e);
		}

		const auto &Get(index_t e) const noexcept
		{
			assert(Contain(e));
			return _container.Get(e);
		}

		decltype(auto) Create(index_t e, const value_type_t& arg)  noexcept
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

		void Instantiate(index_t e, index_t proto) noexcept
		{
			MPL::for_tuple(_tracers, [&e](auto& tracer)
			{
				tracer.Create(e);
			});
			//鬼畜的VS只有这样才能编译过
			if constexpr(MPL::is_detected<SupportInstantiate, T>{})
			{
				_container.Instantiate(e, proto);
				_entity.set(e, true);
			}
			else
				_container.Create(e, _container.Get(proto));
		}

		bool Contain(index_t e) const noexcept
		{
			return _entity.contain(e);
		}

		void Remove(index_t e) noexcept
		{
			assert(Contain(e));
			MPL::for_tuple(_tracers, [e](auto& tracer)
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
		static constexpr index_t Level = 2u;
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
			return *(new (_states[bucket] + index) T{ arg });
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
		SparseVec<index_t> _redirector;
		auto GetFree()
		{
			std::optional<index_t> id{};
			if (!_empty.empty())
				id = HBV::last(_empty);
			return id;
		}
	public:
		DenseVec(const HBV::bit_vector& entities)
			: _redirector(entities), _empty(10u, true) 
		{
			_states.resize(10u);
		}

		T &Get(index_t e)
		{
			return _states[_redirector.Get(e)];
		}

		const T &Get(index_t e) const
		{
			return _states[_redirector.Get(e)];
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
			_redirector.Create(e, free.value());
			return *(new(&_states[free.value()]) T{ arg });
		}

		void Remove(index_t e)
		{
			if constexpr(!std::is_pod_v<T>)
				_states[_redirector.Get(e)].~T();
			_empty.set(_redirector.Get(e), true);
			_redirector.Remove(e);
		}
	};

	template<typename T>
	class UniqueVec
	{
		struct Unique
		{
			T* state{nullptr};
			index_t refCount;
			HBV::bit_vector entities{10u};
		};
		lni::vector<Unique> _states;
		SparseVec<index_t> _redirector;
		
	public:
		UniqueVec(const HBV::bit_vector& entities)
			: _redirector(entities), FilterId(-1) {}
		const T &Get(index_t e) const
		{
			if(FilterId>0)
				return *(_states[FilterId].state);
			else
				return *(_states[_redirector.Get(e)].state);
		}

		index_t UniqueSize() const
		{
			return _states.size();
		}

		int32_t FilterId;

		const HBV::bit_vector &Available() const
		{
			return _states[FilterId].entities;
		}

		void BatchCreate(index_t begin, index_t end, const T& arg)
		{
			Create(begin, arg);
			index_t prototype = _redirector.Get(begin);
			_states[prototype].refCount += end - begin;
			_redirector.BatchCreate(begin + 1, end, prototype);
		}

		void Instantiate(index_t e, index_t proto)
		{
			index_t prototype = _redirector.Get(proto);
			_states[prototype].refCount++;
			_redirector.Create(e, prototype);
		}

		//Hack!强行取得了arg的所有权
		const T &Create(index_t e, const T& arg)
		{
			for (index_t i = 0; i < _states.size(); ++i)
				if (&arg == _states[i].state)
				{
					_states[i].refCount++;
					auto& entities = _states[i].entities;
					if (entities.size() < e)
						entities.grow_to(e * 3 / 2);
					entities.set(e, true);
					_redirector.Create(e, i);
					return arg;
				}
			for (index_t i = 0; i < _states.size(); ++i)
				if (nullptr == _states[i].state)
				{
					auto& unique = _states[i];
					unique.refCount = 1;
					unique.state = const_cast<T*>(&arg);
					unique.entities.set(e, true);
					return arg;
				}
			_states.emplace_back();
			auto& unique = _states.back();
			unique.refCount = 1;
			unique.state = const_cast<T*>(&arg);
			unique.entities.set(e, true);
			return arg;
		}

		void Remove(index_t e)
		{
			auto& unique = _states[_redirector.Get(e)];
			if (--unique.refCount == 0)
			{
				unique.entities.clear();
				delete unique.state;
				unique.state = nullptr;
				_redirector.Remove(e);
			}
			else
			{
				unique.entities.set(e, false);
				_redirector.Remove(e);
			}
		}
	};

	template<typename T>
	class SharedVec
	{
		uvector<T> _states;
		HBV::bit_vector _empty;
		uvector<index_t> _refs;
		SparseVec<index_t> _redirector;
		auto GetFree()
		{
			std::optional<index_t> id{};
			if(!_empty.empty())
				id = HBV::first(_empty);
			return id;
		}

		index_t CreateFree(index_t e)
		{
			auto free = GetFree();
			while (!free.has_value() || free.value() >= _states.size())
			{
				_states.resize(_states.size() + 10u);
				_refs.resize(_states.size() + 10u);
				_empty.grow_to(_states.size(), true);
				free = GetFree();
			}
			_empty.set(free.value(), false);
			_refs[free.value()] = 1;
			_redirector.Create(e, free.value());
			return free.value();
		}

	public:
		SharedVec(const HBV::bit_vector& entities)
			: _redirector(entities), _empty(10u, true)
		{
			_states.resize(10u);
			_refs.resize(10u);
		}

		T & Get(index_t e)
		{
			index_t id = _redirector.Get(e);
			if (_refs[id] > 1)
			{
				_refs[id]--;
				return *(new(&_states[CreateFree(e)]) T{ _states[id] });
			}
			return _states[id];
		}

		const T &Get(index_t e) const
		{
			return _states[_redirector.Get(e)];
		}

		void BatchCreate(index_t begin, index_t end, const T& arg)
		{
			Create(begin, arg);
			index_t prototype = _redirector.Get(begin);
			_refs[prototype] = end - begin;
			_redirector.BatchCreate(begin + 1, end, prototype);
		}

		void Instantiate(index_t e, index_t proto)
		{
			index_t prototype = _redirector.Get(proto);
			_refs[prototype]++;
			_redirector.Create(e, prototype);
		}

		T &Create(index_t e, const T& arg)
		{
			return *(new(&_states[CreateFree(e)]) T{ arg });
		}

		void Remove(index_t e)
		{
			if constexpr(!std::is_pod_v<T>)
				_states[_redirector.Get(e)].~T();
			_refs[_redirector.Get(e)]--;
			_empty.set(_redirector.Get(e), true);
			_redirector.Remove(e);
		}
	};
}

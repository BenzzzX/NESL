#pragma once
#include "vector.h"
#include <optional>
#include "HBV.h"

namespace ESL
{
	using Generation = uint32_t;
	using index_t = uint32_t;

	struct Entity
	{
		index_t id;
		Generation generation;
	};

	class Entities
	{
		lni::vector<Generation> _generation;
		HBV::bit_vector _dead;
		HBV::bit_vector _alive;
		HBV::bit_vector _killed;

		void Grow(index_t to)
		{
			_dead.grow(to, true);
			_generation.resize(to, 0u);
			_killed.grow(to);
			_alive.grow(to);
		}

		auto GetFree()
		{
			std::optional<index_t> id{};
			HBV::for_each<true>(_dead, [&id](index_t i)
			{
				id.emplace(i);
				return false;
			});
			return id;
		}
	public:
		Entities() : _generation(10000u), _dead(10000u, true), _killed(10000u), _alive(10000u) {}

		void Grow()
		{
			Grow(_generation.size() * 2u);
		}

		const HBV::bit_vector& Available() const
		{
			return _alive;
		}

		Entity Get(index_t i) const
		{
			return { i, _generation[i] };
		}

		std::optional<Entity> TrySpawn()
		{
			auto id = GetFree();
			if (!id.has_value()) return{};
			Generation &g = _generation[id.value()];
			_dead.set(id.value(), false);
			_alive.set(id.value(), true);
			return Entity{ id.value(), g+1 };
		}

		bool Alive(Entity e) const
		{
			return _generation[e.id] == e.generation
				&& _alive.contain(e.id);
		}

		template<typename F>
		void MergeWith(F&& f)
		{
			HBV::for_each(_killed, [this, &f](index_t i)
			{
				bool kill = f(Entity{ i, _generation[i] });
				_dead.set(i, kill);
				_alive.set(i, !kill);
			});
			_killed.clear();
		}

		void Merge()
		{
			HBV::for_each(_killed, [this](index_t i)
			{
				_dead.set(i, true);
				_alive.set(i, false);
			});
			_killed.clear();
		}

		void Kill(Entity e) const
		{
			Entities* mutableThis = const_cast<Entities*>(this);
			mutableThis->_killed.set(e.id, true);
		}
	};

}
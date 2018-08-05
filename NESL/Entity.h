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

		

		std::optional<index_t> GetFree()
		{
			if (_dead.empty()) return {};

			return first(_dead);
		}

		friend class States;
	public:
		Entities() : _generation(10u), _dead(10u, true), _killed(10u), _alive(10u, false) {}

		void Grow(index_t to)
		{
			_dead.grow_to(to, true);
			_generation.resize(to, 0u);
			_killed.grow_to(to);
			_alive.grow_to(to);
		}

		void Grow()
		{
			Grow(_generation.size() * 2u);
		}

		void TryGrow()
		{
			if (!GetFree().has_value())
			{
				Grow();
			}
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
			return Entity{ id.value(), g + 1 };
		}

		Entity ForceSpawn()
		{
			auto id = GetFree();
			if (!id.has_value())
			{
				Grow();
				id = GetFree();
			}
			Generation &g = _generation[id.value()];
			_dead.set(id.value(), false);
			_alive.set(id.value(), true);
			return Entity{ id.value(), g + 1 };
		}

		std::pair<index_t, index_t> BatchSpawn(index_t n)
		{
			index_t end = last(_alive) + 1;
			if(end + n > _generation.size())
				Grow(end + n);
			_alive.set_range(end, end + n, true);
			_killed.set_range(end, end + n, false);
			for (index_t i = end; i < end + n; ++i)
				++_generation[i];
			return { end ,end + n };
		}

		bool Alive(Entity e) const
		{
			return _generation[e.id] == e.generation
				&& _alive.contain(e.id);
		}

		void DoKill()
		{
			_dead.merge(_killed);
			_alive.merge<true>(_killed);
			_killed.clear();
		}

		void Kill(Entity e) const
		{
			Entities* mutableThis = const_cast<Entities*>(this);
			mutableThis->_killed.set(e.id, true);
		}
	};

}
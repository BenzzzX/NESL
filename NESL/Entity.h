#pragma once
#include <vector>
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

	class GEntities
	{
		std::vector<Generation> _generation;
		HBV::bit_vector _alive;
		HBV::bit_vector _rised;
		HBV::bit_vector _killed;

		void Grow(index_t to)
		{
			_alive.grow(to);
			_generation.resize(to, 0u);
			_rised.grow(to);
		}

		auto GetFree()
		{
			std::optional<index_t> id{};
			auto _notdead = HBV::compose(HBV::or_op, _alive, _rised);
			auto _dead = HBV::compose(HBV::not_op, _notdead);
			HBV::for_each<true>(_dead, [&id](index_t i)
			{
				id.emplace(i);
				return false;
			});
			return id;
		}
	public:
		GEntities() : _generation(10000u), _alive(10000u), _rised(10000u), _killed(10000u)
		{

		}

		void Grow()
		{
			Grow(_generation.size() * 2u);
		}

		std::optional<Entity> TrySpawn()
		{
			auto id = GetFree();
			if (!id.has_value()) return{};
			Generation &g = _generation[id.value()];
			_rised.set(id.value(), true);
			return Entity{ id.value(), g+1 };
		}

		bool Alive(Entity e)
		{
			return _generation[e.id] == e.generation
				&& _alive.contain(e.id);
		}

		void Merge()
		{
			HBV::for_each(_rised, [this](index_t i)
			{
				_alive.set(i, true);
				Generation &g = _generation[i];
				g += 1;
			});

			HBV::for_each(_killed, [this](index_t i)
			{
				_alive.set(i, false);
			});

			_rised.clear();
			_killed.clear();
		}

		void Kill(Entity e)
		{
			_killed.set(e.id, true);
		}
	};
}
#pragma once
#include "vector.h"
#include <optional>
#include "HBV.h"

namespace ESL
{
	//设置为无符号数,溢出自动循环
	using Generation = uint8_t;
	using index_t = uint32_t;

	struct Entity
	{
		//实际上bit_vector支持的最大ID就是1<<24
		//这里刚好可以把Entity打包到32bit
		index_t id : 24;
		Generation generation : 8;

		operator index_t() const
		{
			return id;
		}
	};

	class Entities
	{
		//bit_vector并不记录有效位的数量,这里手动记录
		index_t _freeCount;
		index_t _killedCount;
		lni::vector<Generation> _generation;
		HBV::bit_vector _dead;
		HBV::bit_vector _alive;
		HBV::bit_vector _killed;

		std::optional<index_t> GetFree()
		{
			if (_dead.empty()) return {};

			return first(_dead);
		}

		Entity ForceSpawn()
		{
			auto id = GetFree();
			if (!id.has_value())
			{
				Grow();
				id = GetFree();
			}
			--_freeCount;
			Generation &g = _generation[id.value()];
			_dead.set(id.value(), false);
			_alive.set(id.value(), true);
			return Entity{ id.value(), g += 1 };
		}

		std::pair<index_t, index_t> BatchSpawn(index_t n)
		{
			index_t end = last(_alive) + 1;
			if (end + n > _generation.size())
				GrowTo(end + n);
			_freeCount-=n;
			_alive.set_range(end, end + n, true);
			for (index_t i = end; i < end + n; ++i)
				++_generation[i];
			return { end ,end + n };
		}

		void DoKill()
		{
			_freeCount += _killedCount;
			_killedCount = 0;
			_dead.merge(_killed);
			_alive.merge<true>(_killed);
			_killed.clear();
		}

		void GrowTo(index_t to)
		{
			_freeCount += to - _generation.size();
			_dead.grow_to(to, true);
			_generation.resize(to, 0u);
			_killed.grow_to(to);
			_alive.grow_to(to);
		}

		void Grow(index_t base = 50u)
		{
			GrowTo(_generation.size() * 3u / 2u + base);
		}

		friend class States;
	public:
		Entities() : _generation(10u), _dead(10u, true), _killed(10u), _alive(10u, false), _freeCount(10u), _killedCount(0u) {}

		const HBV::bit_vector& Available() const
		{
			return _alive;
		}

		Entity Get(index_t i) const
		{
			return { i, _generation[i] };
		}

		//Hack!在大多数情况下,Spawn不会与Get冲突
		//所以为了减少Block,Entities只开放一个保守的Spawn
		std::optional<Entity> TrySpawn()
		{
			auto id = GetFree();
			if (!id.has_value()) return{};
			--_freeCount;
			Generation &g = _generation[id.value()];
			_dead.set(id.value(), false);
			_alive.set(id.value(), true);
			return Entity{ id.value(), g += 1 };
		}

		bool Alive(Entity e) const
		{
			return _generation[e.id] == e.generation
				&& _alive.contain(e.id);
		}


		void Kill(Entity e)
		{
			_killedCount++;
			_killed.set(e.id, true);
		}
	};

}
#pragma once
#include <type_traits>
#include <vector>
#include <tuple>
#include <array>
namespace HBV
{
	using index_t = uint32_t;

	constexpr index_t BitsPerLayer = 5u;
	constexpr index_t LayerCount = 4u;

	template<index_t layer>
	constexpr index_t index_of(index_t id) noexcept
	{
		return id >> ((LayerCount - layer)*BitsPerLayer);
	}

	template<index_t layer>
	constexpr index_t value_of(index_t id) noexcept
	{
		return 1u << ((id >> ((LayerCount - layer - 1)*BitsPerLayer)) & ((1 << BitsPerLayer) - 1));
	}

	constexpr index_t EmptyNode = 0u;

	class bit_vector
	{
		index_t _layer0;
		std::vector<index_t> _layer1;
		std::vector<index_t> _layer2;
		std::vector<index_t> _layer3;
	public:
		bit_vector(index_t max)
		{
			grow(max);
		}

		void grow(index_t to)
		{
			_layer3.resize(index_of<3>(to) + 1, 0u);
			_layer2.resize(index_of<2>(to) + 1, 0u);
			_layer1.resize(index_of<1>(to) + 1, 0u);
		}

		index_t layer0() const noexcept
		{
			return _layer0;
		}

		index_t layer1(index_t id) const noexcept
		{
			return _layer1[id];
		}

		index_t layer2(index_t id) const noexcept
		{
			return _layer2[id];
		}

		index_t layer3(index_t id) const noexcept
		{
			return _layer3[id];
		}

		bool set(index_t id, bool value) noexcept
		{
			index_t index_3 = index_of<3>(id);
			index_t value_3 = value_of<3>(id);

			if (value)
			{
				if (_layer3[index_3] & value_3) return false;
				if (_layer3[index_3] == EmptyNode)
				{
					_layer2[index_of<2>(id)] |= value_of<2>(id);
					_layer1[index_of<1>(id)] |= value_of<1>(id);
					_layer0 |= value_of<0>(id);
				}
				_layer3[index_3] |= value_3;
				return true;
			}
			else
			{
				if (index_3 >= _layer3.size()) return false;
				_layer3[index_3] &= ~value_3;
				if (_layer3[index_3] != EmptyNode) return true;

				index_t index_2 = index_of<2>(id);
				_layer2[index_2] &= ~value_of<2>(id);
				if (_layer2[index_2] != EmptyNode) return true;

				index_t index_1 = index_of<1>(id);
				_layer1[index_1] &= ~value_of<1>(id);
				if (_layer1[index_1] != EmptyNode) return true;


				_layer0 &= ~value_of<0>(id);
				return true;
			}
		}

		void clear() noexcept
		{
			_layer3.clear();
			_layer2.clear();
			_layer1.clear();
			_layer0 = 0u;
		}

		bool contain(index_t id) const noexcept
		{
			index_t index_3 = index_of<3>(id);
			return (index_3 < _layer3.size()) && (_layer3[index_3] & value_of<3>(id));
		}

		index_t layer(index_t level, index_t id) const noexcept
		{
			switch (level)
			{
			case 0: 
				return layer0();
			case 1: 
				return layer1(id);
			case 2: 
				return layer2(id);
			case 3: 
				return layer3(id);
			default:
				return 0;
			}
		}
	};

	template<typename F,typename... Ts>
	class bit_vector_composer
	{
		const std::tuple<const Ts&...> _nodes;
		F op;
	public:
		bit_vector_composer(F&& f, const Ts&... args) : op(std::forward<F>(f)), _nodes(args...) {}
		template<index_t... i>
		index_t compose_layer0(std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer0()...);
		}

		index_t layer0() const noexcept
		{
			return compose_layer0(std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_layer1(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer1(id)...);
		}

		index_t layer1(index_t id) const noexcept
		{
			return compose_layer1(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_layer2(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer2(id)...);
		}

		index_t layer2(index_t id) const noexcept
		{
			return compose_layer2(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_layer3(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer3(id)...);
		}

		index_t layer3(index_t id) const noexcept
		{
			return compose_layer3(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_contain(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).contain(id)...);
		}

		index_t contain(index_t id) const noexcept
		{
			return compose_contain(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		index_t layer(index_t level, index_t id) const noexcept
		{
			switch (level)
			{
			case 0:
				return layer0();
			case 1:
				return layer1(id);
			case 2:
				return layer2(id);
			case 3:
				return layer3(id);
			default:
				return 0;
			}
		}
	};

	
	struct and_op_t
	{
		template<typename... Ts>
		index_t operator()(Ts... args)
		{
			return (args & ...);
		}
	};

	struct or_op_t
	{
		template<typename... Ts>
		index_t operator()(Ts... args)
		{
			return (args | ...);
		}
	};

	auto and_op = and_op_t{};

	auto or_op = and_op_t{};

	auto not_op = [](index_t a)->index_t
	{
		return ~a;
	};

	template<typename F, typename... Ts>
	auto compose(F&& f, const Ts&... args)
	{
		return bit_vector_composer<F, Ts...>(std::forward<F>(f), args...);
	}

	index_t lowbit_pos(index_t id)
	{

		double d = id ^ (id - !!id);
		return (((int*)&d)[1] >> 20) - 1023;
	}

	template<typename T>
	bool empty(const T& vec)
	{
		return vec.layer0() == 0u;
	}

	template<bool controll = false, typename T, typename F>
	void for_each(const T& vec, const F& f)
	{
		std::array<index_t, LayerCount> masks{};
		masks[0] = vec.layer0();
		std::array<index_t, LayerCount> prefix{};
		while (true)
		{
			next:
			for (int32_t level = LayerCount - 1; level >= 0; --level)
			{
				if (masks[level] == 0) continue;
				index_t low = lowbit_pos(masks[level]);
				masks[level] &= ~(1 << low);
				index_t id = prefix[level] | low;
				if (level == 3)
				{
					if constexpr(controll)
					{
						if (!f(id)) return;
					}
					else
					{
						f(id);
						goto next;
					}
				}
				else
				{
					masks[level + 1] = vec.layer(level + 1, id);
					prefix[level + 1] = id << BitsPerLayer;
					goto next;
				}
			}
			return;
		}
	}
}

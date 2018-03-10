#pragma once
#include <type_traits>
#include "vector.h"
#include <tuple>
#include <array>
#include "small_vector.h"

namespace HBV
{
	using index_t = uint32_t;

	constexpr index_t BitsPerLayer = 6u;
	constexpr index_t LayerCount = 4u;

	//node index
	template<index_t layer>
	constexpr index_t index_of(index_t id) noexcept
	{
		return id >> ((LayerCount - layer)*BitsPerLayer);
	}

	//node value
	template<index_t layer>
	constexpr index_t value_of(index_t id) noexcept
	{
		return 1u << ((id >> ((LayerCount - layer - 1)*BitsPerLayer)) & ((1 << BitsPerLayer) - 1));
	}

	//fill num till highest bit
	//001001 -> 001111
	index_t fillbits(index_t x)
	{
		x |= (x >> 1);
		x |= (x >> 2);
		x |= (x >> 4);
		x |= (x >> 8);
		x |= (x >> 16);
		return x;
	}

	constexpr index_t EmptyNode = 0u;

	class bit_vector
	{
		index_t _layer0;
		chobo::small_vector<index_t> _layer1;
		lni::vector<index_t> _layer2;
		lni::vector<index_t> _layer3;
	public:
		bit_vector(index_t max, bool fill = false) noexcept
		{
			_layer0 = 0u;
			grow(max, fill);
		}

		bit_vector() noexcept { _layer0 = 0u; }

		void grow(index_t to, bool fill = false) noexcept
		{
			if (fill)
			{
				index_t value = (1u << BitsPerLayer) - 1u;
				_layer3.resize(index_of<3>(to) + 1, value);
				_layer2.resize(index_of<2>(to) + 1, value);
				_layer1.resize(index_of<1>(to) + 1, value);

				_layer3[index_of<3>(to)] = fillbits(value_of<3>(to));
				_layer2[index_of<2>(to)] = fillbits(value_of<2>(to));
				_layer1[index_of<1>(to)] = fillbits(value_of<1>(to));
				_layer0 = fillbits(value_of<0>(to)) - fillbits(_layer0) + _layer0;
			}
			else
			{
				_layer3.resize(index_of<3>(to) + 1, 0u);
				_layer2.resize(index_of<2>(to) + 1, 0u);
				_layer1.resize(index_of<1>(to) + 1, 0u);
			}
		}

		index_t size() const noexcept
		{
			return _layer3.size() * BitsPerLayer;
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
				//bubble for new node
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
				//bubble for empty node
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
			std::fill(_layer3.begin(), _layer3.end(), 0u);
			std::fill(_layer2.begin(), _layer2.end(), 0u);
			std::fill(_layer1.begin(), _layer1.end(), 0u);
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

	//compile time lazy compose
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
		bool compose_contain(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).contain(id)...);
		}

		bool contain(index_t id) const noexcept
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

	struct not_op_t {} not_op;

	//Degenerate into raw bitvector, could be very slow
	template<typename T>
	class bit_vector_not_composer
	{
		const T& _node;
		/*
		index_t _layer0;
		lni::vector<index_t> _layer1;
		lni::vector<index_t> _layer2;
		*/
	public:

		bit_vector_not_composer(const T& arg) : _node(arg) 
		{
			/*index_t to = _node.size() / BitsPerLayer;
			index_t value = (1 << BitsPerLayer - 1);
			_layer2.resize(index_of<2>(to) + 1, value);
			_layer1.resize(index_of<1>(to) + 1, value);
			_layer0 = value;*/
		}

		index_t layer0() const noexcept
		{
			return (1 << BitsPerLayer - 1);// _layer0;
		}

		index_t layer1(index_t id) const noexcept
		{
			return (1 << BitsPerLayer - 1);//_layer1[id];
		}

		index_t layer2(index_t id) const noexcept
		{
			return (1 << BitsPerLayer - 1);//_layer2[id];
		}

		index_t layer3(index_t id) const noexcept
		{
			return ~_node.layer3(id);
		}

		bool contain(index_t id) const noexcept
		{
			return ~_node.contain(id);
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

	template<typename F, typename... Ts>
	auto compose(F&& f, const Ts&... args)
	{
		return bit_vector_composer<F, Ts...>(std::forward<F>(f), args...);
	}

	template<typename T>
	auto compose(not_op_t f, const T& arg)
	{
		return bit_vector_not_composer<T>(arg);
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

		for (int32_t level = LayerCount - 1; level >= 0;)
		{
			//empty node, search parent
			if (masks[level] == 0) 
			{ 
				--level; 
				continue; 
			}
			index_t low = lowbit_pos(masks[level]);
			masks[level] &= ~(1 << low);
			index_t id = prefix[level] | low;
			if (level == 3) //leaf node, search sibling
			{
				if constexpr(controll)
				{
					if (!f(id)) break;
					continue;
				}
				else
				{
					f(id);
					continue;
				}
			}
			else //tree node, search child
			{
				masks[level + 1] = vec.layer(level + 1, id);
				prefix[level + 1] = id << BitsPerLayer;
				++level;
				continue;
			}
		}
	}
}

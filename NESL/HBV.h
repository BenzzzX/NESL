#pragma once
#include <type_traits>
#include <tuple>
#include <array>
#include <limits>
#include <intrin.h>
#include <algorithm>

#include "small_vector.h"
#include "vector.h"

namespace HBV
{
	using index_t = uint32_t;
	using flag_t = uint64_t;

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
	constexpr flag_t value_of(index_t id) noexcept
	{
		constexpr index_t mask = (1 << BitsPerLayer) - 1;
		index_t index = id >> ((LayerCount - layer - 1)*BitsPerLayer);
		return flag_t(1u) << (index & mask);
	}

	//fill num till highest bit
	//001001 -> 001111
	flag_t fillbits(flag_t x)
	{
		x |= (x >> 1);
		x |= (x >> 2);
		x |= (x >> 4);
		x |= (x >> 8);
		x |= (x >> 16);
		x |= (x >> 32);
		return x;
	}

	index_t lowbit_pos(flag_t id)
	{
		unsigned long result;
		return _BitScanForward64(&result, id) ? result : 0;
	}
	
	index_t highbit_pos(flag_t id)
	{
		unsigned long result;
		return _BitScanReverse64(&result, id) ? result : 0;
	}

	constexpr flag_t EmptyNode = 0u;
	constexpr flag_t FullNode = EmptyNode - 1u;

	class bit_vector
	{
		index_t _end;
		flag_t _layer0;
		chobo::small_vector<flag_t> _layer1;
		lni::vector<flag_t> _layer2;
		lni::vector<flag_t> _layer3;

		void set_range_true(index_t begin, index_t end)
		{
			index_t startPos = begin;
			index_t endPos = end - 1;

#define		SET_LAYER(N) \
			if (index_of<N>(startPos) == index_of<N>(endPos)) \
			{ \
				_layer##N[index_of<N>(startPos)] |= (value_of<N>(endPos) - value_of<N>(startPos)) + value_of<N>(endPos); \
			} \
			else \
			{ \
				index_t start = index_of<N>(startPos); \
				index_t end = index_of<N>(endPos); \
				if (start + 1 < end) \
					std::fill(&_layer##N[start + 1], &_layer##N[end], FullNode); \
				_layer##N[start] |= FullNode - value_of<N>(startPos) + 1; \
				_layer##N[end] |= (value_of<N>(endPos) - 1) + value_of<N>(endPos); \
			}

			SET_LAYER(3);
			SET_LAYER(2);
			SET_LAYER(1);

			_layer0 |= (value_of<0>(endPos) - value_of<0>(startPos)) + value_of<0>(endPos);
#undef		SET_LAYER
		}

		void bubble_empty(index_t id)
		{
			index_t index_3 = index_of<3>(id);
			if (_layer3[index_3] != EmptyNode) return;

			index_t index_2 = index_of<2>(id);
			_layer2[index_2] &= ~value_of<2>(id);
			if (_layer2[index_2] != EmptyNode) return;

			index_t index_1 = index_of<1>(id);
			_layer1[index_1] &= ~value_of<1>(id);
			if (_layer1[index_1] != EmptyNode) return;

			_layer0 &= ~value_of<0>(id);
		}

		void bubble_fill(index_t id)
		{
			index_t index_3 = index_of<3>(id);
			if (_layer3[index_3] == EmptyNode)
			{
				_layer2[index_of<2>(id)] |= value_of<2>(id);
				_layer1[index_of<1>(id)] |= value_of<1>(id);
				_layer0 |= value_of<0>(id);
			}
		}

		void set_range_false(index_t begin, index_t end)
		{
			index_t startPos = begin;
			index_t endPos = end - 1;

			if (index_of<3>(startPos) == index_of<3>(endPos))
			{
				_layer3[index_of<3>(startPos)] &= ~((value_of<3>(endPos) - value_of<3>(startPos)) + value_of<3>(endPos));
				bubble_empty(startPos);
			}
			else
			{
				index_t start = index_of<3>(startPos);
				index_t end = index_of<3>(endPos);
				if (start + 1 < end)
					std::fill(&_layer3[start + 1], &_layer3[end], EmptyNode);
				_layer3[start] &= value_of<3>(startPos) - 1;
				_layer3[end] &= FullNode - value_of<3>(endPos)*2 + 1;

				bubble_empty(startPos);
				bubble_empty(endPos);
			}

			if (index_of<2>(startPos) == index_of<2>(endPos))
			{
				_layer2[index_of<2>(startPos)] &= ~(value_of<2>(endPos) - value_of<2>(startPos) * 2);
				bubble_empty(startPos);
			}
			else
			{
				index_t start = index_of<2>(startPos);
				index_t end = index_of<2>(endPos);
				if (start + 1 < end)
					std::fill(&_layer2[start + 1], &_layer2[end], EmptyNode);
				_layer2[start] &= (value_of<2>(startPos) - 1) + value_of<2>(startPos);
				_layer2[end] &= FullNode - value_of<2>(endPos) + 1;
			}

			if (index_of<1>(startPos) == index_of<1>(endPos))
			{
				_layer1[index_of<1>(startPos)] &= ~(value_of<1>(endPos) - value_of<1>(startPos) * 2);
				bubble_empty(startPos);
			}
			else
			{
				index_t start = index_of<1>(startPos);
				index_t end = index_of<1>(endPos);
				if (start + 1 < end)
					std::fill(&_layer1[start + 1], &_layer1[end], EmptyNode);
				_layer1[start] &= (value_of<1>(startPos) - 1) + value_of<1>(startPos);
				_layer1[end] &= FullNode - value_of<1>(endPos) + 1;
			}

			_layer0 &= ~((value_of<0>(endPos) - value_of<0>(startPos)) + value_of<0>(endPos));

		}

	public:
		bit_vector(index_t max, bool fill = false) noexcept
		{
			_layer0 = 0u;
			_end = max - 1;
			if (fill)
			{
				_layer3.resize(index_of<3>(_end) + 1, FullNode);
				_layer3[index_of<3>(_end)] = (value_of<3>(_end) - 1) + value_of<3>(_end);
				_layer2.resize(index_of<2>(_end) + 1, FullNode);
				_layer2[index_of<2>(_end)] = (value_of<2>(_end) - 1) + value_of<2>(_end);
				_layer1.resize(index_of<1>(_end) + 1, FullNode);
				_layer1[index_of<1>(_end)] = (value_of<1>(_end) - 1) + value_of<1>(_end);
				_layer0 = (value_of<0>(_end) - 1) + value_of<0>(_end);
			}
			else
			{
				_layer3.resize(index_of<3>(_end) + 1, 0u);
				_layer2.resize(index_of<2>(_end) + 1, 0u);
				_layer1.resize(index_of<1>(_end) + 1, 0u);
			}
		}

		bit_vector() noexcept 
			: bit_vector(10) {}

		void grow_to(index_t to, bool set = false) noexcept
		{
			to -= 1;
			to = std::min<index_t>(16'777'216u, to);
			
			_layer3.resize(index_of<3>(to) + 1, 0u);
			_layer2.resize(index_of<2>(to) + 1, 0u);
			_layer1.resize(index_of<1>(to) + 1, 0u);
			if (set)
				set_range(_end + 1, to + 1, true);
			_end = to;
		}

		bool empty()
		{
			return layer0() == EmptyNode;
		}

		void set_range(index_t begin, index_t end, bool value)
		{
			if (value)
				set_range_true(begin, end);
			else
				set_range_false(begin, end);
		}

		index_t size() const noexcept
		{
			return _end + 1;
		}

		flag_t layer0() const noexcept
		{
			return _layer0;
		}

		flag_t layer1(index_t id) const noexcept
		{
			return _layer1[id];
		}

		flag_t layer2(index_t id) const noexcept
		{
			return _layer2[id];
		}

		flag_t layer3(index_t id) const noexcept
		{
			return _layer3[id];
		}

		void set(index_t id, bool value) noexcept
		{
			index_t index_3 = index_of<3>(id);
			flag_t value_3 = value_of<3>(id);

			if (value)
			{
				//bubble for new node
				bubble_fill(id);
				_layer3[index_3] |= value_3;
			}
			else
			{
				//bubble for empty node
				_layer3[index_3] &= ~value_3;
				bubble_empty(id);
			}
		}

		void clear() noexcept
		{
			std::fill(_layer3.begin(), _layer3.end(), 0u);
			std::fill(_layer2.begin(), _layer2.end(), 0u);
			std::fill(_layer1.begin(), _layer1.end(), 0u);
			_layer0 = 0u;
		}

		//NOTE: it won't grow
		template<bool reverse = false, typename T>
		void merge(const T& vec)
		{
			std::array<HBV::flag_t, LayerCount - 1> nodes{};
			std::array<index_t, LayerCount - 1> prefix{};
			if constexpr(reverse)
				nodes[0] = vec.layer0() & _layer0;
			else 
				nodes[0] = vec.layer0();
			index_t level = 0;
			if (nodes[0] == EmptyNode) return;
			for (;;)
			{
				index_t low = lowbit_pos(nodes[level]);
				nodes[level] &= ~(flag_t(1u) << low);
				index_t id = prefix[level] | low;

				++level;
				if (level == 3)
				{
					if (id >= _layer3.size()) return;
					
					HBV::flag_t node = vec.layer3(id);
					if constexpr(reverse)
					{
						_layer3[id] &= ~node;
						bubble_empty(id << BitsPerLayer);
					}
					else
					{
						bubble_fill(id << BitsPerLayer);
						_layer3[id] |= node;
					}
					do
					{
						//root is empty, stop iterating
						if (level == 0)
							return;
						--level;
					} while (nodes[level] == EmptyNode);
				}
				else
				{
					if constexpr(reverse)
						nodes[level] = vec.layer(level, id) & layer(level, id);
					else
						nodes[level] = vec.layer(level, id);
					prefix[level] = id << BitsPerLayer;
				}
			}
		}

		bool contain(index_t id) const noexcept
		{
			index_t index_3 = index_of<3>(id);
			return (index_3 < _layer3.size()) && (_layer3[index_3] & value_of<3>(id));
		}

		flag_t layer(index_t level, index_t id) const noexcept
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
		template<typename T>
		struct storage { using type = T; };

		template<>
		struct storage<bit_vector> { using type = const bit_vector&; };

		template<typename T>
		using storage_t = typename storage<T>::type;

		const std::tuple<storage_t<Ts>...> _nodes;
		F op = {};
	public:

		template<typename... Ts>
		bit_vector_composer(Ts&&... args) : _nodes(std::forward<Ts>(args)...) {}
		template<index_t... i>
		flag_t compose_layer0(std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer0()...);
		}

		flag_t layer0() const noexcept
		{
			return compose_layer0(std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		flag_t compose_layer1(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer1(id)...);
		}

		flag_t layer1(index_t id) const noexcept
		{
			return compose_layer1(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		flag_t compose_layer2(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer2(id)...);
		}

		flag_t layer2(index_t id) const noexcept
		{
			return compose_layer2(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		flag_t compose_layer3(index_t id, std::index_sequence<i...>) const noexcept
		{
			return op(std::get<i>(_nodes).layer3(id)...);
		}

		flag_t layer3(index_t id) const noexcept
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

		flag_t layer(index_t level, index_t id) const noexcept
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
		flag_t operator()(Ts... args) const
		{
			return (args & ...);
		}
	};

	struct or_op_t
	{
		template<typename... Ts>
		flag_t operator()(Ts... args) const
		{
			return (args | ...);
		}
	};

	auto and_op = and_op_t{};

	auto or_op = or_op_t{};

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

		flag_t layer0() const noexcept
		{
			return (1 << BitsPerLayer - 1);// _layer0;
		}

		flag_t layer1(index_t id) const noexcept
		{
			return (1 << BitsPerLayer - 1);//_layer1[id];
		}

		flag_t layer2(index_t id) const noexcept
		{
			return (1 << BitsPerLayer - 1);//_layer2[id];
		}

		flag_t layer3(index_t id) const noexcept
		{
			return ~_node.layer3(id);
		}

		bool contain(index_t id) const noexcept
		{
			return ~_node.contain(id);
		}

		flag_t layer(index_t level, index_t id) const noexcept
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
	__forceinline bit_vector_composer<F, std::decay_t<Ts>...> compose(F, Ts&&... args)
	{
		return { std::forward<Ts>(args)... };
	}

	

	template<typename T>
	bool empty(const T& vec)
	{
		return vec.layer0() == 0u;
	}

	template<index_t Level = 3, typename T>
	index_t last(const T& vec) noexcept
	{
		flag_t nodes{};
		nodes = vec.layer0();
		if (nodes == EmptyNode) return 0;
		index_t prefix{};

		for (int32_t level = 0;; ++level)
		{
			index_t high = highbit_pos(nodes);
			index_t id = prefix | high;
			if (level >= Level)
				return id;
			prefix = flag_t(id) << BitsPerLayer;
			nodes = vec.layer(level + 1, id);
		}
	}

	template<index_t Level = 3, typename T>
	index_t first(const T& vec) noexcept
	{
		flag_t nodes{};
		nodes = vec.layer0();
		if (nodes == EmptyNode) return 0;
		index_t prefix{};

		for (int32_t level = 0;; ++level)
		{
			index_t high = lowbit_pos(nodes);
			index_t id = prefix | high;
			if (level >= Level)
				return id;
			prefix = flag_t(id) << BitsPerLayer;
			nodes = vec.layer(level + 1, id);
		}
	}


	template<index_t Level = 3, typename T, typename F>
	void for_each(const T& vec, const F& f) noexcept
	{
		std::array<flag_t, Level + 1> nodes{};
		std::array<index_t, Level + 1> prefix{};
		nodes[0] = vec.layer0();
		index_t level = 0;
		if (nodes[0] == EmptyNode) return;
		
		for (;;)
		{
			index_t low = lowbit_pos(nodes[level]);
			nodes[level] &= ~(flag_t(1u) << low);
			index_t id = prefix[level] | low;
			if (level < Level) //leaf node, iterate sibling
			{
				++level;
				nodes[level] = vec.layer(level, id);
				prefix[level] = id << BitsPerLayer;
			}
			else //tree node, iterate child
			{
				f(id);
				while (nodes[level] == EmptyNode)
				{
					//root is empty, stop iterating
					if (level == 0)
						return;
					--level;
				}
			}
		}
	}
}

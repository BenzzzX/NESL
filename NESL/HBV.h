#pragma once
#include <type_traits>
#include <vector>
namespace HBV
{
	using index_t = std::size_t;

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

		void grow(index_t to)
		{
			_layer3.resize(index_of<3>(to));
			_layer2.resize(index_of<2>(to));
			_layer1.resize(index_of<1>(to));
		}
	public:
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
				if (index_3 >= _layer3.size()) grow(id);
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
				_layer3[index_3] &= !value_3;
				if (_layer3[index_3] != EmptyNode) return true;

				index_t index_2 = index_of<2>(id);
				_layer2[index_2] &= !value_of<2>(id);
				if (_layer2[index_2] != EmptyNode) return true;

				index_t index_1 = index_of<1>(id);
				_layer1[index_1] &= !value_of<1>(id);
				if (_layer1[index_1] != EmptyNode) return true;


				_layer0 &= !value_of<0>(id);
			}
		}

		void clear()
		{
			_layer3.clear();
			_layer2.clear();
			_layer1.clear();
			_layer0 = 0u;
		}

		bool contain(index_t id)
		{
			index_t index_3 = index_of<3>(id);
			return (index_3 < _layer3.size()) && (_layer3[index_3] & !value_of<3>(id));
		}
	};

	template<typename F,typename... Ts>
	class bit_vector_composer
	{
		std::tuple<Ts&...> node;
	public:
		template<index_t... i>
		index_t compose_layer0(std::index_sequence<i...>) const noexcept
		{
			return F{}(std::get<i>(node).layer0()...);
		}

		index_t layer0() const noexcept
		{
			return compose_layer0(std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_layer1(index_t id, std::index_sequence<i...>) const noexcept
		{
			return F{}(std::get<i>(node).layer1(id)...);
		}

		index_t layer1(index_t id) const noexcept
		{
			return compose_layer1(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_layer2(index_t id, std::index_sequence<i...>) const noexcept
		{
			return F{}(std::get<i>(node).layer2(id)...);
		}

		index_t layer2(index_t id) const noexcept
		{
			return compose_layer2(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_layer3(index_t id, std::index_sequence<i...>) const noexcept
		{
			return F{}(std::get<i>(node).layer3(id)...);
		}

		index_t layer3(index_t id) const noexcept
		{
			return compose_layer3(id, std::make_index_sequence<sizeof...(Ts)>());
		}

		template<index_t... i>
		index_t compose_contain(index_t id, std::index_sequence<i...>) const noexcept
		{
			return F{}(std::get<i>(node).contain(id)...);
		}

		index_t contain(index_t id) const noexcept
		{
			return compose_contain(id, std::make_index_sequence<sizeof...(Ts)>());
		}
	};
}

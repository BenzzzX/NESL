#pragma once
#include <any>
#include <unordered_map>

namespace ESL
{
	class states
	{
		std::unordered_map<std::size_t, std::any> _states;
	public:
		template<typename T>
		T* get() noexcept
		{
			auto it = _states.find(typeid(T).hash_code());
			if (it == _states.end())
				return nullptr;
			return std::any_cast<T>(&it->second);
		}

		template<typename T>
		bool valid() const noexcept
		{
			auto it = _states.find(typeid(T).hash_code());
			if (it == _states.end())
				return false;
			return true;
		}

		template<typename T, typename... Ts>
		bool add(Ts... args) noexcept
		{
			return _states.insert({ typeid(T).hash_code(), std::any{T{args...}} }).second;
		}

		template <typename... Ts> struct DispatchHelper
		{
			template <typename F>
			void dispatch(states& st, F& f) noexcept
			{
				f((*st.get<Ts>())...);
			}

			bool is_legal(const states& st) noexcept
			{
				return (st.valid<Ts>() || ...);
			}
		};
	};

	


}


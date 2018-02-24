#pragma once
#include <any>
#include <unordered_map>

namespace ESL
{
	class GlobalStates
	{
		std::unordered_map<std::size_t, std::any> _state;
	public:
		template<typename T>
		T* Get() noexcept
		{
			auto it = _state.find(typeid(T).hash_code());
			if (it == _state.end())
				return nullptr;
			return std::any_cast<T>(&it->second);
		}

		template<typename T>
		bool Valid() const noexcept
		{
			auto it = _state.find(typeid(T).hash_code());
			if (it == _state.end())
				return false;
			return true;
		}

		template<typename T, typename... Ts>
		bool Add(Ts... args) noexcept
		{
			return _state.insert({ typeid(T).hash_code(), std::any{T{args...}} }).second;
		}

		template <typename... Ts> 
		struct DispatchHelper
		{
			template <typename F>
			void Dispatch(GlobalStates& st, F& f) noexcept
			{
				f((*st.Get<Ts>())...);
			}

			bool Legal(const GlobalStates& st) noexcept
			{
				return (st.Valid<Ts>() || ...);
			}
		};
	};
}


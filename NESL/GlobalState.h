#pragma once

namespace ESL
{
	template<typename T>
	class GlobalState
	{
		T _singleton;
	public:
		template<typename... Ts>
		GlobalState(Ts&&... args) : _singleton({ std::forward<Ts>(args)... }) {}

		T &Raw()
		{
			return _singleton;
		}
	};
}


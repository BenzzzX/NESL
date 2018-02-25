#pragma once

namespace HBV
{
	//TODO
	template<bool controll = false, typename T, typename F>
	void for_each_paralell(const T& vec, const F& f);
}

namespace ESL
{
	//TODO
	template<typename F, typename S>
	void ParallelDispatch(S states, F&& logic);
}


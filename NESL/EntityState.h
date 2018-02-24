#pragma once
#include "HBV.h"

namespace ESL
{
	template<typename T>
	class GEntityState
	{
		HBV::bit_vector _entity;
		T _container;
	};
}

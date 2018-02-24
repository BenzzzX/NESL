#include "NESL.h"
#include "GlobalStates.h"
#include <iostream>
int main()
{
	HBV::bit_vector veca{6u}, vecb{6u};
	veca.set(1, true);
	veca.set(3, true);
	vecb.set(1, true);
	auto vecab = HBV::compose(HBV::and_op, veca, vecb);

	HBV::for_each(vecab, [](uint32_t id) {
		std::cout << id << std::endl;
	});

	//ESL::GlobalStates st;
	//st.Add<int>(1);
	
	//std::cout << *st.Get<int>() << std::endl;
    return 0;
}


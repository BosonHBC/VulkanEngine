#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "../Engine/Engine.h"

int main()
{
	VKE::init();

	VKE::run();

	VKE::cleanup();

	_CrtDumpMemoryLeaks();

	return 0;
}
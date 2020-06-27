#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "../Engine/Engine.h"

int main()
{
	int exitCode = 0;
	exitCode = VKE::init();

	VKE::run();

	VKE::cleanup();

	_CrtDumpMemoryLeaks();

	return exitCode;
}
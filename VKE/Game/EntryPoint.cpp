#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#include "../Engine/Engine.h"

int main()
{
	int exitCode = 0;
	exitCode = VKE::init();
	if (exitCode == EXIT_FAILURE) 
	{
		return exitCode;
	}

	VKE::run();

	VKE::cleanup();

	_CrtDumpMemoryLeaks();

	return exitCode;
}
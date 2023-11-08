#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>

#include "../Engine/Engine.h"

int32 main()
{
	int32 exitCode = 0;
	exitCode = VKE::Init();
	if (exitCode == EXIT_FAILURE) 
	{
		return exitCode;
	}

	VKE::Run();

	VKE::Cleanup();

	_CrtDumpMemoryLeaks();

	return exitCode;
}
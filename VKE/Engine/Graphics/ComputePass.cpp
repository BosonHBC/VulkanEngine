#include "ComputePass.h"

namespace VKE
{

	bool FComputePass::needSynchronization() const
	{
		assert(pMainDevice);
		if (pMainDevice)
		{
			return ComputeFamilyIndex != pMainDevice->QueueFamilyIndices.graphicFamily;
		}
		return false;
	}

}
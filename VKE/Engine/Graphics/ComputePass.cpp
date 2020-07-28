#include "ComputePass.h"

namespace VKE
{

	bool FComputePass::needSynchronization() const
	{
		assert(pMainDevice);
		if (pMainDevice)
		{
			return ComputeFamilyIndex != pMainDevice->QueueFamilies.graphicFamily;
		}
		return false;
	}

}
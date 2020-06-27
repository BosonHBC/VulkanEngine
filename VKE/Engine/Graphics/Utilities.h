#pragma once
// Indices (location) of Queue Families (if they exist at all)
namespace VKE
{
	struct FQueueFamilyIndices
	{
		int graphicFamily = -1;		// Location of Graphics Queue Family

		bool IsValid() const { return graphicFamily >= 0; }
	};
}

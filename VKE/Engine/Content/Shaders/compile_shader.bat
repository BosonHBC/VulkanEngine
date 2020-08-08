C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe vert.vert -o vert.spv
C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe frag.frag -o frag.spv
C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe bigTriangle.vert -o bigTriangle.spv
C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe second.frag -o second.spv
C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe particle/particle.frag -o particle/particle.frag.spv
C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe particle/particle.vert -o particle/particle.vert.spv
C:/VulkanSDK/1.2.141.2/Bin32/glslc.exe particle/particle.comp -o particle/particle.comp.spv

D:\Github\VulkanEngine\VKE\AssetBuilder\Binaries\Win32\Debug\AssetBuilder.exe "frag.spv" "vert.spv" "bigTriangle.spv" "second.spv" "particle/particle.frag.spv" "particle/particle.vert.spv" "particle/particle.comp.spv"
pause
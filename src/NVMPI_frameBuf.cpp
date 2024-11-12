#include "NVMPI_frameBuf.hpp"
#include <iostream> //LOG. TODO: add some LOG() define

bool NVMPI_frameBuf::alloc(NvBufferCreateParams& input_params)
{
	int ret = 0;
#ifdef WITH_NVUTILS
	ret = NvBufSurf::NvAllocate(&input_params, 1, &dst_dma_fd);
	if(ret<0)
	{
		std::cerr << "Failed to allocate buffer" << std::endl;
		return false;
	}
	
	ret = NvBufSurfaceFromFd(dst_dma_fd, (void**)(&dst_dma_surface));
	if(ret<0)
	{
		std::cerr << "Failed to get surface for buffer" << std::endl;
		NvBufferDestroy(dst_dma_fd);
		dst_dma_fd = -1;
		return false;
	}
#else
	ret = NvBufferCreateEx(&dst_dma_fd, &input_params);
	if(ret<0)
	{
		std::cerr << "Failed to allocate buffer" << std::endl;
		return false;
	}
#endif
	
	return true;
}

bool NVMPI_frameBuf::destroy()
{
	int ret = 0;
	if(dst_dma_fd >= 0)
	{
		ret = NvBufferDestroy(dst_dma_fd);
		if(ret<0)
		{
			std::cerr << "Failed to Destroy NvBuffer" << std::endl;
			return false;
		}
		dst_dma_fd = -1;
#ifdef WITH_NVUTILS
		dst_dma_surface = NULL;
#endif
	}
	
	return true;
}

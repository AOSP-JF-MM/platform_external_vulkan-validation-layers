/*
 * Vulkan
 *
 * Copyright (C) 2015 LunarG, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Jon Ashburn <jon@lunarg.com>
 *   Courtney Goeltzenleuchter <courtney@lunarg.com>
 */

#include <string.h>
#include "loader_platform.h"
#include "loader.h"
#include "wsi_lunarg.h"

/************  Trampoline entrypoints *******************/
/* since one entrypoint is instance level will make available all entrypoints */
VkResult VKAPI wsi_lunarg_GetDisplayInfoWSI(
        VkDisplayWSI                            display,
        VkDisplayInfoTypeWSI                    infoType,
        size_t*                                 pDataSize,
        void*                                   pData)
{
    const VkLayerInstanceDispatchTable *disp;
    VkResult res;

    disp = loader_get_instance_dispatch(display);

    loader_platform_thread_lock_mutex(&loader_lock);
    res = disp->GetDisplayInfoWSI(display, infoType, pDataSize, pData);
    loader_platform_thread_unlock_mutex(&loader_lock);
    return res;
}

VkResult wsi_lunarg_CreateSwapChainWSI(
        VkDevice                                device,
        const VkSwapChainCreateInfoWSI*         pCreateInfo,
        VkSwapChainWSI*                         pSwapChain)
{
    const VkLayerDispatchTable *disp;
    VkResult res;

    disp = loader_get_dispatch(device);

    res = disp->CreateSwapChainWSI(device, pCreateInfo, pSwapChain);
    if (res == VK_SUCCESS) {
        loader_init_dispatch(*pSwapChain, disp);
    }

    return res;
}

VkResult wsi_lunarg_DestroySwapChainWSI(
        VkSwapChainWSI                          swapChain)
{
    const VkLayerDispatchTable *disp;

    disp = loader_get_dispatch(swapChain);

    return disp->DestroySwapChainWSI(swapChain);
}

static VkResult wsi_lunarg_GetSwapChainInfoWSI(
        VkSwapChainWSI                          swapChain,
        VkSwapChainInfoTypeWSI                  infoType,
        size_t*                                 pDataSize,
        void*                                   pData)
{
    const VkLayerDispatchTable *disp;

    disp = loader_get_dispatch(swapChain);

    return disp->GetSwapChainInfoWSI(swapChain, infoType, pDataSize, pData);
}

static VkResult wsi_lunarg_QueuePresentWSI(
        VkQueue                                 queue,
        const VkPresentInfoWSI*                 pPresentInfo)
{
    const VkLayerDispatchTable *disp;

    disp = loader_get_dispatch(queue);

    return disp->QueuePresentWSI(queue, pPresentInfo);
}

/************  loader instance chain termination entrypoints ***************/
VkResult loader_GetDisplayInfoWSI(
        VkDisplayWSI                            display,
        VkDisplayInfoTypeWSI                    infoType,
        size_t*                                 pDataSize,
        void*                                   pData)
{
    /* TODO: need another way to find the icd, display is not a gpu object */
//    uint32_t gpu_index;
//    struct loader_icd *icd = loader_get_icd((VkPhysicalDevice) display, &gpu_index); //TODO fix dispaly -> PhysDev
    VkResult res = VK_ERROR_INITIALIZATION_FAILED;

    for (struct loader_instance *inst = loader.instances; inst; inst = inst->next) {
        for (struct loader_icd *icd = inst->icds; icd; icd = icd->next) {
            for (uint32_t i = 0; i < icd->gpu_count; i++) {
                if (icd->GetDisplayInfoWSI)
                    res = icd->GetDisplayInfoWSI(display, infoType, pDataSize, pData);
            }
        }
    }

    return res;
}


/************ extension enablement ***************/
#define WSI_LUNARG_EXT_ARRAY_SIZE 1
static const struct loader_extension_property wsi_lunarg_extension_info = {
    .info =  {
        .sType = VK_STRUCTURE_TYPE_EXTENSION_PROPERTIES,
        .name = VK_WSI_LUNARG_EXTENSION_NAME,
        .version = VK_WSI_LUNARG_REVISION,
        .description = "loader: LunarG WSI extension",
        },
    .origin = VK_EXTENSION_ORIGIN_LOADER,
};

void wsi_lunarg_add_instance_extensions(
        struct loader_extension_list *ext_list)
{
    loader_add_to_ext_list(ext_list, 1, &wsi_lunarg_extension_info);
}

void wsi_lunarg_create_instance(
        struct loader_instance *ptr_instance)
{
    ptr_instance->wsi_lunarg_enabled = has_vk_extension_property(
                                             &wsi_lunarg_extension_info.info,
                                             &ptr_instance->enabled_instance_extensions);
}

void *wsi_lunarg_GetInstanceProcAddr(
        VkInstance                              instance,
        const char*                             pName)
{
    if (instance == VK_NULL_HANDLE)
        return NULL;

    /* since two of these entrypoints must be loader handled will report all */
    if (!strcmp(pName, "vkGetDisplayInfoWSI"))
        return (void*) wsi_lunarg_GetDisplayInfoWSI;
    if (!strcmp(pName, "vkCreateSwapChainWSI"))
        return (void*) wsi_lunarg_CreateSwapChainWSI;
    if (!strcmp(pName, "vkDestroySwapChainWSI"))
        return (void*) wsi_lunarg_DestroySwapChainWSI;
    if (!strcmp(pName, "vkGetSwapChainInfoWSI"))
        return (void*) wsi_lunarg_GetSwapChainInfoWSI;
    if (!strcmp(pName, "vkQueuePresentWSI"))
        return (void*) wsi_lunarg_QueuePresentWSI;

    return NULL;
}

void *wsi_lunarg_GetDeviceProcAddr(
        VkDevice                                device,
        const char*                             name)
{
    if (device == VK_NULL_HANDLE)
        return NULL;

    /* only handle device entrypoints that are loader special cases */
    if (!strcmp(name, "vkCreateSwapChainWSI"))
        return (void*) wsi_lunarg_CreateSwapChainWSI;

    return NULL;
}
#pragma once
#include <vulkan/vulkan_raii.hpp>
#include "VkBootstrap.h"
#include <SDL3/SDL.h>

class VulkanContext {
public:
    vk::raii::Context                context; // creates the RAII Vulkan_hpp context for the entire project
    vk::raii::Instance               instance            = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger      = nullptr;
    vk::raii::SurfaceKHR             surface             = nullptr;
    vk::raii::PhysicalDevice         physicalDevice      = nullptr;
    vk::raii::Device                 device              = nullptr;
    vkb::Device                      vkbDevice           = {};
    uint32_t                         graphicsFamilyIndex = 0;
    uint32_t                         presentFamilyIndex  = 0;
    vk::raii::Queue                  graphicsQueue       = nullptr;
    vk::raii::Queue                  presentQueue        = nullptr;
    // also responsible for the present queue (in my case they are in the same family)

    void init(SDL_Window* window);
    void cleanup();

private:
    void handleBootstrapErrors(auto obj_ret);

};

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "types.hpp"


static bool32 create_instance(VkInstance *instance) {
  // Initialise info about our application (its name etc.)
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Peony";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "peony";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  uint32 glfwExtensionCount = 0;
  char const **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  createInfo.enabledExtensionCount = glfwExtensionCount;
  createInfo.ppEnabledExtensionNames = glfwExtensions;

  createInfo.enabledLayerCount = 0;

  if (vkCreateInstance(&createInfo, nullptr, instance) != VK_SUCCESS) {
    return false;
  }

  return true;
}


void vulkan__init(VkInstance *instance) {
  create_instance(instance);
}


void vulkan__destroy(VkInstance *instance) {
  vkDestroyInstance(*instance, nullptr);
}

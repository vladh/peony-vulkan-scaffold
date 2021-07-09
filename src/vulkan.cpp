#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"


constexpr bool const USE_VALIDATION = true;
constexpr char const * const VALIDATION_LAYERS[] = {
  "VK_LAYER_KHRONOS_validation",
};


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
  if (USE_VALIDATION) {
    createInfo.enabledLayerCount = LEN(VALIDATION_LAYERS);
    createInfo.ppEnabledLayerNames = VALIDATION_LAYERS;
  } else {
    createInfo.enabledLayerCount = 0;
  }

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


bool ensure_validation_layers_supported() {
  // Get available layers
  uint32 n_available_layers;
  vkEnumerateInstanceLayerProperties(&n_available_layers, nullptr);

  VkLayerProperties *available_layers = (VkLayerProperties*)calloc(
    1, n_available_layers * sizeof(VkLayerProperties)
  );
  defer { free(available_layers); };
  vkEnumerateInstanceLayerProperties(&n_available_layers, available_layers);

  // Compare with desired layers
  uint32 n_validation_layers = LEN(VALIDATION_LAYERS);
  range_named (idx_desired, 0, n_validation_layers) {
    bool32 did_find_layer = false;
    range_named (idx_available, 0, n_available_layers) {
      if (pstr_eq(
          available_layers[idx_available].layerName,
          VALIDATION_LAYERS[idx_desired]
      )) {
        logs::info("Found validation layer: %s", VALIDATION_LAYERS[idx_desired]);
        did_find_layer = true;
        break;
      }
    }
    if (!did_find_layer) {
      logs::error("Could not find validation layer: %s", VALIDATION_LAYERS[idx_desired]);
      return false;
    }
  }

  return true;
}


void vulkan::init(VkInstance *instance) {
  if (USE_VALIDATION) {
    if (!ensure_validation_layers_supported()) {
      logs::fatal("Could not get required validation layers.");
    }
  }
  create_instance(instance);
}


void vulkan::destroy(VkInstance *instance) {
  vkDestroyInstance(*instance, nullptr);
}

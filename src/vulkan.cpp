#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"


constexpr u32 const MAX_N_REQUIRED_EXTENSIONS = 256;
constexpr bool const USE_VALIDATION = true;
constexpr char const * const VALIDATION_LAYERS[] = {
  "VK_LAYER_KHRONOS_validation",
};


static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
  VkDebugUtilsMessageTypeFlagsEXT message_type,
  const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
  void* p_user_data
) {
  logs::info("(Validation layer) %s", p_callback_data->pMessage);
  return VK_FALSE;
}


static void get_required_extensions(
  char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS],
  u32 *n_required_extensions
) {
  char const **glfw_extensions = glfwGetRequiredInstanceExtensions(
    n_required_extensions
  );
  range (0, *n_required_extensions) {
    required_extensions[idx] = glfw_extensions[idx];
  }
  if (USE_VALIDATION) {
    required_extensions[(*n_required_extensions)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  }
}


static bool create_instance(
  VkState *vk_state,
  VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_create_info
) {
  // Initialise info about our application (its name etc.)
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Peony";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "peony";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  create_info.pApplicationInfo = &app_info;

  char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS];
  u32 n_required_extensions;
  get_required_extensions(required_extensions, &n_required_extensions);
  create_info.enabledExtensionCount = n_required_extensions;
  create_info.ppEnabledExtensionNames = required_extensions;

  if (USE_VALIDATION) {
    create_info.enabledLayerCount = LEN(VALIDATION_LAYERS);
    create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    create_info.pNext = debug_messenger_create_info;
  } else {
    create_info.enabledLayerCount = 0;
  }

  if (vkCreateInstance(&create_info, nullptr, &vk_state->instance) != VK_SUCCESS) {
    return false;
  }

  return true;
}


static bool ensure_validation_layers_supported() {
  // Get available layers
  u32 n_available_layers;
  vkEnumerateInstanceLayerProperties(&n_available_layers, nullptr);

  VkLayerProperties *available_layers = (VkLayerProperties*)calloc(
    1, n_available_layers * sizeof(VkLayerProperties)
  );
  defer { free(available_layers); };
  vkEnumerateInstanceLayerProperties(&n_available_layers, available_layers);

  // Compare with desired layers
  u32 n_validation_layers = LEN(VALIDATION_LAYERS);
  range_named (idx_desired, 0, n_validation_layers) {
    bool did_find_layer = false;
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


static VkResult CreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT *p_create_info,
  const VkAllocationCallbacks *p_allocator,
  VkDebugUtilsMessengerEXT *p_debug_messenger
) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkCreateDebugUtilsMessengerEXT"
  );
  if (func == nullptr) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return func(instance, p_create_info, p_allocator, p_debug_messenger);
}


static void DestroyDebugUtilsMessengerEXT(
  VkInstance instance,
  VkDebugUtilsMessengerEXT debug_messenger,
  const VkAllocationCallbacks* p_allocator
) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT"
  );
  if (func == nullptr) {
    return;
  }
  func(instance, debug_messenger, p_allocator);
}


static void init_debug_messenger(
  VkState *vk_state,
  VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_create_info
) {
  if (!USE_VALIDATION) {
    return;
  }

  if (
    CreateDebugUtilsMessengerEXT(
      vk_state->instance, debug_messenger_create_info, nullptr, &vk_state->debug_messenger
    ) != VK_SUCCESS
  ) {
    logs::fatal("Could not set up debug messenger.");
  }
}


void vulkan::init(VkState *vk_state) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info{};

  if (USE_VALIDATION) {
    if (!ensure_validation_layers_supported()) {
      logs::fatal("Could not get required validation layers.");
    }

    debug_messenger_create_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_create_info.messageSeverity =
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | */
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_create_info.pfnUserCallback = debug_callback;
  }


  create_instance(vk_state, &debug_messenger_create_info);
  init_debug_messenger(vk_state, &debug_messenger_create_info);
}


void vulkan::destroy(VkState *vk_state) {
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger, nullptr);
  }
  vkDestroyInstance(vk_state->instance, nullptr);
}

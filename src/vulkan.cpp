#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"


constexpr u32 const MAX_N_REQUIRED_EXTENSIONS = 256;
constexpr u32 const MAX_N_PHYSICAL_DEVICES = 8;
constexpr u32 const MAX_N_QUEUE_FAMILIES = 64;
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
  if (
    strstr(p_callback_data->pMessage, "invalid layer manifest file version") != nullptr
  ) {
    return VK_FALSE;
  }
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
  assert(*n_required_extensions < MAX_N_REQUIRED_EXTENSIONS);
}


static bool create_instance(
  VkState *vk_state,
  VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_create_info
) {
  // Initialise info about our application (its name etc.)
  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Peony",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "peony",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
  };

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


static QueueFamilyIndices get_queue_family_indices(VkPhysicalDevice physical_device) {
  QueueFamilyIndices indices = {};
  VkQueueFamilyProperties queue_families[MAX_N_QUEUE_FAMILIES];
  u32 n_queue_families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_families, nullptr);
  assert(n_queue_families < MAX_N_QUEUE_FAMILIES);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_families, queue_families);

  range (0, n_queue_families) {
    VkQueueFamilyProperties *family = &queue_families[idx];
    if (family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.idx_graphics_family_queue = idx;
    }
  }

  return indices;
}


static bool is_physical_device_suitable(
  VkPhysicalDevice physical_device, QueueFamilyIndices queue_family_indices
) {
  return queue_family_indices.idx_graphics_family_queue.has_value();
}


static void pick_physical_device(VkState *vk_state) {
  vk_state->physical_device = VK_NULL_HANDLE;

  VkPhysicalDevice physical_devices[MAX_N_PHYSICAL_DEVICES];
  u32 n_devices = 0;
  vkEnumeratePhysicalDevices(vk_state->instance, &n_devices, nullptr);
  if (n_devices == 0) {
    logs::fatal("Could not find any physical devices.");
  }
  assert(n_devices < MAX_N_PHYSICAL_DEVICES);
  vkEnumeratePhysicalDevices(vk_state->instance, &n_devices, physical_devices);

  QueueFamilyIndices queue_family_indices;

  range (0, n_devices) {
    VkPhysicalDevice *device = &physical_devices[idx];
    queue_family_indices = get_queue_family_indices(*device);
    if (is_physical_device_suitable(*device, queue_family_indices)) {
      vk_state->physical_device = *device;
      vk_state->queue_family_indices = queue_family_indices;
    }
  }

  if (vk_state->physical_device == VK_NULL_HANDLE) {
    logs::fatal("Could not find any suitable physical devices.");
  }
}


void make_logical_device(VkState *vk_state) {
  f32 queue_priorities = 1.0f;

  VkDeviceQueueCreateInfo queue_create_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .queueFamilyIndex = vk_state->queue_family_indices.idx_graphics_family_queue.value(),
    .queueCount = 1,
    .pQueuePriorities = &queue_priorities,
  };
  VkPhysicalDeviceFeatures device_features = {};
  VkDeviceCreateInfo device_create_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_create_info,
    .enabledExtensionCount = 0,
    .pEnabledFeatures = &device_features,
  };

  if (
    vkCreateDevice(
      vk_state->physical_device, &device_create_info, nullptr, &vk_state->device
    ) != VK_SUCCESS
  ) {
    logs::fatal("Could not create logical device.");
  }

  vkGetDeviceQueue(
    vk_state->device,
    vk_state->queue_family_indices.idx_graphics_family_queue.value(),
    0,
    &vk_state->graphics_queue
  );
}


void vulkan::init(VkState *vk_state) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {};

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
    debug_messenger_create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_create_info.pfnUserCallback = debug_callback;
  }


  create_instance(vk_state, &debug_messenger_create_info);
  init_debug_messenger(vk_state, &debug_messenger_create_info);
  pick_physical_device(vk_state);
  make_logical_device(vk_state);
}


void vulkan::destroy(VkState *vk_state) {
  vkDestroyDevice(vk_state->device, nullptr);
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger, nullptr);
  }
  vkDestroyInstance(vk_state->instance, nullptr);
}

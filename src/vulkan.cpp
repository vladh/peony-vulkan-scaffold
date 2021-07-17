#include <stdint.h>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "glm.hpp"
#include "vulkan.hpp"
#include "logs.hpp"
#include "intrinsics.hpp"
#include "types.hpp"
#include "constants.hpp"
#include "files.hpp"


constexpr u32 const MAX_N_REQUIRED_EXTENSIONS = 256;
constexpr bool const USE_VALIDATION = true;
constexpr char const * const VALIDATION_LAYERS[] = {"VK_LAYER_KHRONOS_validation"};
constexpr char const * const REQUIRED_DEVICE_EXTENSIONS[] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  #if PLATFORM & PLATFORM_MACOS
    "VK_KHR_portability_subset",
  #endif
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
    n_required_extensions);
  range (0, *n_required_extensions) {
    required_extensions[idx] = glfw_extensions[idx];
  }
  if (USE_VALIDATION) {
    required_extensions[(*n_required_extensions)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  }
  #if PLATFORM & PLATFORM_MACOS
    required_extensions[(*n_required_extensions)++] =
      "VK_KHR_get_physical_device_properties2";
  #endif
  assert(*n_required_extensions <= MAX_N_REQUIRED_EXTENSIONS);
}


static bool init_instance(
  VkState *vk_state,
  VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_create_info
) {
  // Initialise info about our application (its name etc.)
  VkApplicationInfo const app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pApplicationName = "Peony",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "peony",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_0,
  };

  // Initialise other creation parameters such as required extensions
  VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
  };

  char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS];
  u32 n_required_extensions;
  get_required_extensions(required_extensions, &n_required_extensions);
  create_info.enabledExtensionCount = n_required_extensions;
  create_info.ppEnabledExtensionNames = required_extensions;

  // Set validation layer creation options
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
    1, n_available_layers * sizeof(VkLayerProperties));
  defer { free(available_layers); };
  vkEnumerateInstanceLayerProperties(&n_available_layers, available_layers);

  // Compare with desired layers
  u32 const n_validation_layers = LEN(VALIDATION_LAYERS);
  range_named (idx_desired, 0, n_validation_layers) {
    bool did_find_layer = false;
    range_named (idx_available, 0, n_available_layers) {
      if (
        pstr_eq(
          available_layers[idx_available].layerName, VALIDATION_LAYERS[idx_desired])
      ) {
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
  auto const func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkCreateDebugUtilsMessengerEXT");
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
  auto const func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkDestroyDebugUtilsMessengerEXT");
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
      vk_state->instance, debug_messenger_create_info,
      nullptr, &vk_state->debug_messenger) != VK_SUCCESS
  ) {
    logs::fatal("Could not set up debug messenger.");
  }
}


static QueueFamilyIndices get_queue_family_indices(
  VkPhysicalDevice physical_device, VkSurfaceKHR surface
) {
  constexpr u32 const MAX_N_QUEUE_FAMILIES = 64;
  QueueFamilyIndices indices = {};
  VkQueueFamilyProperties queue_families[MAX_N_QUEUE_FAMILIES];
  u32 n_queue_families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &n_queue_families, nullptr);
  assert(n_queue_families <= MAX_N_QUEUE_FAMILIES);
  vkGetPhysicalDeviceQueueFamilyProperties(
    physical_device, &n_queue_families, queue_families);

  range (0, n_queue_families) {
    VkQueueFamilyProperties *family = &queue_families[idx];
    VkBool32 supports_present = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(
      physical_device, idx, surface, &supports_present);
    if (family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.idx_graphics_family_queue = idx;
    }
    if (supports_present) {
      indices.idx_present_family_queue = idx;
    }
  }

  return indices;
}


static bool are_queue_family_indices_complete(QueueFamilyIndices queue_family_indices) {
  return queue_family_indices.idx_graphics_family_queue.has_value() &&
    queue_family_indices.idx_present_family_queue.has_value();
}


static bool are_required_extensions_supported(VkPhysicalDevice physical_device) {
  constexpr u32 const MAX_N_EXTENSIONS = 512;
  VkExtensionProperties supported_extensions[MAX_N_EXTENSIONS];
  u32 n_supported_extensions;
  vkEnumerateDeviceExtensionProperties(
    physical_device, nullptr, &n_supported_extensions, nullptr);
  assert(n_supported_extensions <= MAX_N_EXTENSIONS);
  vkEnumerateDeviceExtensionProperties(
    physical_device, nullptr, &n_supported_extensions, supported_extensions);

  range_named (idx_required, 0, LEN(REQUIRED_DEVICE_EXTENSIONS)) {
    bool did_find_extension = false;
    range_named (idx_supported, 0, n_supported_extensions) {
      if (
        pstr_eq(
          supported_extensions[idx_supported].extensionName,
          REQUIRED_DEVICE_EXTENSIONS[idx_required])
      ) {
        did_find_extension = true;
        break;
      }
    }
    if (!did_find_extension) {
      return false;
    }
  }

  return true;
}


static void init_swapchain_support_details(
  SwapChainSupportDetails *details, VkPhysicalDevice physical_device, VkSurfaceKHR surface
) {
  *details = {};

  // Capabilities
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    physical_device, surface, &details->capabilities);

  // Formats
  vkGetPhysicalDeviceSurfaceFormatsKHR(
    physical_device, surface, &details->n_formats, nullptr);
  if (details->n_formats != 0) {
    vkGetPhysicalDeviceSurfaceFormatsKHR(
      physical_device, surface, &details->n_formats, details->formats);
  }

  // Present modes
  vkGetPhysicalDeviceSurfacePresentModesKHR(
    physical_device, surface, &details->n_present_modes, nullptr);
  if (details->n_present_modes != 0) {
    vkGetPhysicalDeviceSurfacePresentModesKHR(
      physical_device, surface, &details->n_present_modes, details->present_modes);
  }
}


static void print_physical_device_info(
  VkPhysicalDevice physical_device,
  QueueFamilyIndices queue_family_indices,
  SwapChainSupportDetails *swapchain_support_details
) {
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physical_device, &properties);
  logs::info("Found physical device: %s (%d)", properties.deviceName, physical_device);
  logs::info("  Queue families");
  logs::info("    idx_graphics_family_queue: %d",
    (
      queue_family_indices.idx_graphics_family_queue.has_value() ?
      queue_family_indices.idx_graphics_family_queue.value() :
      -1
    ));
  logs::info("    idx_present_family_queue: %d",
    (
      queue_family_indices.idx_present_family_queue.has_value() ?
      queue_family_indices.idx_present_family_queue.value() :
      -1
    ));
  logs::info("  Swap chain support");
  logs::info("    Capabilities");
  logs::info("      minImageCount: %d",
    swapchain_support_details->capabilities.minImageCount);
  logs::info("      maxImageCount: %d",
    swapchain_support_details->capabilities.maxImageCount);
  logs::info("      currentExtent: %d x %d",
    swapchain_support_details->capabilities.currentExtent.width,
    swapchain_support_details->capabilities.currentExtent.height);
  logs::info("      minImageExtent: %d x %d",
    swapchain_support_details->capabilities.minImageExtent.width,
    swapchain_support_details->capabilities.minImageExtent.height);
  logs::info("      maxImageExtent: %d x %d",
    swapchain_support_details->capabilities.maxImageExtent.width,
    swapchain_support_details->capabilities.maxImageExtent.height);
  logs::info("      ...");
  logs::info("    Formats (%d)", swapchain_support_details->n_formats);
  range (0, swapchain_support_details->n_formats) {
    logs::info("      %d", swapchain_support_details->formats[idx].format);
  }
  logs::info("    Present modes (%d)", swapchain_support_details->n_present_modes);
  range (0, swapchain_support_details->n_present_modes) {
    logs::info("      %d", swapchain_support_details->present_modes[idx]);
  }
}


static bool is_physical_device_suitable(
  VkPhysicalDevice physical_device,
  QueueFamilyIndices queue_family_indices,
  SwapChainSupportDetails *swapchain_support_details
) {
  return are_queue_family_indices_complete(queue_family_indices) &&
    are_required_extensions_supported(physical_device) &&
    swapchain_support_details->n_formats > 0 &&
    swapchain_support_details->n_present_modes > 0;
}


static void init_physical_device(VkState *vk_state) {
  // Get all physical devices
  vk_state->physical_device = VK_NULL_HANDLE;

  constexpr u32 const MAX_N_PHYSICAL_DEVICES = 8;
  VkPhysicalDevice physical_devices[MAX_N_PHYSICAL_DEVICES];
  u32 n_devices = 0;
  vkEnumeratePhysicalDevices(vk_state->instance, &n_devices, nullptr);
  if (n_devices == 0) {
    logs::fatal("Could not find any physical devices.");
  }
  assert(n_devices < MAX_N_PHYSICAL_DEVICES);
  vkEnumeratePhysicalDevices(vk_state->instance, &n_devices, physical_devices);

  // Check which physical device we actually want
  QueueFamilyIndices queue_family_indices;
  range (0, n_devices) {
    VkPhysicalDevice *physical_device = &physical_devices[idx];
    queue_family_indices = get_queue_family_indices(*physical_device, vk_state->surface);
    SwapChainSupportDetails swapchain_support_details = {};
    init_swapchain_support_details(
      &swapchain_support_details, *physical_device, vk_state->surface);
    print_physical_device_info(
      *physical_device, queue_family_indices, &swapchain_support_details);
    if (
      is_physical_device_suitable(
        *physical_device, queue_family_indices, &swapchain_support_details)
    ) {
      vk_state->physical_device = *physical_device;
      vk_state->queue_family_indices = queue_family_indices;
      vk_state->swapchain_support_details = swapchain_support_details;
    }
  }

  if (vk_state->physical_device == VK_NULL_HANDLE) {
    logs::fatal("Could not find any suitable physical devices.");
  }
}


static void init_logical_device(VkState *vk_state) {
  constexpr u32 const MAX_N_QUEUES = 32;
  VkDeviceQueueCreateInfo queue_create_infos[MAX_N_QUEUES];
  u32 n_queue_create_infos = 0;
  u32 potential_queues[2] = {
    vk_state->queue_family_indices.idx_graphics_family_queue.value(),
    vk_state->queue_family_indices.idx_present_family_queue.value(),
  };
  u32 const n_potential_queues = 2;
  f32 const queue_priorities = 1.0f;

  range (0, n_potential_queues) {
    u32 const potential_queue = potential_queues[idx];
    bool is_already_created = false;
    range_named (idx_existing, 0, n_queue_create_infos) {
      if (queue_create_infos[idx_existing].queueFamilyIndex == potential_queue) {
        is_already_created = true;
      }
    }
    if (is_already_created) {
      break;
    }
    queue_create_infos[n_queue_create_infos++] = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = potential_queue,
      .queueCount = 1,
      .pQueuePriorities = &queue_priorities,
    };
  }
  VkPhysicalDeviceFeatures device_features = {};
  VkDeviceCreateInfo device_create_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = n_queue_create_infos,
    .pQueueCreateInfos = queue_create_infos,
    .enabledExtensionCount = LEN(REQUIRED_DEVICE_EXTENSIONS),
    .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS,
    .pEnabledFeatures = &device_features,
  };

  if (
    vkCreateDevice(vk_state->physical_device, &device_create_info, nullptr,
      &vk_state->device) != VK_SUCCESS
  ) {
    logs::fatal("Could not create logical device.");
  }

  vkGetDeviceQueue(
    vk_state->device,
    vk_state->queue_family_indices.idx_graphics_family_queue.value(),
    0,
    &vk_state->graphics_queue);
  vkGetDeviceQueue(
    vk_state->device,
    vk_state->queue_family_indices.idx_present_family_queue.value(),
    0,
    &vk_state->present_queue);
}


static void init_surface(VkState *vk_state, GLFWwindow *window) {
  if (
    glfwCreateWindowSurface(vk_state->instance, window, nullptr, &vk_state->surface) !=
    VK_SUCCESS
  ) {
    logs::fatal("Could not create window surface.");
  }
}


static VkSurfaceFormatKHR choose_swap_surface_format(SwapChainSupportDetails *details) {
  range (0, details->n_formats) {
    VkSurfaceFormatKHR candidate = details->formats[idx];
    if (
      candidate.format == VK_FORMAT_B8G8R8A8_SRGB &&
      candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    ) {
      return candidate;
    }
  }
  return details->formats[0];
}


static VkPresentModeKHR choose_swap_present_mode(SwapChainSupportDetails *details) {
  range (0, details->n_present_modes) {
    VkPresentModeKHR candidate = details->present_modes[idx];
    if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
      return candidate;
    }
  }

  // Guaranteed to always be available.
  return VK_PRESENT_MODE_FIFO_KHR;
}


static VkExtent2D choose_swap_extent(
  SwapChainSupportDetails *details, GLFWwindow *window
) {
  if (details->capabilities.currentExtent.width != UINT32_MAX) {
    return details->capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D extent = {(u32)width, (u32)height};
    extent.width = clamp(
      extent.width,
      details->capabilities.minImageExtent.width,
      details->capabilities.maxImageExtent.width);
    extent.height = clamp(
      extent.height,
      details->capabilities.minImageExtent.height,
      details->capabilities.maxImageExtent.height);

    return extent;
  }
}


static void init_swapchain(VkState *vk_state, GLFWwindow *window) {
  SwapChainSupportDetails *details = &vk_state->swapchain_support_details;
  VkSurfaceCapabilitiesKHR *capabilities = &details->capabilities;
  QueueFamilyIndices *indices = &vk_state->queue_family_indices;

  VkSurfaceFormatKHR surface_format = choose_swap_surface_format(details);
  VkPresentModeKHR present_mode = choose_swap_present_mode(details);
  VkExtent2D extent = choose_swap_extent(details, window);

  // Just get one more than the minimum. We can probably tune this later.
  u32 image_count = capabilities->minImageCount + 1;
  if (capabilities->maxImageCount > 0 && image_count > capabilities->maxImageCount) {
    image_count = capabilities->maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchain_create_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = vk_state->surface,
    .minImageCount = image_count,
    .imageFormat = surface_format.format,
    .imageColorSpace = surface_format.colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .preTransform = capabilities->currentTransform,
    // We're not trying to do transparency in the windowing system.
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = present_mode,
    // We don't care about the colors of pixels obscured by other windows.
    .clipped = VK_TRUE,
    // We're not recreating an old swapchain.
    .oldSwapchain = VK_NULL_HANDLE,
  };

  u32 queue_family_indices[] = {
    indices->idx_graphics_family_queue.value(), indices->idx_present_family_queue.value()
  };

  if (indices->idx_graphics_family_queue != indices->idx_present_family_queue) {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount = LEN(queue_family_indices);
    swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (
    vkCreateSwapchainKHR(
      vk_state->device, &swapchain_create_info, nullptr, &vk_state->swapchain) !=
    VK_SUCCESS
  ) {
    logs::fatal("Could not create swapchain.");
  }

  vkGetSwapchainImagesKHR(
    vk_state->device, vk_state->swapchain, &vk_state->n_swapchain_images, nullptr);
  vkGetSwapchainImagesKHR(
    vk_state->device, vk_state->swapchain, &vk_state->n_swapchain_images,
    vk_state->swapchain_images);

  vk_state->swapchain_image_format = surface_format.format;
  vk_state->swapchain_extent = extent;
}


static void init_image_views(VkState *vk_state) {
  range (0, vk_state->n_swapchain_images) {
    VkImageView *image_view = &vk_state->swapchain_image_views[idx];

    VkImageViewCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = vk_state->swapchain_images[idx],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = vk_state->swapchain_image_format,
      .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
    };

    if (
      vkCreateImageView(vk_state->device, &create_info, nullptr, image_view) !=
      VK_SUCCESS
    ) {
      logs::fatal("Could not create image views.");
    }
  }
}


static VkShaderModule init_shader_module(
  VkState *vk_state, u8 const *shader, size_t size
) {
  VkShaderModuleCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = size,
    .pCode = (u32*)shader,
  };

  VkShaderModule shader_module;
  if (
    vkCreateShaderModule(vk_state->device, &create_info, nullptr, &shader_module) !=
    VK_SUCCESS
  ) {
    logs::fatal("Could not create shader module.");
  }

  return shader_module;
}


static void init_pipeline(VkState *vk_state) {
  MemoryPool pool = {};

  size_t vert_shader_size;
  u8 *vert_shader = files::load_file_to_pool_u8(
    &pool, "bin/shaders/test.vert.spv", &vert_shader_size);

  size_t frag_shader_size;
  u8 *frag_shader = files::load_file_to_pool_u8(
    &pool, "bin/shaders/test.frag.spv", &frag_shader_size);

  logs::info("vert_shader size: %d", vert_shader_size);
  logs::info("frag_shader size: %d", frag_shader_size);
  logs::info("pool.used: %d", pool.used);

  VkShaderModule vert_shader_module = init_shader_module(
    vk_state, vert_shader, vert_shader_size);
  VkShaderModule frag_shader_module = init_shader_module(
    vk_state, frag_shader, frag_shader_size);



  vkDestroyShaderModule(vk_state->device, vert_shader_module, nullptr);
  vkDestroyShaderModule(vk_state->device, frag_shader_module, nullptr);
}


void vulkan::init(VkState *vk_state, GLFWwindow *window) {
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


  logs::info("Creating instance");
  init_instance(vk_state, &debug_messenger_create_info);
  logs::info("Creating debug messenger");
  init_debug_messenger(vk_state, &debug_messenger_create_info);
  logs::info("Creating surface");
  init_surface(vk_state, window);
  logs::info("Creating physical device");
  init_physical_device(vk_state);
  logs::info("Creating logical device");
  init_logical_device(vk_state);
  logs::info("Creating swapchain");
  init_swapchain(vk_state, window);
  logs::info("Creating swapchain image views");
  init_image_views(vk_state);
  logs::info("Creating pipeline");
  init_pipeline(vk_state);
}


void vulkan::destroy(VkState *vk_state) {
  range (0, vk_state->n_swapchain_images) {
    vkDestroyImageView(vk_state->device, vk_state->swapchain_image_views[idx], nullptr);
  }
  vkDestroySwapchainKHR(vk_state->device, vk_state->swapchain, nullptr);
  vkDestroyDevice(vk_state->device, nullptr);
  if (USE_VALIDATION) {
    DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger, nullptr);
  }
  vkDestroySurfaceKHR(vk_state->instance, vk_state->surface, nullptr);
  vkDestroyInstance(vk_state->instance, nullptr);
}

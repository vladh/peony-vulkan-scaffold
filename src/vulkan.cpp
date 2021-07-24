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
  VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_info
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
  VkInstanceCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
  };

  char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS];
  u32 n_required_extensions;
  get_required_extensions(required_extensions, &n_required_extensions);
  ci.enabledExtensionCount = n_required_extensions;
  ci.ppEnabledExtensionNames = required_extensions;

  // Set validation layer creation options
  if (USE_VALIDATION) {
    ci.enabledLayerCount = LEN(VALIDATION_LAYERS);
    ci.ppEnabledLayerNames = VALIDATION_LAYERS;
    ci.pNext = debug_messenger_info;
  } else {
    ci.enabledLayerCount = 0;
  }

  if (vkCreateInstance(&ci, nullptr, &vk_state->instance) != VK_SUCCESS) {
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
  const VkDebugUtilsMessengerCreateInfoEXT *p_info,
  const VkAllocationCallbacks *p_allocator,
  VkDebugUtilsMessengerEXT *p_debug_messenger
) {
  auto const func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
    instance, "vkCreateDebugUtilsMessengerEXT");
  if (func == nullptr) {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return func(instance, p_info, p_allocator, p_debug_messenger);
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
  VkDebugUtilsMessengerCreateInfoEXT *debug_messenger_info
) {
  if (!USE_VALIDATION) {
    return;
  }

  if (
    CreateDebugUtilsMessengerEXT(
      vk_state->instance, debug_messenger_info,
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
      indices.graphics = idx;
    }
    if (supports_present) {
      indices.present = idx;
    }
  }

  return indices;
}


static bool are_queue_family_indices_complete(QueueFamilyIndices queue_family_indices) {
  return queue_family_indices.graphics != NO_QUEUE_FAMILY &&
    queue_family_indices.present != NO_QUEUE_FAMILY;
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
  logs::info("    graphics: %d", queue_family_indices.graphics);
  logs::info("    present: %d", queue_family_indices.present);
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
  VkDeviceQueueCreateInfo queue_cis[MAX_N_QUEUES];
  u32 n_queue_cis = 0;
  u32 potential_queues[2] = {
    (u32)vk_state->queue_family_indices.graphics,
    (u32)vk_state->queue_family_indices.present,
  };
  u32 const n_potential_queues = 2;
  f32 const queue_priorities = 1.0f;

  range (0, n_potential_queues) {
    u32 const potential_queue = potential_queues[idx];
    bool is_already_created = false;
    range_named (idx_existing, 0, n_queue_cis) {
      if (queue_cis[idx_existing].queueFamilyIndex == potential_queue) {
        is_already_created = true;
      }
    }
    if (is_already_created) {
      break;
    }
    queue_cis[n_queue_cis++] = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = potential_queue,
      .queueCount = 1,
      .pQueuePriorities = &queue_priorities,
    };
  }
  VkPhysicalDeviceFeatures device_features = {};
  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = n_queue_cis,
    .pQueueCreateInfos = queue_cis,
    .enabledExtensionCount = LEN(REQUIRED_DEVICE_EXTENSIONS),
    .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS,
    .pEnabledFeatures = &device_features,
  };

  if (
    vkCreateDevice(vk_state->physical_device, &device_info, nullptr,
      &vk_state->device) != VK_SUCCESS
  ) {
    logs::fatal("Could not create logical device.");
  }

  vkGetDeviceQueue(
    vk_state->device,
    vk_state->queue_family_indices.graphics,
    0,
    &vk_state->graphics_queue);
  vkGetDeviceQueue(
    vk_state->device,
    vk_state->queue_family_indices.present,
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

  VkSwapchainCreateInfoKHR swapchain_info = {
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
    (u32)indices->graphics, (u32)indices->present
  };

  if (indices->graphics != indices->present) {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = LEN(queue_family_indices);
    swapchain_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (
    vkCreateSwapchainKHR(
      vk_state->device, &swapchain_info, nullptr, &vk_state->swapchain) !=
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

    VkImageViewCreateInfo ci = {
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
      vkCreateImageView(vk_state->device, &ci, nullptr, image_view) !=
      VK_SUCCESS
    ) {
      logs::fatal("Could not create image views.");
    }
  }
}


static VkShaderModule init_shader_module(
  VkState *vk_state, u8 const *shader, size_t size
) {
  VkShaderModuleCreateInfo ci = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = size,
    .pCode = (u32*)shader,
  };

  VkShaderModule shader_module;
  if (
    vkCreateShaderModule(vk_state->device, &ci, nullptr, &shader_module) !=
    VK_SUCCESS
  ) {
    logs::fatal("Could not create shader module.");
  }

  return shader_module;
}


static void init_render_pass(VkState *vk_state) {
  VkAttachmentDescription color_attachment = {
    .format = vk_state->swapchain_image_format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };
  VkAttachmentReference color_attachment_ref = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };
  VkSubpassDescription subpass = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment_ref,
  };
  VkSubpassDependency dependency = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo render_pass_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &color_attachment,
    .subpassCount = 1,
    .pSubpasses = &subpass,
    .dependencyCount = 1,
    .pDependencies = &dependency,
  };

  if (
    vkCreateRenderPass(vk_state->device, &render_pass_info, nullptr,
      &vk_state->render_pass) != VK_SUCCESS
  ) {
    logs::fatal("Could not create render pass.");
  }
}


static void init_pipeline(VkState *vk_state) {
  MemoryPool pool = {};

  size_t vert_shader_size;
  u8 *vert_shader = files::load_file_to_pool_u8(
    &pool, "bin/shaders/test.vert.spv", &vert_shader_size);
  VkShaderModule vert_shader_module = init_shader_module(
    vk_state, vert_shader, vert_shader_size);
  VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vert_shader_module,
    .pName = "main",
  };

  size_t frag_shader_size;
  u8 *frag_shader = files::load_file_to_pool_u8(
    &pool, "bin/shaders/test.frag.spv", &frag_shader_size);
  VkShaderModule frag_shader_module = init_shader_module(
    vk_state, frag_shader, frag_shader_size);
  VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = frag_shader_module,
    .pName = "main",
  };

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_stage_info,
    frag_shader_stage_info,
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = nullptr,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = nullptr,
  };
  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE,
  };
  VkViewport viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = (f32)vk_state->swapchain_extent.width,
    .height = (f32)vk_state->swapchain_extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };
  VkRect2D scissor = {
    .offset = {0, 0},
    .extent = vk_state->swapchain_extent,
  };
  VkPipelineViewportStateCreateInfo viewport_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &viewport,
    .scissorCount = 1,
    .pScissors = &scissor,
  };
  VkPipelineRasterizationStateCreateInfo rasterizer_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .lineWidth = 1.0f,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
  };
  VkPipelineMultisampleStateCreateInfo multisampling_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
  };
  VkPipelineColorBlendAttachmentState color_blend_attachment = {
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_TRUE,
    .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .alphaBlendOp = VK_BLEND_OP_ADD,
  };
  VkPipelineColorBlendStateCreateInfo color_blending_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachment,
  };

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };

  if (
    vkCreatePipelineLayout(vk_state->device, &pipeline_layout_info, nullptr,
      &vk_state->pipeline_layout) != VK_SUCCESS
  ) {
    logs::fatal("Could not create pipeline layout.");
  }

  VkGraphicsPipelineCreateInfo pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shader_stages,
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_state_info,
    .pRasterizationState = &rasterizer_info,
    .pMultisampleState = &multisampling_info,
    .pDepthStencilState = nullptr,
    .pColorBlendState = &color_blending_info,
    .pDynamicState = nullptr,
    .layout = vk_state->pipeline_layout,
    .renderPass = vk_state->render_pass,
    .subpass = 0,
  };

  if (
    vkCreateGraphicsPipelines(vk_state->device, VK_NULL_HANDLE, 1, &pipeline_info,
      nullptr, &vk_state->pipeline) != VK_SUCCESS
  ) {
    logs::fatal("Could not create graphics pipeline.");
  }

  vkDestroyShaderModule(vk_state->device, vert_shader_module, nullptr);
  vkDestroyShaderModule(vk_state->device, frag_shader_module, nullptr);
}


static void init_framebuffers(VkState *vk_state) {
  range (0, vk_state->n_swapchain_images) {
    VkImageView attachments[] = { vk_state->swapchain_image_views[idx] };
    VkFramebufferCreateInfo framebuffer_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = vk_state->render_pass,
      .attachmentCount = 1,
      .pAttachments = attachments,
      .width = vk_state->swapchain_extent.width,
      .height = vk_state->swapchain_extent.height,
      .layers = 1,
    };
    if (
      vkCreateFramebuffer(vk_state->device, &framebuffer_info, nullptr,
        &vk_state->swapchain_framebuffers[idx]) != VK_SUCCESS
    ) {
      logs::fatal("Could not create framebuffer.");
    }
  }
}


static void init_command_pool(VkState *vk_state) {
  VkCommandPoolCreateInfo pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = (u32)vk_state->queue_family_indices.graphics,
  };

  if (
    vkCreateCommandPool(
      vk_state->device, &pool_info, nullptr, &vk_state->command_pool) != VK_SUCCESS
  ) {
    logs::fatal("Could not create command pool.");
  }
}


static void init_command_buffers(VkState *vk_state) {
  VkCommandBufferAllocateInfo alloc_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = vk_state->command_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = vk_state->n_swapchain_images,
  };

  if (
    vkAllocateCommandBuffers(vk_state->device, &alloc_info, vk_state->command_buffers)
    != VK_SUCCESS
  ) {
    logs::fatal("Could not allocate command buffers.");
  }

  range (0, vk_state->n_swapchain_images) {
    VkCommandBuffer command_buffer = vk_state->command_buffers[idx];

    VkCommandBufferBeginInfo buffer_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };

    if (vkBeginCommandBuffer(command_buffer, &buffer_info) != VK_SUCCESS) {
      logs::fatal("Could not begin recording command buffer.");
    }

    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderpass_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = vk_state->render_pass,
      .framebuffer = vk_state->swapchain_framebuffers[idx],
      .renderArea.offset = {0, 0},
      .renderArea.extent = vk_state->swapchain_extent,
      .clearValueCount = 1,
      .pClearValues = &clear_color,
    };

    vkCmdBeginRenderPass(command_buffer, &renderpass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
      vk_state->pipeline);
    vkCmdDraw(command_buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);
    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
      logs::fatal("Could not record command buffer.");
    }
  }
}


static void init_semaphores(VkState *vk_state) {
  VkSemaphoreCreateInfo semaphore_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };

  if (
    vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
      &vk_state->image_available) != VK_SUCCESS ||
    vkCreateSemaphore(vk_state->device, &semaphore_info, nullptr,
      &vk_state->render_finished) != VK_SUCCESS
  ) {
    logs::fatal("Could not create semaphores.");
  }
}


void vulkan::init(VkState *vk_state, GLFWwindow *window) {
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {};

  if (USE_VALIDATION) {
    if (!ensure_validation_layers_supported()) {
      logs::fatal("Could not get required validation layers.");
    }

    debug_messenger_info.sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debug_messenger_info.messageSeverity =
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | */
      /* VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | */
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug_messenger_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug_messenger_info.pfnUserCallback = debug_callback;
  }


  logs::info("Creating instance");
  init_instance(vk_state, &debug_messenger_info);
  logs::info("Creating debug messenger");
  init_debug_messenger(vk_state, &debug_messenger_info);
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
  logs::info("Creating render pass");
  init_render_pass(vk_state);
  logs::info("Creating pipeline");
  init_pipeline(vk_state);
  logs::info("Creating framebuffers");
  init_framebuffers(vk_state);
  logs::info("Creating command pool");
  init_command_pool(vk_state);
  logs::info("Creating command buffers");
  init_command_buffers(vk_state);
  logs::info("Creating semaphones");
  init_semaphores(vk_state);
}


void vulkan::destroy(VkState *vk_state) {
  vkDestroySemaphore(vk_state->device, vk_state->render_finished, nullptr);
  vkDestroySemaphore(vk_state->device, vk_state->image_available, nullptr);
  vkDestroyCommandPool(vk_state->device, vk_state->command_pool, nullptr);
  range (0, vk_state->n_swapchain_images) {
    vkDestroyFramebuffer(vk_state->device, vk_state->swapchain_framebuffers[idx],
      nullptr);
  }
  vkDestroyPipeline(vk_state->device, vk_state->pipeline, nullptr);
  vkDestroyPipelineLayout(vk_state->device, vk_state->pipeline_layout, nullptr);
  vkDestroyRenderPass(vk_state->device, vk_state->render_pass, nullptr);
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


void vulkan::render(VkState *vk_state) {
  u32 idx_image;
  vkAcquireNextImageKHR(vk_state->device, vk_state->swapchain, UINT64_MAX,
    vk_state->image_available, VK_NULL_HANDLE, &idx_image);

  VkSemaphore wait_semaphores[] = { vk_state->image_available };
  VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
  VkSemaphore signal_semaphores[] = { vk_state->render_finished };

  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = wait_semaphores,
    .pWaitDstStageMask = wait_stages,
    .commandBufferCount = 1,
    .pCommandBuffers = &vk_state->command_buffers[idx_image],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signal_semaphores,
  };

  if (
    vkQueueSubmit(vk_state->graphics_queue, 1, &submit_info,
      VK_NULL_HANDLE) != VK_SUCCESS
  ) {
    logs::fatal("Could not submit draw command buffer.");
  }

  VkSwapchainKHR swapchains[] = { vk_state->swapchain };
  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signal_semaphores,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices = &idx_image,

  };
  vkQueuePresentKHR(vk_state->present_queue, &present_info);
}


void vulkan::wait(VkState *vk_state) {
  vkQueueWaitIdle(vk_state->present_queue);
  vkDeviceWaitIdle(vk_state->device);
}

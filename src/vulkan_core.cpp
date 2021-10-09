/*
  General Vulkan initialisation: instance, physical devices, logical devices,
  extensions and so on.
*/

#include <vector>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "../src_external/pstr.h"

#include "vulkan.hpp"
#include "intrinsics.hpp"
#include "logs.hpp"
#include "vkutils.hpp"


namespace vulkan::core {
  //
  // Swapchain
  //
  static VkSurfaceFormatKHR choose_swap_surface_format(SwapchainSupportDetails *details) {
    range (0, details->n_formats) {
      VkSurfaceFormatKHR candidate = details->formats[idx];
      if (candidate.format == VK_FORMAT_B8G8R8A8_SRGB && candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return candidate;
      }
    }
    return details->formats[0];
  }


  static VkPresentModeKHR choose_swap_present_mode(SwapchainSupportDetails *details) {
    range (0, details->n_present_modes) {
      VkPresentModeKHR candidate = details->present_modes[idx];
      if (candidate == VK_PRESENT_MODE_MAILBOX_KHR) {
        return candidate;
      }
    }

    // Guaranteed to always be available.
    return VK_PRESENT_MODE_FIFO_KHR;
  }


  static VkExtent2D choose_swap_extent(SwapchainSupportDetails *details, GLFWwindow *window) {
    if (details->capabilities.currentExtent.width != UINT32_MAX) {
      return details->capabilities.currentExtent;
    } else {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D extent = {(u32)width, (u32)height};
      extent.width = clamp(extent.width, details->capabilities.minImageExtent.width,
        details->capabilities.maxImageExtent.width);
      extent.height = clamp(extent.height, details->capabilities.minImageExtent.height,
        details->capabilities.maxImageExtent.height);

      return extent;
    }
  }


  static void init_support_details(
    SwapchainSupportDetails *details, VkPhysicalDevice physical_device, VkSurfaceKHR surface
  ) {
    *details = {};

    // Capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &details->capabilities);

    // Formats
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &details->n_formats, nullptr);
    if (details->n_formats != 0) {
      vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &details->n_formats, details->formats);
    }

    // Present modes
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &details->n_present_modes, nullptr);
    if (details->n_present_modes != 0) {
      vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &details->n_present_modes,
        details->present_modes);
    }
  }


  static void init_swapchain(VkState *vk_state, GLFWwindow *window, VkExtent2D *extent) {
    // Create the swapchain itself
    SwapchainSupportDetails *details       = &vk_state->swapchain_support_details;
    VkSurfaceCapabilitiesKHR *capabilities = &details->capabilities;
    QueueFamilyIndices *indices            = &vk_state->queue_family_indices;

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(details);
    VkPresentModeKHR present_mode     = choose_swap_present_mode(details);
    *extent                           = choose_swap_extent(details, window);
    logs::info("Extent is %d x %d", extent->width, extent->height);

    // Just get one more than the minimum. We can probably tune this later.
    u32 image_count = capabilities->minImageCount + 1;
    if (capabilities->maxImageCount > 0 && image_count > capabilities->maxImageCount) {
      image_count = capabilities->maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_info = {
      .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface          = vk_state->surface,
      .minImageCount    = image_count,
      .imageFormat      = surface_format.format,
      .imageColorSpace  = surface_format.colorSpace,
      .imageExtent      = *extent,
      .imageArrayLayers = 1,
      .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .preTransform     = capabilities->currentTransform,
      // We're not trying to do transparency in the windowing system.
      .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode      = present_mode,
      // We don't care about the colors of pixels obscured by other windows.
      .clipped          = VK_TRUE,
      // We're not recreating an old swapchain.
      .oldSwapchain     = VK_NULL_HANDLE,
    };

    u32 const queue_family_indices[] = {(u32)indices->graphics, (u32)indices->present};

    if (indices->graphics != indices->present) {
      // If we need to use this swapchain from two different queues, allow that
      swapchain_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
      swapchain_info.queueFamilyIndexCount = LEN(queue_family_indices);
      swapchain_info.pQueueFamilyIndices   = queue_family_indices;
    } else {
      // Otherwise, we only ever use it from one queue
      swapchain_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

    vkutils::check(vkCreateSwapchainKHR(vk_state->device, &swapchain_info, nullptr, &vk_state->swapchain));

    VkImage swapchain_images[MAX_N_SWAPCHAIN_IMAGES];
    vkGetSwapchainImagesKHR(vk_state->device, vk_state->swapchain, &vk_state->n_swapchain_images, nullptr);
    vkGetSwapchainImagesKHR(vk_state->device, vk_state->swapchain, &vk_state->n_swapchain_images, swapchain_images);

    vk_state->swapchain_image_format = surface_format.format;

    // Create image views for the swapchain
    range (0, vk_state->n_swapchain_images) {
      VkImageView *image_view = &vk_state->swapchain_image_views[idx];

      VkImageViewCreateInfo const image_view_info = {
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = swapchain_images[idx],
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = vk_state->swapchain_image_format,
        .components = {
          .r              = VK_COMPONENT_SWIZZLE_IDENTITY,
          .g              = VK_COMPONENT_SWIZZLE_IDENTITY,
          .b              = VK_COMPONENT_SWIZZLE_IDENTITY,
          .a              = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
          .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel   = 0,
          .levelCount     = 1,
          .baseArrayLayer = 0,
          .layerCount     = 1,
        },
      };

      vkutils::check(vkCreateImageView(vk_state->device, &image_view_info, nullptr, image_view));
    }
  }


  //
  // Core
  //
  static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* p_callback_data,
    void* p_user_data
  ) {
    if (strstr(p_callback_data->pMessage, "invalid layer manifest file version") != nullptr) {
      return VK_FALSE;
    }
    logs::info("(Validation layer) %s", p_callback_data->pMessage);
    return VK_FALSE;
  }


  static void get_required_extensions(
    char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS], u32 *n_required_extensions
  ) {
    char const **glfw_extensions = glfwGetRequiredInstanceExtensions( n_required_extensions);
    range (0, *n_required_extensions) {
      required_extensions[idx] = glfw_extensions[idx];
    }
    if (USE_VALIDATION) {
      required_extensions[(*n_required_extensions)++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    }
    #if PLATFORM & PLATFORM_MACOS
      required_extensions[(*n_required_extensions)++] = "VK_KHR_get_physical_device_properties2";
    #endif
    assert(*n_required_extensions <= MAX_N_REQUIRED_EXTENSIONS);
  }


  static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *p_info,
    const VkAllocationCallbacks *p_allocator,
    VkDebugUtilsMessengerEXT *p_debug_messenger
  ) {
    auto const func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
      "vkCreateDebugUtilsMessengerEXT");
    if (func == nullptr) {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return func(instance, p_info, p_allocator, p_debug_messenger);
  }


  static void DestroyDebugUtilsMessengerEXT(
    VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* p_allocator
  ) {
    auto const func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance,
      "vkDestroyDebugUtilsMessengerEXT");
    if (func == nullptr) {
      return;
    }
    func(instance, debug_messenger, p_allocator);
  }


  static bool ensure_validation_layers_supported() {
    // Get available layers
    u32 n_available_layers;
    vkEnumerateInstanceLayerProperties(&n_available_layers, nullptr);

    std::vector<VkLayerProperties> available_layers(n_available_layers);
    vkEnumerateInstanceLayerProperties(&n_available_layers, available_layers.data());

    // Compare with desired layers
    for (auto desired_layer : VALIDATION_LAYERS) {
      bool did_find_layer = false;
      for (auto available_layer : available_layers) {
        if (pstr_eq(available_layer.layerName, desired_layer)) {
          did_find_layer = true;
          break;
        }
      }
      if (!did_find_layer) {
        logs::error("Could not find validation layer: %s", desired_layer);
        return false;
      }
    }

    return true;
  }


  static void init_instance(VkState *vk_state) {
    // Debug messenger info
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debug_callback,
    };

    if (USE_VALIDATION) {
      if (!ensure_validation_layers_supported()) {
        logs::fatal("Could not get required validation layers.");
      }
    }

    // Initialise info about our application (its name etc.)
    VkApplicationInfo const app_info = {
      .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName   = "Peony",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName        = "peony",
      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion         = VK_API_VERSION_1_0,
    };

    // Initialise other creation parameters such as required extensions
    char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS];
    u32 n_required_extensions;
    get_required_extensions(required_extensions, &n_required_extensions);
    VkInstanceCreateInfo instance_info = {
      .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo        = &app_info,
      .enabledExtensionCount   = n_required_extensions,
      .ppEnabledExtensionNames = required_extensions,
    };

    // Set validation layer creation options
    if (USE_VALIDATION) {
      instance_info.enabledLayerCount   = (u32)VALIDATION_LAYERS.size();
      instance_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
      instance_info.pNext               = &debug_messenger_info;
    } else {
      instance_info.enabledLayerCount   = 0;
    }

    vkutils::check(vkCreateInstance(&instance_info, nullptr, &vk_state->instance));

    // Init debug messenger
    if (USE_VALIDATION) {
      vkutils::check(CreateDebugUtilsMessengerEXT(vk_state->instance, &debug_messenger_info, nullptr,
        &vk_state->debug_messenger));
    }
  }


  static QueueFamilyIndices get_queue_families(
    VkPhysicalDevice physical_device,
    u32 *n_queue_families,
    VkQueueFamilyProperties *queue_families,
    VkSurfaceKHR surface
  ) {
    QueueFamilyIndices indices = {
      .graphics = NO_QUEUE_FAMILY,
      .present  = NO_QUEUE_FAMILY,
      .transfer = NO_QUEUE_FAMILY,
    };
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, n_queue_families, nullptr);
    assert(*n_queue_families <= MAX_N_QUEUE_FAMILIES);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, n_queue_families, queue_families);

    range (0, *n_queue_families) {
      VkQueueFamilyProperties *family = &queue_families[idx];
      VkBool32 supports_present = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, idx, surface, &supports_present);
      // Graphics queue
      if (family->queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphics = idx;
      }
      // Present queue
      if (supports_present) {
        indices.present = idx;
      }
      // Transfer queue
      // NOTE: This check might be too restrictive
      if (
        family->queueFlags & VK_QUEUE_TRANSFER_BIT &&
        !(family->queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        !(supports_present)
      ) {
        indices.transfer = idx;
      }
    }

    return indices;
  }


  static bool are_queue_family_indices_complete(QueueFamilyIndices indices) {
    // We don't need the transfer family right now!
    return indices.graphics != NO_QUEUE_FAMILY &&
      indices.present != NO_QUEUE_FAMILY;
  }


  static bool are_required_extensions_supported(VkPhysicalDevice physical_device) {
    u32 n_supported_extensions;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &n_supported_extensions, nullptr);

    std::vector<VkExtensionProperties> supported_extensions( n_supported_extensions);
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &n_supported_extensions, supported_extensions.data());

    for (auto required_extension : REQUIRED_DEVICE_EXTENSIONS) {
      bool did_find_extension = false;
      for (auto supported_extension : supported_extensions) {
        if (pstr_eq(supported_extension.extensionName, required_extension)) {
          did_find_extension = true;
          break;
        }
      }
      if (!did_find_extension) {
        logs::warning("Unsupported required extension: %s", required_extension);
        return false;
      }
    }

    return true;
  }


  static void print_physical_device_info(
    VkPhysicalDevice physical_device,
    QueueFamilyIndices queue_family_indices,
    SwapchainSupportDetails *swapchain_support_details
  ) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    logs::info("Found physical device: %s", properties.deviceName);
    logs::info("  Queue families");
    logs::info("    graphics: %d", queue_family_indices.graphics);
    logs::info("    present: %d", queue_family_indices.present);
    logs::info("    transfer: %d", queue_family_indices.transfer);
    logs::info("  Swap chain support");
    logs::info("    Capabilities");
    logs::info("      minImageCount: %d", swapchain_support_details->capabilities.minImageCount);
    logs::info("      maxImageCount: %d", swapchain_support_details->capabilities.maxImageCount);
    logs::info("      currentExtent: %d x %d", swapchain_support_details->capabilities.currentExtent.width,
      swapchain_support_details->capabilities.currentExtent.height);
    logs::info("      minImageExtent: %d x %d", swapchain_support_details->capabilities.minImageExtent.width,
      swapchain_support_details->capabilities.minImageExtent.height);
    logs::info("      maxImageExtent: %d x %d", swapchain_support_details->capabilities.maxImageExtent.width,
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


  static void print_logical_device_info(
    VkQueue graphics_queue,
    VkQueue present_queue,
    VkQueue asset_queue
  ) {
    logs::info("Logical device:");
    logs::info("  Queues");
    logs::info("    graphics_queue: %d", graphics_queue);
    logs::info("    present_queue: %d", present_queue);
    logs::info("    asset_queue: %d", asset_queue);
  }


  static bool is_physical_device_suitable(
    VkPhysicalDevice physical_device,
    QueueFamilyIndices queue_family_indices,
    SwapchainSupportDetails *swapchain_support_details
  ) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physical_device, &properties);
    logs::info("Testing physical device: %s", properties.deviceName);

    VkPhysicalDeviceFeatures supported_features;
    vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

    if (!are_queue_family_indices_complete(queue_family_indices)) {
      logs::info("...but queue family indices were not complete");
      return false;
    }
    if (!are_required_extensions_supported(physical_device)) {
      logs::info("...but required extensions were not supported");
      return false;
    }
    if (!(swapchain_support_details->n_formats > 0)) {
      logs::info("...but there were no available swapchain formats");
      return false;
    }
    if (!(swapchain_support_details->n_present_modes > 0)) {
      logs::info("...but there were no available present modes");
      return false;
    }
    if (!supported_features.samplerAnisotropy) {
      logs::info("...but sampler anisotropy was not supported");
      return false;
    }

    logs::info("Physical device is suitable");
    return true;
  }


  static void init_physical_device(VkState *vk_state) {
    vk_state->physical_device = VK_NULL_HANDLE;

    // Get all physical devices
    constexpr u32 MAX_N_PHYSICAL_DEVICES = 8;
    VkPhysicalDevice physical_devices[MAX_N_PHYSICAL_DEVICES];
    u32 n_devices = 0;
    vkEnumeratePhysicalDevices(vk_state->instance, &n_devices, nullptr);
    if (n_devices == 0) {
      logs::fatal("Could not find any physical devices.");
    }
    assert(n_devices < MAX_N_PHYSICAL_DEVICES);
    vkEnumeratePhysicalDevices(vk_state->instance, &n_devices, physical_devices);

    // Check which physical device we actually want
    range (0, n_devices) {
      VkPhysicalDevice *physical_device = &physical_devices[idx];
      QueueFamilyIndices queue_family_indices = get_queue_families(*physical_device, &vk_state->n_queue_families,
        vk_state->queue_families, vk_state->surface);
      SwapchainSupportDetails swapchain_support_details = {};
      init_support_details(&swapchain_support_details, *physical_device, vk_state->surface);
      print_physical_device_info(*physical_device, queue_family_indices, &swapchain_support_details);

      if (is_physical_device_suitable(*physical_device, queue_family_indices, &swapchain_support_details)) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(*physical_device, &properties);
        logs::info("Using physical device: %s", properties.deviceName);
        vk_state->physical_device           = *physical_device;
        vk_state->swapchain_support_details = swapchain_support_details;
        vk_state->queue_family_indices      = queue_family_indices;
        break;
      }
    }

    if (vk_state->physical_device == VK_NULL_HANDLE) {
      logs::fatal("Could not find any suitable physical devices.");
    }

    vkGetPhysicalDeviceProperties(vk_state->physical_device,
      &vk_state->physical_device_properties);
  }


  static void init_logical_device(VkState *vk_state) {
    f32 const queue_priorities[3] = {1.0f, 1.0f, 1.0f};

    if (vk_state->n_queue_families == 1) {
      // If we only have a single queue family, we need to take all of our queues out of that family
      if (vk_state->queue_families[0].queueCount == 1) {
        // If we have a single queue family and only a single queue in it, we're in a super annoying situation :(
        // This happens on Intel integrated GPUs
        // In this case, all our queues are just the same one single queue that we can make
        // This means we can't submit to queues from multiple threads...oof!!!
        VkDeviceQueueCreateInfo queue_infos[1] = {
          {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = (u32)vk_state->queue_family_indices.graphics,
            .queueCount       = 1,
            .pQueuePriorities = queue_priorities,
          },
        };
        VkPhysicalDeviceFeatures const device_features = {.samplerAnisotropy = VK_TRUE};
        VkDeviceCreateInfo const device_info = {
          .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
          .queueCreateInfoCount    = 1,
          .pQueueCreateInfos       = queue_infos,
          .enabledExtensionCount   = (u32)REQUIRED_DEVICE_EXTENSIONS.size(),
          .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
          .pEnabledFeatures        = &device_features,
        };

        vkutils::check(vkCreateDevice(vk_state->physical_device, &device_info, nullptr, &vk_state->device));

        VkQueue our_only_queue_oof;
        // A single queue, from the graphics queue family
        vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.graphics, 0, &our_only_queue_oof);
        vk_state->graphics_queue = our_only_queue_oof;
        vk_state->present_queue = our_only_queue_oof;
        vk_state->asset_queue = our_only_queue_oof;
      } else {
        // If we have a single queue family but we can make multiple queues out of it, just make all of our queues here
        VkDeviceQueueCreateInfo queue_infos[1] = {
          {
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = (u32)vk_state->queue_family_indices.graphics,
            .queueCount       = 3,
            .pQueuePriorities = queue_priorities,
          },
        };
        VkPhysicalDeviceFeatures const device_features = {.samplerAnisotropy = VK_TRUE};
        VkDeviceCreateInfo const device_info = {
          .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
          .queueCreateInfoCount    = 1,
          .pQueueCreateInfos       = queue_infos,
          .enabledExtensionCount   = (u32)REQUIRED_DEVICE_EXTENSIONS.size(),
          .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
          .pEnabledFeatures        = &device_features,
        };

        vkutils::check(vkCreateDevice(vk_state->physical_device, &device_info, nullptr, &vk_state->device));
        // Graphics queue from the graphics queue family
        vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.graphics, 0, &vk_state->graphics_queue);
        // Present queue, also from the graphics queue family
        vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.graphics, 0, &vk_state->present_queue);
        // Asset queue, also from the graphics queue family
        vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.graphics, 0, &vk_state->asset_queue);
      }
    } else {
      // If we support multiple queue families, we can just get all of our queues out of the respective family

      // We don't currently handle the case if somehow the graphics family and the present family is the same
      if ((u32)vk_state->queue_family_indices.graphics == (u32)vk_state->queue_family_indices.present) {
        logs::error("We have an as of yet unsupported queue arrangement, this should be fixed in the code here");
      }

      VkDeviceQueueCreateInfo queue_infos[2] = {
        // Graphics and asset queues
        {
          .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = (u32)vk_state->queue_family_indices.graphics,
          .queueCount       = 2,
          .pQueuePriorities = queue_priorities,
        },
        // Present queue
        {
          .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = (u32)vk_state->queue_family_indices.present,
          .queueCount       = 1,
          .pQueuePriorities = queue_priorities,
        },
      };
      VkPhysicalDeviceFeatures const device_features = {.samplerAnisotropy = VK_TRUE};
      VkDeviceCreateInfo const device_info = {
        .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = 2,
        .pQueueCreateInfos       = queue_infos,
        .enabledExtensionCount   = (u32)REQUIRED_DEVICE_EXTENSIONS.size(),
        .ppEnabledExtensionNames = REQUIRED_DEVICE_EXTENSIONS.data(),
        .pEnabledFeatures        = &device_features,
      };

      vkutils::check(vkCreateDevice(vk_state->physical_device, &device_info, nullptr, &vk_state->device));
      // Graphics queue from the graphics queue family
      vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.graphics, 0, &vk_state->graphics_queue);
      // Present queue from the present queue family
      vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.present, 0, &vk_state->present_queue);
      // Asset queue from the graphics queue family (because the graphics queue family can do everything)
      vkGetDeviceQueue(vk_state->device, (u32)vk_state->queue_family_indices.graphics, 1, &vk_state->asset_queue);
    }

    print_logical_device_info(vk_state->graphics_queue, vk_state->present_queue, vk_state->asset_queue);
  }


  static void init_surface(VkState *vk_state, GLFWwindow *window) {
    vkutils::check(glfwCreateWindowSurface(vk_state->instance, window, nullptr, &vk_state->surface));
  }


  static void init_descriptor_pool(VkState *vk_state) {
    constexpr u32 n_max_sets = 1000;
    constexpr VkDescriptorPoolSize descriptor_pool_sizes[] = {
      vkutils::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100),
      vkutils::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100),
    };
    auto const pool_info = vkutils::descriptor_pool_create_info(n_max_sets, LEN(descriptor_pool_sizes),
      descriptor_pool_sizes);
    vkutils::check(vkCreateDescriptorPool(vk_state->device, &pool_info, nullptr, &vk_state->descriptor_pool));
  }


  static void init(VkState *vk_state, GLFWwindow *window, VkExtent2D *extent) {
    init_instance(vk_state);
    init_surface(vk_state, window);
    init_physical_device(vk_state);
    init_logical_device(vk_state);
    init_descriptor_pool(vk_state);
    init_swapchain(vk_state, window, extent);
  }


  static void destroy(VkState *vk_state) {
    vkDestroyDescriptorPool(vk_state->device, vk_state->descriptor_pool, nullptr);
    vkDestroyDevice(vk_state->device, nullptr);
    if (USE_VALIDATION) {
      core::DestroyDebugUtilsMessengerEXT(vk_state->instance, vk_state->debug_messenger, nullptr);
    }
    vkDestroySurfaceKHR(vk_state->instance, vk_state->surface, nullptr);
    vkDestroyInstance(vk_state->instance, nullptr);
  }
}

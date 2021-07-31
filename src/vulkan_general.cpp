/*
  General Vulkan initialisation: instance, physical devices, logical devices,
  extensions and so on.
*/


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
  char const *required_extensions[MAX_N_REQUIRED_EXTENSIONS];
  u32 n_required_extensions;
  get_required_extensions(required_extensions, &n_required_extensions);
  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pApplicationInfo = &app_info,
    .enabledExtensionCount = n_required_extensions,
    .ppEnabledExtensionNames = required_extensions,
  };

  // Set validation layer creation options
  if (USE_VALIDATION) {
    instance_info.enabledLayerCount = LEN(VALIDATION_LAYERS);
    instance_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    instance_info.pNext = debug_messenger_info;
  } else {
    instance_info.enabledLayerCount = 0;
  }

  if (vkCreateInstance(&instance_info, nullptr, &vk_state->instance) != VK_SUCCESS) {
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
    CreateDebugUtilsMessengerEXT(vk_state->instance, debug_messenger_info,
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
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
    &n_queue_families, queue_families);

  range (0, n_queue_families) {
    VkQueueFamilyProperties *family = &queue_families[idx];
    VkBool32 supports_present = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,
      idx, surface, &supports_present);
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
  vkEnumerateDeviceExtensionProperties(physical_device,
    nullptr, &n_supported_extensions, nullptr);
  assert(n_supported_extensions <= MAX_N_EXTENSIONS);
  vkEnumerateDeviceExtensionProperties(physical_device,
    nullptr, &n_supported_extensions, supported_extensions);

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


static void print_physical_device_info(
  VkPhysicalDevice physical_device,
  QueueFamilyIndices queue_family_indices,
  SwapchainSupportDetails *swapchain_support_details
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
  SwapchainSupportDetails *swapchain_support_details
) {
  VkPhysicalDeviceFeatures supported_features;
  vkGetPhysicalDeviceFeatures(physical_device, &supported_features);

  return are_queue_family_indices_complete(queue_family_indices) &&
    are_required_extensions_supported(physical_device) &&
    swapchain_support_details->n_formats > 0 &&
    swapchain_support_details->n_present_modes > 0 &&
    supported_features.samplerAnisotropy;
}


static void init_physical_device(VkState *vk_state) {
  vk_state->physical_device = VK_NULL_HANDLE;

  // Get all physical devices
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
  range (0, n_devices) {
    VkPhysicalDevice *physical_device = &physical_devices[idx];

    QueueFamilyIndices queue_family_indices = get_queue_family_indices(
      *physical_device, vk_state->surface);

    SwapchainSupportDetails swapchain_support_details = {};
    init_swapchain_support_details(&swapchain_support_details,
      *physical_device, vk_state->surface);

    print_physical_device_info(*physical_device,
      queue_family_indices, &swapchain_support_details);

    if (
      is_physical_device_suitable(*physical_device,
        queue_family_indices, &swapchain_support_details)
    ) {
      vk_state->physical_device = *physical_device;
      vk_state->queue_family_indices = queue_family_indices;
      vk_state->swapchain_support_details = swapchain_support_details;
    }
  }

  if (vk_state->physical_device == VK_NULL_HANDLE) {
    logs::fatal("Could not find any suitable physical devices.");
  }

  vkGetPhysicalDeviceProperties(vk_state->physical_device,
    &vk_state->physical_device_properties);
}


static void init_logical_device(VkState *vk_state) {
  constexpr u32 const MAX_N_QUEUES = 32;
  VkDeviceQueueCreateInfo queue_infos[MAX_N_QUEUES];
  u32 n_queue_infos = 0;
  u32 const potential_queues[] = {
    (u32)vk_state->queue_family_indices.graphics,
    (u32)vk_state->queue_family_indices.present,
  };
  f32 const queue_priorities = 1.0f;

  range (0, LEN(potential_queues)) {
    u32 const potential_queue = potential_queues[idx];
    bool is_already_created = false;
    range_named (idx_existing, 0, n_queue_infos) {
      // Compare potential_queue (a handle) with existing_queue.queueFamilyIndex
      // (also a handle)
      if (queue_infos[idx_existing].queueFamilyIndex == potential_queue) {
        is_already_created = true;
      }
    }
    if (is_already_created) {
      break;
    }
    queue_infos[n_queue_infos++] = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = potential_queue,
      .queueCount = 1,
      .pQueuePriorities = &queue_priorities,
    };
  }
  VkPhysicalDeviceFeatures const device_features = {.samplerAnisotropy = VK_TRUE};
  VkDeviceCreateInfo const device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .queueCreateInfoCount = n_queue_infos,
    .pQueueCreateInfos = queue_infos,
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

  vkGetDeviceQueue(vk_state->device,
    vk_state->queue_family_indices.graphics,
    0,
    &vk_state->graphics_queue);
  vkGetDeviceQueue(vk_state->device,
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


static void init_command_pool(VkState *vk_state) {
  VkCommandPoolCreateInfo const pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = (u32)vk_state->queue_family_indices.graphics,
  };

  if (
    vkCreateCommandPool(vk_state->device, &pool_info, nullptr,
      &vk_state->command_pool) != VK_SUCCESS
  ) {
    logs::fatal("Could not create command pool.");
  }
}
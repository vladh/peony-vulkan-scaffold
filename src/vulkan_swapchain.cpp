/*
  Functions that handle swapchain creation.
*/


static VkSurfaceFormatKHR choose_swap_surface_format(SwapchainSupportDetails *details) {
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


static VkExtent2D choose_swap_extent(
  SwapchainSupportDetails *details, GLFWwindow *window
) {
  if (details->capabilities.currentExtent.width != UINT32_MAX) {
    return details->capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D extent = {(u32)width, (u32)height};
    extent.width = clamp(extent.width,
      details->capabilities.minImageExtent.width,
      details->capabilities.maxImageExtent.width);
    extent.height = clamp(extent.height,
      details->capabilities.minImageExtent.height,
      details->capabilities.maxImageExtent.height);

    return extent;
  }
}


static void init_swapchain_support_details(
  SwapchainSupportDetails *details,
  VkPhysicalDevice physical_device,
  VkSurfaceKHR surface
) {
  *details = {};

  // Capabilities
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device,
    surface, &details->capabilities);

  // Formats
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,
    surface, &details->n_formats, nullptr);
  if (details->n_formats != 0) {
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device,
      surface, &details->n_formats, details->formats);
  }

  // Present modes
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,
    surface, &details->n_present_modes, nullptr);
  if (details->n_present_modes != 0) {
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device,
      surface, &details->n_present_modes, details->present_modes);
  }
}


static void init_swapchain(VkState *vk_state, GLFWwindow *window) {
  // Create the swapchain itself
  SwapchainSupportDetails *details = &vk_state->swapchain_support_details;
  VkSurfaceCapabilitiesKHR *capabilities = &details->capabilities;
  QueueFamilyIndices *indices = &vk_state->queue_family_indices;

  VkSurfaceFormatKHR surface_format = choose_swap_surface_format(details);
  VkPresentModeKHR present_mode = choose_swap_present_mode(details);
  VkExtent2D extent = choose_swap_extent(details, window);
  logs::info("Extent is %d x %d", extent.width, extent.height);

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

  u32 const queue_family_indices[] = {
    (u32)indices->graphics, (u32)indices->present
  };

  if (indices->graphics != indices->present) {
    // If we need to use this swapchain from two different queues, allow that
    swapchain_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_info.queueFamilyIndexCount = LEN(queue_family_indices);
    swapchain_info.pQueueFamilyIndices = queue_family_indices;
  } else {
    // Otherwise, we only ever use it from one queue
    swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  if (
    vkCreateSwapchainKHR(vk_state->device, &swapchain_info, nullptr,
      &vk_state->swapchain) != VK_SUCCESS
  ) {
    logs::fatal("Could not create swapchain.");
  }

  vkGetSwapchainImagesKHR(vk_state->device,
    vk_state->swapchain, &vk_state->n_swapchain_images, nullptr);
  vkGetSwapchainImagesKHR(vk_state->device,
    vk_state->swapchain, &vk_state->n_swapchain_images, vk_state->swapchain_images);

  vk_state->swapchain_image_format = surface_format.format;
  vk_state->swapchain_extent = extent;

  // Create image views for the swapchain
  range (0, vk_state->n_swapchain_images) {
    VkImageView *image_view = &vk_state->swapchain_image_views[idx];

    VkImageViewCreateInfo const image_view_info = {
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
      vkCreateImageView(vk_state->device, &image_view_info, nullptr,
        image_view) != VK_SUCCESS
    ) {
      logs::fatal("Could not create image views.");
    }
  }
}

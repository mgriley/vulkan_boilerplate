#include "app.h"
#include "utils.h"
/*
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
*/

#include <chrono>
#include <thread>
#include <utility>
#include <tuple>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;
using namespace glm;

struct AppState {
  GLFWwindow* win = nullptr;

  AppState();
};

AppState::AppState()
{
}

vector<char> read_file(const string& filename) {
  ifstream file(filename, ios::ate | ios::binary);
  size_t file_size = (size_t) file.tellg();
  vector<char> buffer(file_size);
  file.seekg(0);
  file.read(buffer.data(), file_size);
  file.close();
  return buffer;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData) {

  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    printf("validation layer: %s\n", pCallbackData->pMessage);
  }

  return VK_FALSE;
}

VkShaderModule create_shader_module(VkDevice& device, const vector<char>& code) {
  VkShaderModuleCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    .codeSize = code.size(),
    .pCode = reinterpret_cast<const uint32_t*>(code.data())
  };
  VkShaderModule module;
  VkResult res = vkCreateShaderModule(device, &create_info, nullptr, &module);
  assert(res == VK_SUCCESS);
  return module;
}

void init_vulkan(AppState& state) {
  VkResult res;
  
  // enumerate instance extensions
  uint32_t num_exts = 0;
  res = vkEnumerateInstanceExtensionProperties(nullptr, &num_exts, nullptr);
  vector<VkExtensionProperties> ext_props{num_exts};
  res = vkEnumerateInstanceExtensionProperties(nullptr,
      &num_exts, ext_props.data());
  printf("exts:\n");
  for (int i = 0; i < num_exts; ++i) {
    printf("%s\n", ext_props[i].extensionName);
  }
  printf("\n");

  // enumerate layers
  uint32_t num_layers = 0;
  res = vkEnumerateInstanceLayerProperties(
      &num_layers, nullptr);
  vector<VkLayerProperties> layer_props{num_layers};
  res = vkEnumerateInstanceLayerProperties(
      &num_layers, layer_props.data());
  printf("layers:\n");
  for (int i = 0; i < num_layers; ++i) {
    printf("%s\n", layer_props[i].layerName);
  }
  printf("\n");

  // gather extensions
  uint32_t glfw_ext_count = 0;
  const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  vector<const char*> ext_names;
  ext_names.insert(ext_names.end(), glfw_exts, glfw_exts + glfw_ext_count);
  ext_names.push_back("VK_EXT_debug_utils");

  // gather layers
  vector<const char*> layer_names = {
    "VK_LAYER_KHRONOS_validation"
  };

  // setup instance
  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = nullptr,
    .pApplicationName = "my_vulkan_app",
    .applicationVersion = 1,
    .pEngineName = "my_vulkan_app",
    .engineVersion = 1,
    .apiVersion = VK_API_VERSION_1_0
  };
    VkInstanceCreateInfo inst_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .pApplicationInfo = &app_info,
    .enabledExtensionCount = static_cast<uint32_t>(ext_names.size()),
    .ppEnabledExtensionNames = ext_names.data(),
    .enabledLayerCount = static_cast<uint32_t>(layer_names.size()),
    .ppEnabledLayerNames = layer_names.data()
  };
  VkInstance inst;
  res = vkCreateInstance(&inst_info, nullptr, &inst);
  if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
    printf("cant find a compatible vulkan ICD\n");
    exit(1);
  } else if (res) {
    printf("Error occurred on create instance\n");
    exit(1);
  }

  // setup debug callback
  VkDebugUtilsMessengerCreateInfoEXT debug_utils_info = {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = vulkan_debug_callback,
    .pUserData = nullptr
  };
  VkDebugUtilsMessengerEXT debug_messenger;
  auto create_debug_utils = (PFN_vkCreateDebugUtilsMessengerEXT)
    vkGetInstanceProcAddr(inst, "vkCreateDebugUtilsMessengerEXT");
  auto destroy_debug_utils = (PFN_vkDestroyDebugUtilsMessengerEXT)
    vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
  assert(create_debug_utils);
  assert(destroy_debug_utils);
  res = create_debug_utils(inst, &debug_utils_info, nullptr, &debug_messenger);
  assert(res == VK_SUCCESS);

  // init surface
  VkSurfaceKHR surface;
  res = glfwCreateWindowSurface(inst, state.win, nullptr, &surface);
  assert(res == VK_SUCCESS);

  // retrieve physical device
  // assume the first GPU will do

  uint32_t device_count = 1;
  VkPhysicalDevice phys_device = {};
  res = vkEnumeratePhysicalDevices(inst, &device_count, &phys_device);
  assert(!res && device_count == 1);

  // init device

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_family_count,
      nullptr);
  assert(queue_family_count > 0);
  vector<VkQueueFamilyProperties> queue_fam_props{queue_family_count};
  vkGetPhysicalDeviceQueueFamilyProperties(phys_device, &queue_family_count,
      queue_fam_props.data());
  assert(queue_family_count > 0);

  bool found_index = false;
  uint32_t target_family_index = 0;
  printf("queue families:\n");
  for (int i = 0; i < queue_family_count; ++i) {
    auto& q_fam = queue_fam_props[i];
    bool supports_graphics = q_fam.queueFlags & VK_QUEUE_GRAPHICS_BIT;
    bool supports_compute = q_fam.queueFlags & VK_QUEUE_COMPUTE_BIT;
    VkBool32 supports_present = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(phys_device, i, surface, &supports_present);
    printf("G: %i, C: %i, P: %d, count: %d\n", supports_graphics ? 1 : 0,
        supports_compute ? 1 : 0, supports_present, q_fam.queueCount);

    if (supports_graphics && supports_compute && supports_present) {
      target_family_index = i;
      found_index = true;
    }
  }
  printf("\n");
  assert(found_index);

  float queue_priority = 1.0f;
  VkDeviceQueueCreateInfo queue_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .pNext = nullptr,
    .queueFamilyIndex = target_family_index,
    .queueCount = 1,
    .pQueuePriorities = &queue_priority
  };

  vector<const char*> device_ext_names = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };
  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = nullptr,
    .queueCreateInfoCount = 1,
    .pQueueCreateInfos = &queue_info,
    .enabledExtensionCount = static_cast<uint32_t>(device_ext_names.size()),
    .ppEnabledExtensionNames = device_ext_names.data(),
    .enabledLayerCount = 0,
    .ppEnabledLayerNames = nullptr,
    .pEnabledFeatures = nullptr
  };
  VkDevice device;
  res = vkCreateDevice(phys_device, &device_info, nullptr, &device);
  assert(res == VK_SUCCESS);

  // retrieve our queue
  VkQueue queue;
  vkGetDeviceQueue(device, target_family_index, 0, &queue);

  // query surface properties
  VkSurfaceCapabilitiesKHR surface_caps;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_device, surface,
      &surface_caps);
  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count,
      nullptr);
  vector<VkSurfaceFormatKHR> surface_formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(phys_device, surface, &format_count,
      surface_formats.data());
  uint32_t present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface,
      &present_mode_count, nullptr);
  vector<VkPresentModeKHR> present_modes(present_mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(phys_device, surface,
      &present_mode_count, present_modes.data());
  assert(format_count > 0 && present_mode_count > 0);
  printf("surface formats: %d, surface present modes: %d\n",
      format_count, present_mode_count);

  VkSurfaceFormatKHR target_format;
  bool found_format = false;
  for (int i = 0; i < format_count; ++i) {
    auto& format = surface_formats[i];
    if (format.format == VK_FORMAT_UNDEFINED ||
        (format.format == VK_FORMAT_B8G8R8A8_UNORM &&
         format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)) {
      target_format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
      found_format = true;
      break;
    }
  }
  assert(found_format);

  // guarenteed to be supported?
  VkPresentModeKHR target_present_mode = VK_PRESENT_MODE_FIFO_KHR;

  VkExtent2D target_extent = surface_caps.currentExtent;
  uint32_t target_image_count = surface_caps.minImageCount + 1;
  printf("target extent w: %d, h %d\n", target_extent.width, target_extent.height);
  printf("image count min: %d, max: %d\n",
      surface_caps.minImageCount, surface_caps.maxImageCount);

  // setup the swapchain
  VkSwapchainCreateInfoKHR swapchain_info = {
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = surface,
    .minImageCount = target_image_count,
    .imageFormat = target_format.format,
    .imageColorSpace = target_format.colorSpace,
    .imageExtent = target_extent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0,
    .pQueueFamilyIndices = nullptr,
    .preTransform = surface_caps.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = target_present_mode,
    .oldSwapchain = VK_NULL_HANDLE
  };
  VkSwapchainKHR swapchain;
  res = vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain);
  assert(res == VK_SUCCESS);

  // retrieve the swapchain images
  uint32_t swapchain_img_count = 0;
  vkGetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, nullptr);
  vector<VkImage> swapchain_images(swapchain_img_count);
  vkGetSwapchainImagesKHR(device, swapchain, &swapchain_img_count, swapchain_images.data());

  // create image views
  vector<VkImageView> swapchain_img_views(swapchain_images.size());
  for (int i = 0; i < swapchain_images.size(); ++i) {
    VkImageViewCreateInfo img_view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = swapchain_images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = target_format.format,
      .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1
    };
    res = vkCreateImageView(device, &img_view_info, nullptr, &swapchain_img_views[i]);
    assert(res == VK_SUCCESS);
  }

  // create render pass

  VkSubpassDependency dependency = {
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
  };
  VkAttachmentDescription color_attachment = {
    .format = target_format.format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  };
  VkAttachmentReference color_attachment_ref = {
    .attachment = 0,
    .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
  };
  VkSubpassDescription subpass_desc = {
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .colorAttachmentCount = 1,
    .pColorAttachments = &color_attachment_ref
  };
  VkRenderPassCreateInfo render_pass_info = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .attachmentCount = 1,
    .pAttachments = &color_attachment,
    .subpassCount = 1,
    .pSubpasses = &subpass_desc,
    .dependencyCount = 1,
    .pDependencies = &dependency
  };
  VkRenderPass render_pass;
  res = vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass);
  assert(res == VK_SUCCESS);

  // create graphics pipeline

  auto vert_shader_code = read_file("../shaders/vert.spv");
  auto frag_shader_code = read_file("../shaders/frag.spv");
  VkShaderModule vert_module = create_shader_module(device, vert_shader_code);
  VkShaderModule frag_module = create_shader_module(device, frag_shader_code);

  VkPipelineShaderStageCreateInfo vert_stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_VERTEX_BIT,
    .module = vert_module,
    .pName = "main"
  };
  VkPipelineShaderStageCreateInfo frag_stage_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .module = frag_module,
    .pName = "main"
  };
  vector<VkPipelineShaderStageCreateInfo> shader_stages = {
    vert_stage_info, frag_stage_info
  };

  VkPipelineVertexInputStateCreateInfo vertex_input_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 0,
    .pVertexBindingDescriptions = nullptr,
    .vertexAttributeDescriptionCount = 0,
    .pVertexAttributeDescriptions = nullptr
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    .primitiveRestartEnable = VK_FALSE
  };

  VkViewport viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float) target_extent.width,
    .height = (float) target_extent.height,
    .minDepth = 0.0f,
    .maxDepth = 1.0f
  };
  VkRect2D scissor_rect = {
    .offset = {0, 0},
    .extent = target_extent
  };
  VkPipelineViewportStateCreateInfo viewport_state_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
    .viewportCount = 1,
    .pViewports = &viewport,
    .scissorCount = 1,
    .pScissors = &scissor_rect
  };
  VkPipelineRasterizationStateCreateInfo rast_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .depthClampEnable = VK_FALSE,
    .rasterizerDiscardEnable = VK_FALSE,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .lineWidth = 1.0f,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_CLOCKWISE,
    .depthBiasEnable = VK_FALSE,
    .depthBiasConstantFactor = 0.0f,
    .depthBiasClamp = 0.0f,
    .depthBiasSlopeFactor = 0.0f
  };
  VkPipelineMultisampleStateCreateInfo multisampling = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .sampleShadingEnable = VK_FALSE,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .minSampleShading = 1.0f,
    .pSampleMask = nullptr,
    .alphaToCoverageEnable = VK_FALSE,
    .alphaToOneEnable = VK_FALSE
  };
  VkPipelineColorBlendAttachmentState color_blend_attachment = {
    .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    .blendEnable = VK_FALSE,
  };
  VkPipelineColorBlendStateCreateInfo color_blending = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOpEnable = VK_FALSE,
    .attachmentCount = 1,
    .pAttachments = &color_blend_attachment
  };
  // TODO - will return to this later for specifying uniforms
  VkPipelineLayoutCreateInfo pipeline_layout_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
  };
  VkPipelineLayout pipeline_layout;
  res = vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr,
      &pipeline_layout);
  assert(res == VK_SUCCESS);

  VkGraphicsPipelineCreateInfo graphics_pipeline_info = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .stageCount = 2,
    .pStages = shader_stages.data(),
    .pVertexInputState = &vertex_input_info,
    .pInputAssemblyState = &input_assembly_info,
    .pViewportState = &viewport_state_info,
    .pRasterizationState = &rast_info,
    .pMultisampleState = &multisampling,
    .pDepthStencilState = nullptr,
    .pColorBlendState = &color_blending,
    .pDynamicState = nullptr,
    .layout = pipeline_layout,
    .renderPass = render_pass,
    .subpass = 0,
    .basePipelineHandle = VK_NULL_HANDLE,
    .basePipelineIndex = -1
  };
  VkPipeline graphics_pipeline;
  res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
      &graphics_pipeline_info, nullptr, &graphics_pipeline);

  vkDestroyShaderModule(device, vert_module, nullptr);
  vkDestroyShaderModule(device, frag_module, nullptr);

  // init the framebuffers

  vector<VkFramebuffer> swapchain_framebuffers(swapchain_img_views.size());
  for (int i = 0; i < swapchain_img_views.size(); ++i) {
    VkFramebufferCreateInfo framebuffer_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = render_pass,
      .attachmentCount = 1,
      .pAttachments = &swapchain_img_views[i],
      .width = target_extent.width,
      .height = target_extent.height,
      .layers = 1
    };
    res = vkCreateFramebuffer(device, &framebuffer_info, nullptr,
        &swapchain_framebuffers[i]);
    assert(res == VK_SUCCESS);
  }

  // init command pool
  VkCommandPoolCreateInfo cmd_pool_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .queueFamilyIndex = target_family_index,
    .flags = 0
  };
  VkCommandPool cmd_pool;
  res = vkCreateCommandPool(device, &cmd_pool_info, nullptr,
      &cmd_pool);
  assert(res == VK_SUCCESS);

  // init cmd buffers
  vector<VkCommandBuffer> cmd_buffers(swapchain_framebuffers.size());
  VkCommandBufferAllocateInfo cmd_buffer_info = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = cmd_pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = (uint32_t) swapchain_framebuffers.size()
  };
  res = vkAllocateCommandBuffers(device, &cmd_buffer_info,
      cmd_buffers.data());
  assert(res == VK_SUCCESS);

  // record render passes
  for (int i = 0; i < cmd_buffers.size(); ++i) {
    VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      .pInheritanceInfo = nullptr
    };
    res = vkBeginCommandBuffer(cmd_buffers[i], &begin_info);
    assert(res == VK_SUCCESS);

    VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    VkRenderPassBeginInfo render_pass_info = {
      .sType= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = render_pass,
      .framebuffer = swapchain_framebuffers[i],
      .renderArea.offset = {0, 0},
      .renderArea.extent = target_extent,
      .clearValueCount = 1,
      .pClearValues = &clear_color
    };
    vkCmdBeginRenderPass(cmd_buffers[i], &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
        graphics_pipeline);
    vkCmdDraw(cmd_buffers[i], 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd_buffers[i]);
    res = vkEndCommandBuffer(cmd_buffers[i]);
    assert(res == VK_SUCCESS);
  }

  const int max_frames_in_flight = 2;
  vector<VkSemaphore> img_available_semas(max_frames_in_flight);
  vector<VkSemaphore> render_done_semas(max_frames_in_flight);
  vector<VkFence> in_flight_fences(max_frames_in_flight);
  VkSemaphoreCreateInfo sema_info = {
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
  };
  VkFenceCreateInfo fence_info = {
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT
  };
  for (int i = 0; i < max_frames_in_flight; ++i) {
    res = vkCreateSemaphore(device, &sema_info, nullptr,
        &img_available_semas[i]);
    assert(res == VK_SUCCESS);
    res = vkCreateSemaphore(device, &sema_info, nullptr,
        &render_done_semas[i]);
    assert(res == VK_SUCCESS);
    res = vkCreateFence(device, &fence_info, nullptr,
        &in_flight_fences[i]);
    assert(res == VK_SUCCESS);
  }

  // main loop
  size_t current_frame = 0;
  while (!glfwWindowShouldClose(state.win)) {
    glfwPollEvents();

    vkWaitForFences(device, 1, &in_flight_fences[current_frame],
        VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkResetFences(device, 1, &in_flight_fences[current_frame]);
    
    uint32_t img_index;
    vkAcquireNextImageKHR(device, swapchain,
        std::numeric_limits<uint64_t>::max(),
        img_available_semas[current_frame],
        VK_NULL_HANDLE, &img_index);

    vector<VkPipelineStageFlags> wait_stages = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &img_available_semas[current_frame],
      .pWaitDstStageMask = wait_stages.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd_buffers[img_index],
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &render_done_semas[current_frame]
    };
    res = vkQueueSubmit(queue, 1, &submit_info,
        in_flight_fences[current_frame]);
    assert(res == VK_SUCCESS);

    VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &render_done_semas[current_frame],
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &img_index,
      .pResults = nullptr
    };
    vkQueuePresentKHR(queue, &present_info);

    current_frame = (current_frame + 1) % max_frames_in_flight;
  }
  vkDeviceWaitIdle(device);
  
  // cleanup
  for (int i = 0; i < max_frames_in_flight; ++i) {
    vkDestroySemaphore(device, render_done_semas[i], nullptr);
    vkDestroySemaphore(device, img_available_semas[i], nullptr);
    vkDestroyFence(device, in_flight_fences[i], nullptr);
  }
  vkDestroyCommandPool(device, cmd_pool, nullptr);
  for (VkFramebuffer& fb : swapchain_framebuffers) {
    vkDestroyFramebuffer(device, fb, nullptr);
  }
  for (VkImageView& img_view : swapchain_img_views) {
    vkDestroyImageView(device, img_view, nullptr);
  }
  vkDestroyRenderPass(device, render_pass, nullptr);
  vkDestroyPipeline(device, graphics_pipeline, nullptr);
  vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
  vkDestroySwapchainKHR(device, swapchain, nullptr);
  destroy_debug_utils(inst, debug_messenger, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(inst, surface, nullptr);
  vkDestroyInstance(inst, nullptr);
}

void init_glfw(AppState& state) {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // prevent resizing the window for now, since
  // it requires recreating the swapchain
  // TODO: https://vulkan-tutorial.com/Drawing_a_triangle/Swap_chain_recreation
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  state.win = glfwCreateWindow(800, 600, "morph",
      nullptr, nullptr);
  assert(glfwVulkanSupported() == GLFW_TRUE);
}

void main_loop(AppState& state) {
  while (!glfwWindowShouldClose(state.win)) {
    glfwPollEvents();
  }
}

void cleanup_state(AppState& state) {
  glfwDestroyWindow(state.win);
  glfwTerminate();
}

void run_app(int argc, char** argv) {
  // setup the environment for the vulkan loader
  char icd_env_entry[] = ENV_VK_ICD_FILENAMES;
  char layer_env_entry[] = ENV_VK_LAYER_PATH;
  putenv(icd_env_entry);
  putenv(layer_env_entry);

  AppState state;

  init_glfw(state);
  init_vulkan(state);
  //main_loop(state);
  cleanup_state(state);
}

/*
void run_app(int argc, char** argv) {
  signal(SIGSEGV, handle_segfault);

  // read cmd-line args
  if (argc < 2) {
    printf("Incorrect usage. Please use:\n\nexec path\n\n"
        "where \"path\" is the path to the directory containing the"
        " \"shaders\" folder. Ex: \"exec ..\"\n");
    return;
  }
  string base_shader_path(argv[1]);

  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    exit(EXIT_FAILURE);
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  int target_width = 1300;
  int target_height = 700;
  GLFWwindow* window = glfwCreateWindow(target_width, target_height, "morph", NULL, NULL);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwMakeContextCurrent(window);
  gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
  glfwSwapInterval(1);

  printf("OpenGL: %d.%d\n", GLVersion.major, GLVersion.minor);

  GraphicsState g_state(window, base_shader_path);
  setup_opengl(g_state);

  glfwSetWindowUserPointer(window, &g_state);
  glfwSetKeyCallback(window, handle_key_event);

  // setup imgui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  ImGui::StyleColorsDark();
  //ImGui::StyleColorsClassic();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  const char* glsl_version = "#version 150";
  ImGui_ImplOpenGL3_Init(glsl_version);

  bool requires_mac_mojave_fix = true;
  bool show_dev_console = true;
  // for maintaining the fps
  auto start_of_frame = chrono::steady_clock::now();
  // for logging the fps
  int fps_stat = 0;
  int frame_counter = 0;
  auto start_of_sec = start_of_frame;

  while (!glfwWindowShouldClose(window)) {
    std::this_thread::sleep_until(start_of_frame);
    auto cur_time = chrono::steady_clock::now();
    int frame_dur_millis = (int) (1000.0f / g_state.controls.target_fps);
    start_of_frame = cur_time + chrono::milliseconds(frame_dur_millis);

    // for logging the fps
    frame_counter += 1;
    if (start_of_sec < cur_time) {
      start_of_sec = cur_time + chrono::seconds(1);
      fps_stat = frame_counter;
      frame_counter = 0;
    }

    glfwPollEvents();
    update_camera(window, g_state.controls, g_state.camera);

    // the screen appears black on mojave until a resize occurs
    if (requires_mac_mojave_fix) {
      requires_mac_mojave_fix = false;
      glfwSetWindowSize(window, target_width, target_height + 1);
    }
    
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("dev console", &show_dev_console);
    ImGui::DragInt("target fps", &g_state.controls.target_fps, 1.0f, 0, 100);
    ImGui::Text("current fps: %d", fps_stat);

    Controls& controls = g_state.controls;

    ImGui::Separator();
    ImGui::Text("camera:");
    ImGui::Text("eye: %s", vec3_str(g_state.camera.pos()).c_str());
    ImGui::Text("forward: %s", vec3_str(g_state.camera.forward()).c_str());
    ImGui::Text("mode: %s", controls.cam_spherical_mode ? "spherical" : "cartesian");

    ImGui::Separator();
    ImGui::Text("render controls:");
    ImGui::Checkbox("render faces", &controls.render_faces);
    ImGui::Checkbox("render points", &controls.render_points);
    ImGui::Checkbox("render wireframe", &controls.render_wireframe);

    ImGui::Separator();
    ImGui::Text("render program controls:");
    ImGui::PushID("render");
    gen_user_uniforms_ui(g_state.render_state.prog.user_unifs);
    ImGui::PopID();

    ImGui::Separator();
    ImGui::Text("morph program controls:");
    MorphState& m_state = g_state.morph_state;

    vector<const char*> prog_names;
    for (auto& prog : g_state.morph_state.programs) {
      prog_names.push_back(prog.name.c_str());
    }
    ImGui::Combo("program", &m_state.cur_prog_index,
        prog_names.data(), prog_names.size());

    MorphProgram& cur_prog = m_state.programs[m_state.cur_prog_index];
    ImGui::PushID("morph");
    gen_user_uniforms_ui(cur_prog.user_unifs);
    ImGui::PopID();
    
    ImGui::Separator();
    ImGui::Text("simulation controls:");
    ImGui::Text("init data:");
    ImGui::InputInt("AxA samples", &controls.num_zygote_samples);
    controls.num_zygote_samples = std::max(controls.num_zygote_samples, 0);
    
    ImGui::Text("simulation:"); 
    int max_iter_num = 1*1000*1000*1000;
    ImGui::DragInt("iter num", &controls.num_iters, 0.2f, 0, max_iter_num);
    if (ImGui::Button("run once")) {
      run_simulation_pipeline(g_state);  
    }
    ImGui::Text("animation:");
    string anim_btn_text(controls.animating_sim ? "PAUSE" : "PLAY");
    if (ImGui::Button(anim_btn_text.c_str())) {
      controls.animating_sim = !controls.animating_sim;
    }
    ImGui::DragInt("start iter", &controls.start_iter_num, 10.0f, 0, max_iter_num);
    ImGui::DragInt("end iter", &controls.end_iter_num, 10.0f, controls.start_iter_num, max_iter_num);
    ImGui::DragInt("delta iters per frame", &controls.delta_iters, 0.2f, -10, 10);
    ImGui::Checkbox("loop at end", &controls.loop_at_end);
    
    // run the animation
    if (controls.animating_sim) {
      controls.num_iters += controls.delta_iters;
      controls.num_iters = clamp(controls.num_iters,
          controls.start_iter_num, controls.end_iter_num);
      if (controls.loop_at_end && controls.num_iters == controls.end_iter_num) {
        controls.num_iters = controls.start_iter_num;
      }
      run_simulation_pipeline(g_state);
    }

    ImGui::Separator();
    ImGui::Text("debug");
    ImGui::Text("Note that logging will not occur while animating");
    ImGui::Checkbox("log input nodes", &controls.log_input_nodes);
    ImGui::Checkbox("log output nodes", &controls.log_output_nodes);
    ImGui::Checkbox("log render data", &controls.log_render_data);
    ImGui::Checkbox("log durations", &controls.log_durations);
    // do not log while animating, the IO becomes a bottleneck
    if (controls.animating_sim) {
      controls.log_input_nodes = false;
      controls.log_output_nodes = false;
      controls.log_render_data = false;
      controls.log_durations = false;
    }

    ImGui::Separator();
    ImGui::Text("instructions:");
    ImGui::Text("%s", INSTRUCTIONS_STRING);

    ImGui::End();

    ImGui::Render();
    int fb_width = 0;
    int fb_height = 0;
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    
    glViewport(0, 0, fb_width, fb_height);
    glClearColor(1, 1, 1, 1);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    g_state.render_state.fb_width = fb_width;
    g_state.render_state.fb_height = fb_height;
    render_frame(g_state);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(window);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
}*/

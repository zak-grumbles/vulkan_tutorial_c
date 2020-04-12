#include "vk_app.h"
#include "utils.h"

#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYERS = false;
#else
const bool ENABLE_VALIDATION_LAYERS = true;
#endif

const int WIDTH = 800;
const int HEIGHT = 600;
const int MAX_FRAMES_IN_FLIGHT = 2;

// Validation layers
const char* VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};
const uint32_t VALIDATION_LAYER_COUNT = 1;

// Device extensions
const char* DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const uint32_t DEVICE_EXTENSIONS_COUNT = 1;

// "Private" interface
void init_window_(vk_app*);
bool init_vulkan_(vk_app*);

char** get_required_extensions_(uint32_t*);
bool init_instance_(vk_app*);

bool setup_debug_messenger_(vk_app*);
void cleanup_debug_messenger_(vk_app*);

bool create_surface_(vk_app*);

bool pick_physical_device_(vk_app*);
bool is_device_suitable_(VkPhysicalDevice, VkSurfaceKHR);
bool device_supports_exts_(VkPhysicalDevice);
queue_families find_queue_families_(VkPhysicalDevice, VkSurfaceKHR);

bool create_logical_device_(vk_app*);

bool create_render_pass_(vk_app*);
bool create_graphics_pipeline_(vk_app*);
VkShaderModule create_shader_module(vk_app*, const uint32_t*, size_t);

swapchain_details get_swapchain_support_(VkPhysicalDevice, VkSurfaceKHR);
VkSurfaceFormatKHR choose_swap_surface_format_(VkSurfaceFormatKHR*, uint32_t);
VkPresentModeKHR choose_present_mode_(VkPresentModeKHR*, uint32_t);
VkExtent2D choose_swap_extent_(const VkSurfaceCapabilitiesKHR* const capabilities);
bool create_swapchain_(vk_app*);
bool create_image_views_(vk_app*);
bool create_framebuffers_(vk_app*);

bool create_cmd_pool_(vk_app*);
bool create_cmd_buffers_(vk_app*);

bool create_sync_objects_(vk_app*);

void draw_frame_(vk_app*);

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(
        VkDebugUtilsMessageSeverityFlagBitsEXT,
        VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT*,
        void*
        );

/**
 * Initializes the vulkan app struct.
 * Params:
 *   app - vulkan app struct
 */
bool init_vk_app(vk_app* app) {
    init_window_(app);

    bool success = init_vulkan_(app);

    if(success) {
        printf("Successfully initialized vulkan");
    }
    else {
        fprintf(stderr, "Failed to initialize vulkan\n");
    }

    return success;
}

/**
 * Runs the main loop of the given vulkan app.
 * 
 * Params:
 *   app - vulkan app
 */
void run_vk_app(vk_app* app) {
    while(!glfwWindowShouldClose(app->app_window)) {
        glfwPollEvents();
        draw_frame_(app);
    }

    vkDeviceWaitIdle(app->device);
}

/**
 * Cleans up the vk app struct.
 * 
 * Params:
 *   app - vulkan app
 */
void cleanup_vk_app(vk_app* app) {

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(app->device, app->render_finished[i], NULL);
        vkDestroySemaphore(app->device, app->image_available[i], NULL);
        vkDestroyFence(app->device, app->in_flight[i], NULL);
    }
    free(app->image_available);
    app->image_available = NULL;

    free(app->render_finished);
    app->render_finished = NULL;

    free(app->in_flight);
    app->in_flight = NULL;

    free(app->imgs_in_flight);
    app->imgs_in_flight = NULL;

    vkDestroyCommandPool(app->device, app->cmd_pool, NULL);

    for(uint32_t i = 0; i < app->framebuffer_count; i++) {
        vkDestroyFramebuffer(app->device, app->framebuffers[i], NULL);
    }

    vkDestroyPipeline(app->device, app->graphics_pipeline, NULL);

    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);

    vkDestroyRenderPass(app->device, app->render_pass, NULL);

    for(uint32_t i = 0; i < app->swapchain_image_count; i++) {
        vkDestroyImageView(app->device, app->swapchain_image_views[i], NULL);
    }
    free(app->swapchain_image_views);
    app->swapchain_image_views = NULL;

    vkDestroySwapchainKHR(app->device, app->swapchain,
            NULL);

    // Images are destroyed by swapchain destruction
    free(app->swapchain_images);
    app->swapchain_images = NULL;

    vkDestroyDevice(app->device, NULL);

    vkDestroySurfaceKHR(app->instance, app->surface, NULL);

    cleanup_debug_messenger_(app);

    vkDestroyInstance(app->instance, NULL);

    glfwDestroyWindow(app->app_window);
    app->app_window = NULL;
    glfwTerminate();
}

/**
 * Initializes the GLFW window for the vulkan app.
 * 
 * Params:
 *   app - vulkan app
 */
void init_window_(vk_app* app) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    app->app_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Window", NULL, NULL);
}

/**
 * Initializes all of the necessary vulkan structs and
 * data.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   boolean indicating success
 */
bool init_vulkan_(vk_app* app) {
    bool success = true;

    app->current_frame = 0;

    success = init_instance_(app);

    if(success && ENABLE_VALIDATION_LAYERS) {
        setup_debug_messenger_(app);
    }

    if(success) success &= create_surface_(app);
    if(success) success &= pick_physical_device_(app);
    if(success) success &= create_logical_device_(app);
    if(success) success &= create_swapchain_(app);
    if(success) success &= create_image_views_(app);
    if(success) success &= create_render_pass_(app);
    if(success) success &= create_graphics_pipeline_(app);
    if(success) success &= create_framebuffers_(app);
    if(success) success &= create_cmd_pool_(app);
    if(success) success &= create_cmd_buffers_(app);
    if(success) success &= create_sync_objects_(app);

    return success;
}

/**
 * Returns the extensions required for vulkan on this machine.
 * 
 * Params:
 *   count - Set to the total number of extensions
 * 
 * Returns:
 *   array of strings with length 'count'
 */
char** get_required_extensions_(uint32_t* count) {
    uint32_t glfw_extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);

    uint32_t total_count = glfw_extensions_count;
    char** exts = NULL;

    if(ENABLE_VALIDATION_LAYERS) {
        total_count++;
    }

    exts = (char**)malloc(sizeof(char*) * total_count);

    for(uint32_t i = 0; i < glfw_extensions_count; i++) {
        size_t ext_len = strlen(glfw_extensions[i]) + 1;

        exts[i] = (char*)malloc(sizeof(char) * ext_len);
        strcpy(exts[i], glfw_extensions[i]);
    }

    if(ENABLE_VALIDATION_LAYERS) {
        size_t ext_len = strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) + 1;

        exts[total_count - 1] = (char*)malloc(sizeof(char) * ext_len);
        strcpy(exts[total_count - 1], VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    *count = total_count;

    return exts;
}

/**
 * Initializes the vulkan instance.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   bool indicating success.
 */
bool init_instance_(vk_app* app) {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    // get required extensions
    uint32_t ext_count = 0;
    char** required_exts = get_required_extensions_(&ext_count);

    int all_layers_found = 0;
    if(ENABLE_VALIDATION_LAYERS) {
        printf("Validation layers requested. Checking support\n");

        uint32_t layer_count = 0;
        vkEnumerateInstanceLayerProperties(&layer_count, NULL);

        VkLayerProperties* layers = (VkLayerProperties*)malloc(layer_count * sizeof(VkLayerProperties));
        vkEnumerateInstanceLayerProperties(&layer_count, layers);

        printf("Found %i supported layers:\n", layer_count);
        for(int i = 0; i < layer_count; i++) {
            printf("  - %s\n", layers[i].layerName);
        }

        int layers_found = 0;
        for(int i = 0; i < VALIDATION_LAYER_COUNT && layers_found < VALIDATION_LAYER_COUNT; i++) {
            for(int j = 0; j < layer_count; j++) {
                if(strcmp(VALIDATION_LAYERS[i], layers[j].layerName) == 0) {
                    layers_found++;
                    break;
                }
            }
        }
        free(layers);
        layers = NULL;

        if(layers_found != VALIDATION_LAYER_COUNT) {
            fprintf(stderr, "Requested %i validation layers, but only %i were found to be supported\n",
                    VALIDATION_LAYER_COUNT, layers_found);
        }
        else
        {
            all_layers_found = 1;
        }
    }
    else
    {
        printf("No validation layers requested");
    }

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = ext_count;
    create_info.ppEnabledExtensionNames = (const char**)required_exts;

    if(ENABLE_VALIDATION_LAYERS && all_layers_found) {
        create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    }
    else {
        create_info.enabledLayerCount = 0;
    }

    VkResult result = vkCreateInstance(&create_info, NULL, &app->instance);
    if(result != VK_SUCCESS) {
        fprintf(stderr, "Unable to initialize Vulkan instance");
    }

    // free up required extensions
    for(uint32_t i = 0; i < ext_count; i++) {
        free(required_exts[i]);
    }
    free(required_exts);

    return result == VK_SUCCESS;
}

/**
 * Sets up the debug utils messenger callback.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   bool indicating success
 */
bool setup_debug_messenger_(vk_app* app) {
    VkDebugUtilsMessengerCreateInfoEXT create_info = {};

    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_cb;
    create_info.pUserData = NULL;

    PFN_vkCreateDebugUtilsMessengerEXT func = 
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                app->instance, "vkCreateDebugUtilsMessengerEXT"
                );

    if(func != NULL) {
        VkResult result = func(app->instance, &create_info, NULL, &app->debug_messenger);

        return result == VK_SUCCESS;
    }
    else
    {
        fprintf(stderr, "Could not find debug utils func\n");
        return false;
    }
}

/**
 * Cleans up the debug messenger.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   int indicating success
 */
void cleanup_debug_messenger_(vk_app* app) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = 
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                app->instance, "vkDestroyDebugUtilsMessengerEXT"
                );

    if(func != NULL) {
        func(app->instance, app->debug_messenger, NULL);
    }
    else
    {
        fprintf(stderr, "Couldn't find func to destoy debug messenger\n");
    }
}

/**
 * Creates the window surface.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   bool indicating success
 */
bool create_surface_(vk_app* app) {
    VkResult result = glfwCreateWindowSurface(app->instance, app->app_window, NULL, &app->surface);

    if(result != VK_SUCCESS) {
        fprintf(stderr, "Failed to create window surface\n");
    }

    return result == VK_SUCCESS;
}

/**
 * Selects a physical device to use for the app.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   int indicating success
 */
bool pick_physical_device_(vk_app* app) {
    app->physical_device = VK_NULL_HANDLE;

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);

    if(device_count == 0) {
        fprintf(stderr, "Unable to find valid vulkan physical device");
        return 0;
    }

    VkPhysicalDevice* devices = (VkPhysicalDevice*)malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    printf("Found %i potential physical devices:\n", device_count);
    bool found_valid_device = false;
    for(uint32_t i = 0; i < device_count; i++) {
        if(is_device_suitable_(devices[i], app->surface)) {
            app->physical_device = devices[i];
            found_valid_device = true;
            break;
        }
    }

    free(devices);
    devices = NULL;

    return found_valid_device;
}

/**
 * Determines if a physical device meets our criteria.
 * 
 * Params:
 *   device - physical device
 * 
 * Returns:
 *   bool indicating validity
 */
bool is_device_suitable_(VkPhysicalDevice device, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    bool valid = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    queue_families families = find_queue_families_(device, surface);

    valid &= device_supports_exts_(device);
    valid &= families.is_complete;

    if(valid) {
        swapchain_details scd = get_swapchain_support_(device, surface);

        valid &= (scd.num_formats != 0 && scd.num_present_modes != 0);
    }

    printf("  - Checking device %s - %i\n", props.deviceName, valid);

    return valid;
}

/**
 * Checks if the given device supports the required
 * device extensions.
 * 
 * Params:
 *   device - Physical device handle.
 * 
 * Returns:
 *   boolean indicating support
 */
bool device_supports_exts_(VkPhysicalDevice device) {
    uint32_t ext_count = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, NULL);

    VkExtensionProperties* props = (VkExtensionProperties*)malloc(
            sizeof(VkExtensionProperties) * ext_count
            );

    vkEnumerateDeviceExtensionProperties(device, NULL, &ext_count, props);

    bool all_found = true;

    // This is gross, but will work for now since it's known that 
    // we only require 1 device extension.
    for(uint32_t i = 0; i < DEVICE_EXTENSIONS_COUNT; i++) {
        bool ext_found = false;
        for(uint32_t j = 0; j < ext_count; j++) {
            if(strcmp(DEVICE_EXTENSIONS[i], props[j].extensionName) == 0) {
                ext_found = true;
                break;
            }
        }

        all_found &= ext_found;
    }

    free(props);
    props = NULL;

    return all_found;
}

/**
 * Selects appropriate queue families for the given
 * physical device.
 * 
 * Params:
 *   device - physical device
 * 
 * Returns:
 *   queue_families struct containing family indices.
 */
queue_families find_queue_families_(
        VkPhysicalDevice device,
        VkSurfaceKHR surface
        ) {
    queue_families indices = {
        -1,     // graphics family index
        -1,     // present family index
        false   // is complete
    };

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, NULL);

    VkQueueFamilyProperties* families = (VkQueueFamilyProperties*)malloc(
            family_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families);

    printf("  - Found %i queue families\n", family_count);
    for(uint32_t i = 0; i < family_count; i++) {
        printf("    - Family %i has: %i queues, ", i, families[i].queueCount);

        if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            printf("graphics bit, ");

            if(indices.graphics_family_index == -1) {
                indices.graphics_family_index = i;
            }
        }
        else {
            printf("no graphics bit, ");
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);

        if(present_support == VK_TRUE) {
            indices.present_family_index = i;
            printf("presentation support\n");
        }
        else {
            printf("no presentation support\n");
        }

        if(indices.graphics_family_index != -1 && 
                indices.present_family_index != -1) {
            indices.is_complete = true;
            break;
        }
    }

    return indices;
}

/**
 * Gets the details of swapchain support for the given
 * physical device.
 * 
 * Params:
 *   device  - physical device
 *   surface - Render surface
 * 
 * Returns
 *   swapchain_details struct
 */
swapchain_details get_swapchain_support_(VkPhysicalDevice device, VkSurfaceKHR surface) {
    swapchain_details scd = {};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
            &scd.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &scd.num_formats,
            NULL);

    if(scd.num_formats != 0) {
        scd.formats = (VkSurfaceFormatKHR*)malloc(
                sizeof(VkSurfaceFormatKHR) * scd.num_formats);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface,
                &scd.num_formats, scd.formats);
    }

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
            &scd.num_present_modes, NULL);
    if(scd.num_present_modes != 0) {
        scd.present_modes = (VkPresentModeKHR*)malloc(
                sizeof(VkPresentModeKHR) * scd.num_present_modes);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                &scd.num_present_modes, scd.present_modes);
    }

    return scd;
}

/**
 * Searches through the given swap surface formats and tries
 * to find one that matches our prefered format.
 * 
 * Params:
 *   formats     - Available formats
 *   num_formats - number of available formats
 * 
 * Returns:
 *   Chosen VkSurfaceFormatKHR
 */
VkSurfaceFormatKHR choose_swap_surface_format_(
        VkSurfaceFormatKHR* formats,
        uint32_t num_formats
        ) {
    // prefered is nonlinear SRGB color space & B8G8R8A8 format
    for(uint32_t i = 0; i < num_formats; i++) {
        if(formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && 
                formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){

            printf("Found ideal swap surface format\n");
            return formats[i];
        }
    }

    // If we don't find ideal, just take the first one.
    printf("Didn't find ideal swap surface format. Defaulting to first\n");
    return formats[0];
}

/**
 * Searches through the available present modes to find
 * an ideal one.
 * 
 * Params:
 *   present_modes - array of available present modes
 *   num_present_modes - number of modes
 * 
 * Returns:
 *   Chosen VkPresentModeKHR
 */
VkPresentModeKHR choose_present_mode_(
        VkPresentModeKHR* present_modes,
        uint32_t num_present_modes
        ) {
    for(uint32_t i = 0; i < num_present_modes; i++) {
        if(present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            printf("Found desired mailbox present mode\n");
            return present_modes[i];
        }
    }

    printf("Desired present mode not found. Defaulting to FIFO\n");
    return VK_PRESENT_MODE_FIFO_KHR;
}

/**
 * Selects the idea swap extent.
 * 
 * Params:
 *   capabilities - surface capabilities
 * 
 * Retruns:
 *   Chosen VkExtent2D
 */
VkExtent2D choose_swap_extent_(const VkSurfaceCapabilitiesKHR* const capabilities) {

    // If extent width is set to max, we can do what we want.
    if(capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }
    else {
        VkExtent2D actual_extent = { WIDTH, HEIGHT };

        actual_extent.width = fmax(capabilities->minImageExtent.width,
                fmin(capabilities->maxImageExtent.width, actual_extent.width));
        actual_extent.height = fmax(capabilities->minImageExtent.height,
                fmin(capabilities->maxImageExtent.height, actual_extent.height));

        return actual_extent;
    }
}

/**
 * Creates the logical vulkan device for use in the app.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   bool indicating success.
 */
bool create_logical_device_(vk_app* app) {
    queue_families indices = find_queue_families_(app->physical_device, app->surface);

    float queue_priority = 1.0f; 
    VkDeviceQueueCreateInfo graphics_info = {};
    graphics_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_info.queueFamilyIndex = indices.graphics_family_index;
    graphics_info.queueCount = 1;
    graphics_info.pQueuePriorities = &queue_priority;

    VkDeviceQueueCreateInfo present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    present_info.queueFamilyIndex = indices.present_family_index;
    present_info.queueCount = 1;
    present_info.pQueuePriorities = &queue_priority;

    VkDeviceQueueCreateInfo queue_infos[] = {
        graphics_info,
        present_info
    };

    // don't need anything special, leave default
    VkPhysicalDeviceFeatures device_features = {};
    uint32_t queue_create_count = 0;

    if(indices.graphics_family_index != indices.present_family_index) {
        queue_create_count = 2;
    }
    else {
        queue_create_count = 1;
    }

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_infos;
    device_create_info.queueCreateInfoCount = queue_create_count;
    device_create_info.pEnabledFeatures = &device_features;    

    device_create_info.enabledExtensionCount = DEVICE_EXTENSIONS_COUNT;
    device_create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;

    // These are deprecated, but set for outdated implementations
    if(ENABLE_VALIDATION_LAYERS) {
        device_create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
        device_create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    }
    else {
        device_create_info.enabledLayerCount = 0;
    }

    VkResult success = vkCreateDevice(app->physical_device, 
            &device_create_info, NULL, &app->device);

    if(success != VK_SUCCESS) {
        fprintf(stderr, "Unable to create logical device\n");
    }

    vkGetDeviceQueue(app->device, indices.graphics_family_index, 0, &app->graphics_queue);
    vkGetDeviceQueue(app->device, indices.present_family_index, 0, &app->present_queue);

    return success == VK_SUCCESS;
}

/**
 * Constructs the swapchain that will be used by the app.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   bool indicating success.
 */
bool create_swapchain_(vk_app* app) {
    swapchain_details scd = get_swapchain_support_(app->physical_device, app->surface);

    app->swapchain_format = choose_swap_surface_format_(
            scd.formats, scd.num_formats);
    VkPresentModeKHR present_mode = choose_present_mode_(
            scd.present_modes, scd.num_present_modes);
    app->swapchain_extent = choose_swap_extent_(&scd.capabilities);

    uint32_t img_count = scd.capabilities.minImageCount + 1;
    if(scd.capabilities.maxImageCount > 0 && img_count > scd.capabilities.maxImageCount) {
        img_count = scd.capabilities.maxImageCount;
        printf("Exceeded max image count. Using %i\n", img_count);
    }
    else {
        printf("No max image count found. Using %i\n", img_count);
    }

    VkSwapchainCreateInfoKHR create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = app->surface;
    create_info.minImageCount = img_count;
    create_info.imageFormat = app->swapchain_format.format;
    create_info.imageColorSpace = app->swapchain_format.colorSpace;
    create_info.imageExtent = app->swapchain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    queue_families indices = find_queue_families_(app->physical_device,
            app->surface);
    uint32_t queue_fam_indices[] = {
        indices.graphics_family_index,
        indices.present_family_index
    };

    // If using 2 different families, need to make sharing concurrent
    if(indices.graphics_family_index != indices.present_family_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_fam_indices;
    }
    else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = NULL;
    }

    create_info.preTransform = scd.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(app->device,
            &create_info, NULL, &app->swapchain);

    if(result == VK_SUCCESS) {
        result = vkGetSwapchainImagesKHR(app->device, app->swapchain,
                &app->swapchain_image_count, NULL);

        printf("Swapchain has %i images\n", app->swapchain_image_count);
        if(app->swapchain_image_count > 0) {
            app->swapchain_images = malloc(
                    sizeof(VkImage) * app->swapchain_image_count
                    );

            result = vkGetSwapchainImagesKHR(
                    app->device,
                    app->swapchain,
                    &app->swapchain_image_count,
                    app->swapchain_images
                    );
        }
    }

    return result == VK_SUCCESS;
}

/**
 * Creates image views to be used in the render pipeline.
 * 
 * Params:
 *   app - Vulkan app
 * 
 * Returns:
 *   boolean indicating success
 */
bool create_image_views_(vk_app* app) {

    app->swapchain_image_views = (VkImageView*)malloc(
            sizeof(VkImageView) * app->swapchain_image_count
            );

    bool success = true;
    for(uint32_t i = 0; i < app->swapchain_image_count; i++) {
        VkImageViewCreateInfo create_info = {};

        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = app->swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = app->swapchain_format.format;

        // default color mapping
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(
                app->device,
                &create_info,
                NULL,
                &app->swapchain_image_views[i]
                );

        success &= (result == VK_SUCCESS);
    }

    if(success) {
        printf("Succesfully created image views\n");
    }
    else {
        fprintf(stderr, "Failed to create one or more image views\n");
    }

    return success;
}

bool create_render_pass_(vk_app* app) {
    VkAttachmentDescription color_attachment = {};
    color_attachment.format = app->swapchain_format.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // This references the layout(location = 0) out vec4 outColor in shader
    VkAttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dep = {};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo pass_info = {};
    pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_info.attachmentCount = 1;
    pass_info.pAttachments = &color_attachment;
    pass_info.subpassCount = 1;
    pass_info.pSubpasses = &subpass;
    pass_info.dependencyCount = 1;
    pass_info.pDependencies = &dep;

    VkResult result = vkCreateRenderPass(app->device,
        &pass_info,
        NULL,
        &app->render_pass);

    return result == VK_SUCCESS;    
}

bool create_graphics_pipeline_(vk_app* app) {

    size_t vert_size;
    uint32_t* vert_code = read_file("vert.spv", &vert_size);

    if(vert_code == NULL) {
        fprintf(stderr, "Failed to read shader code from \"vert.spv\"\n");
        return false;
    }

    size_t frag_size;
    uint32_t* frag_code = read_file("frag.spv", &frag_size);
    if(frag_code == NULL) {
        fprintf(stderr, "Failed to read shader code from \"frag.spv\"\n");
        return false;
    }

    // Shaders
    VkShaderModule vert_module = create_shader_module(app, vert_code, vert_size);
    VkShaderModule frag_module = create_shader_module(app, frag_code, frag_size);

    VkPipelineShaderStageCreateInfo vert_stage_info = {};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert_module;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info = {};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag_module;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_stage_info,
        frag_stage_info
    };

    // Vertex Info
    VkPipelineVertexInputStateCreateInfo vert_input_info = {};
    vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vert_input_info.vertexBindingDescriptionCount = 0;
    vert_input_info.pVertexBindingDescriptions = NULL;
    vert_input_info.vertexAttributeDescriptionCount = 0;
    vert_input_info.pVertexAttributeDescriptions = NULL;

    // Topology info
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    // Viewport & scissor
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)app->swapchain_extent.width;
    viewport.height = (float)app->swapchain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = app->swapchain_extent;

    VkPipelineViewportStateCreateInfo vp_info = {};
    vp_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_info.viewportCount = 1;
    vp_info.pViewports = &viewport;
    vp_info.scissorCount = 1;
    vp_info.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rast_info = {};
    rast_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rast_info.depthClampEnable = VK_FALSE;
    rast_info.rasterizerDiscardEnable = VK_FALSE;
    rast_info.polygonMode = VK_POLYGON_MODE_FILL;
    rast_info.lineWidth = 1.0f;
    rast_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rast_info.frontFace = VK_FRONT_FACE_CLOCKWISE;    
    rast_info.depthBiasEnable = VK_FALSE;
    rast_info.depthBiasConstantFactor = 0.0f;
    rast_info.depthBiasClamp = 0.0f;
    rast_info.depthBiasSlopeFactor = 0.0f;    

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multi_info = {};
    multi_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multi_info.sampleShadingEnable = VK_FALSE;
    multi_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multi_info.minSampleShading = 1.0f;
    multi_info.pSampleMask = NULL;
    multi_info.alphaToCoverageEnable = VK_FALSE;
    multi_info.alphaToOneEnable = VK_FALSE;

    // Color blending
    VkPipelineColorBlendAttachmentState blend_attachment = {};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | 
        VK_COLOR_COMPONENT_G_BIT | 
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;
    blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.logicOpEnable = VK_FALSE;
    blend_info.logicOp = VK_LOGIC_OP_COPY;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = &blend_attachment;
    blend_info.blendConstants[0] = 0.0f;
    blend_info.blendConstants[1] = 0.0f;
    blend_info.blendConstants[2] = 0.0f;
    blend_info.blendConstants[3] = 0.0f;

    // Pipeline Layout
    VkPipelineLayoutCreateInfo pipeline_layout = {};
    pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout.setLayoutCount = 0;
    pipeline_layout.pSetLayouts = NULL;
    pipeline_layout.pushConstantRangeCount = 0;
    pipeline_layout.pPushConstantRanges = NULL;

    VkResult result = vkCreatePipelineLayout(app->device,
            &pipeline_layout,
            NULL,
            &app->pipeline_layout);

    if(result == VK_SUCCESS) {
        printf("Successfully created pipeline layout\n");
    }
    else {
        fprintf(stderr, "Failed to create pipeline layout\n");
    }

    VkGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vert_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &vp_info;
    pipeline_info.pRasterizationState = &rast_info;
    pipeline_info.pMultisampleState = &multi_info;
    pipeline_info.pDepthStencilState = NULL;
    pipeline_info.pColorBlendState = &blend_info;
    pipeline_info.pDynamicState = NULL;

    pipeline_info.layout = app->pipeline_layout;
    pipeline_info.renderPass = app->render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    result = vkCreateGraphicsPipelines(app->device, VK_NULL_HANDLE,
        1, &pipeline_info, NULL, &app->graphics_pipeline);

    if(result == VK_SUCCESS) {
        printf("Successfully created graphics pipeline\n");
    }
    else {
        fprintf(stderr, "Failed to create graphics pipeline\n");
    }

    vkDestroyShaderModule(app->device, frag_module, NULL);
    free(frag_code);
    frag_code = NULL;

    vkDestroyShaderModule(app->device, vert_module, NULL);
    free(vert_code);
    vert_code = NULL;

    return result == VK_SUCCESS;
}

bool create_framebuffers_(vk_app* app) {

    // will have as many framebuffers as we do swapchain images
    app->framebuffer_count = app->swapchain_image_count;

    app->framebuffers = (VkFramebuffer*)malloc(sizeof(VkFramebuffer) * app->framebuffer_count);
    
    VkResult result = VK_SUCCESS;
    for(uint32_t i = 0; i < app->swapchain_image_count && result == VK_SUCCESS; i++) {
        VkImageView attachments[] = {
            app->swapchain_image_views[i]
        };

        VkFramebufferCreateInfo buf_info = {};
        buf_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        buf_info.renderPass = app->render_pass;
        buf_info.attachmentCount = 1;
        buf_info.pAttachments = attachments;
        buf_info.width = app->swapchain_extent.width;
        buf_info.height = app->swapchain_extent.height;
        buf_info.layers = 1;

        result = vkCreateFramebuffer(
            app->device,
            &buf_info,
            NULL,
            &app->framebuffers[i]
        );

        if(result != VK_SUCCESS) {
            fprintf(stderr, "Unable to create framebuffer %i\n", i);
        }
    }

    bool success = (result == VK_SUCCESS);

    if(success) {
        printf("Successfully created framebuffers\n");
    }
    else {
        fprintf(stderr, "Unable to create framebuffers\n");
    }

    return success;
}

bool create_cmd_pool_(vk_app* app) {
    queue_families fams = find_queue_families_(app->physical_device,
        app->surface);

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = fams.graphics_family_index;
    pool_info.flags = 0;

    VkResult result = vkCreateCommandPool(
        app->device,
        &pool_info,
        NULL,
        &app->cmd_pool
    );

    bool success = result == VK_SUCCESS;

    if(success) {
        printf("Successfully created command pool\n");
    }
    else {
        fprintf(stderr, "Failed to create command pool\n");
    }

    return success;
}

bool create_cmd_buffers_(vk_app* app) {

    // Cmd buffer per framebuffer
    app->cmd_buffer_count = app->framebuffer_count;
    app->cmd_buffers = (VkCommandBuffer*)malloc(
        sizeof(VkCommandBuffer) * app->cmd_buffer_count);

    VkCommandBufferAllocateInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    buf_info.commandPool = app->cmd_pool;
    buf_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    buf_info.commandBufferCount = app->cmd_buffer_count;

    bool success = vkAllocateCommandBuffers(app->device, &buf_info, app->cmd_buffers) == VK_SUCCESS;

    if(success) {
        printf("Successfully created %i command buffers\n", app->cmd_buffer_count);
    }
    else {
        fprintf(stderr, "Unable to create command buffers\n");
        return success;
    }

    VkResult result = VK_SUCCESS;
    for(uint32_t i = 0; i < app->cmd_buffer_count && result == VK_SUCCESS; i++) {
        VkCommandBufferBeginInfo beg_info = {};
        beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beg_info.flags = 0;
        beg_info.pInheritanceInfo = NULL;

        result = vkBeginCommandBuffer(app->cmd_buffers[i], &beg_info);

        if(result != VK_SUCCESS) {
            fprintf(stderr, "Unable to begin cmd buffer %i\n", i);
            break;
        }

        VkRenderPassBeginInfo pass_info = {};
        pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        pass_info.renderPass = app->render_pass;
        pass_info.framebuffer = app->framebuffers[i];

        VkOffset2D offset = {0, 0};
        pass_info.renderArea.offset = offset;
        pass_info.renderArea.extent = app->swapchain_extent;

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        pass_info.clearValueCount = 1;
        pass_info.pClearValues = &clearColor;

        vkCmdBeginRenderPass(app->cmd_buffers[i], &pass_info, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(app->cmd_buffers[i],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            app->graphics_pipeline);

        vkCmdDraw(app->cmd_buffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(app->cmd_buffers[i]);

        result = vkEndCommandBuffer(app->cmd_buffers[i]);

        if(result != VK_SUCCESS) {
            fprintf(stderr, "Unable to fill cmd buffer %i\n", i);
            break;
        }
    }

    success = result == VK_SUCCESS;

    if(success) {
        printf("Successfully initialized command buffers\n");
    }
    else {
        fprintf(stderr, "Unable to initialize command buffers\n");
    }

    return success;
}

bool create_sync_objects_(vk_app* app) {
    app->image_available = (VkSemaphore*)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    app->render_finished = (VkSemaphore*)malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
    app->in_flight = (VkFence*)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);

    app->imgs_in_flight = (VkFence*)malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        app->imgs_in_flight[i] = VK_NULL_HANDLE;
    }

    VkSemaphoreCreateInfo sem_info = {};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkCreateSemaphore(app->device, &sem_info, NULL, &app->image_available[i]);
        vkCreateSemaphore(app->device, &sem_info, NULL, &app->render_finished[i]);
        vkCreateFence(app->device, &fence_info, NULL, &app->in_flight[i]);
    }

    return true;
}

void draw_frame_(vk_app* app) {
    vkWaitForFences(app->device, 1, &app->in_flight[app->current_frame],
        VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    vkAcquireNextImageKHR(app->device, app->swapchain, UINT64_MAX,
        app->image_available[app->current_frame], VK_NULL_HANDLE,
        &image_index);

    if(app->imgs_in_flight[image_index] != VK_NULL_HANDLE) {
        vkWaitForFences(app->device, 1, &app->imgs_in_flight[image_index], VK_TRUE, UINT64_MAX);
    }
    app->imgs_in_flight[image_index] = app->in_flight[app->current_frame];
    
    VkSemaphore wait_sems[] = {app->image_available[app->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_sems;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &app->cmd_buffers[image_index];

    VkSemaphore signal_sems[] = {app->render_finished[app->current_frame]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_sems;

    vkResetFences(app->device, 1, &app->in_flight[app->current_frame]);
    VkResult result = vkQueueSubmit(app->graphics_queue, 1, &submit_info,
        app->in_flight[app->current_frame]);

    if(result != VK_SUCCESS) {
        fprintf(stderr, "Unable to submit draw cmd\n");
        return;
    }

    VkPresentInfoKHR present = {};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = signal_sems;

    VkSwapchainKHR swap_chains = { app->swapchain };
    present.swapchainCount = 1;
    present.pSwapchains = &swap_chains;
    present.pImageIndices = &image_index;
    present.pResults = NULL;

    result = vkQueuePresentKHR(app->present_queue, &present);

    if(result != VK_SUCCESS) {
        fprintf(stderr, "Failed to draw frame");
    }

    app->current_frame = (app->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkShaderModule create_shader_module(vk_app* app, const uint32_t* code, size_t code_len) {
    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code_len;
    info.pCode = code;

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(app->device, 
            &info, NULL, &module);

    if(result != VK_SUCCESS) {
        fprintf(stderr, "Could not create shader module");
    }

    return module;
}

/**
 * Debug utils messenger callback method.
 */
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
        ) {
    fprintf(stderr, "Validation layer: %s", pCallbackData->pMessage);
    return VK_FALSE;
}

//
//  Swapchain Support Interface
//

void cleanup_swapchain_details(swapchain_details* scd) {
    free(scd->formats);
    free(scd->present_modes);
}

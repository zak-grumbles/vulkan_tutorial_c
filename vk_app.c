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

bool create_graphics_pipeline_(vk_app*);
VkShaderModule create_shader_module(vk_app*, const uint32_t*, size_t);

swapchain_details get_swapchain_support_(VkPhysicalDevice, VkSurfaceKHR);
VkSurfaceFormatKHR choose_swap_surface_format_(VkSurfaceFormatKHR*, uint32_t);
VkPresentModeKHR choose_present_mode_(VkPresentModeKHR*, uint32_t);
VkExtent2D choose_swap_extent_(const VkSurfaceCapabilitiesKHR* const capabilities);
bool create_swapchain_(vk_app*);
bool create_image_views_(vk_app*);


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
void init_vk_app(vk_app* app) {
    init_window_(app);

    bool success = init_vulkan_(app);

    if(success) {
        printf("Successfully initialized vulkan");
    }
    else {
        printf("Failed to initialize vulkan");
    }
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
    }
}

/**
 * Cleans up the vk app struct.
 * 
 * Params:
 *   app - vulkan app
 */
void cleanup_vk_app(vk_app* app) {
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

    success = init_instance_(app);
    
    if(success && ENABLE_VALIDATION_LAYERS) {
        setup_debug_messenger_(app);
    }

    if(success) success &= create_surface_(app);
    if(success) success &= pick_physical_device_(app);
    if(success) success &= create_logical_device_(app);
    if(success) success &= create_swapchain_(app);
    if(success) success &= create_image_views_(app);

    if(success) success &= create_graphics_pipeline_(app);

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
    for(uint32_t i = 0; i < device_count; i++) {
        if(is_device_suitable_(devices[i], app->surface)) {
            app->physical_device = devices[i];
            break;
        }
    }

    free(devices);
    devices = NULL;

    return 1;
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

    bool valid = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        features.geometryShader;

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
    VkExtent2D extent = choose_swap_extent_(&scd.capabilities);

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
    create_info.imageExtent = extent;
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

bool create_graphics_pipeline_(vk_app* app) {

    size_t vert_size;
    uint32_t* vert_code = read_file("vert.spv", &vert_size);

    size_t frag_size;
    uint32_t* frag_code = read_file("frag.spv", &frag_size);

    VkShaderModule vertModule = create_shader_module(app, vert_code, vert_size);
    VkShaderModule fragModule = create_shader_module(app, frag_code, frag_size);

    vkDestroyShaderModule(app->device, fragModule, NULL);
    free(frag_code);
    frag_code = NULL;

    vkDestroyShaderModule(app->device, vertModule, NULL);
    free(vert_code);
    vert_code = NULL;

    return true;
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
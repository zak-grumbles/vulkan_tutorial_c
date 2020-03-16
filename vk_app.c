#include "vk_app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
const int ENABLE_VALIDATION_LAYERS = 0;
#else
const int ENABLE_VALIDATION_LAYERS = 1;
#endif

const char* VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};
const uint32_t VALIDATION_LAYER_COUNT = 1;

// "Private" interface
void init_window_(vk_app*);
int init_vulkan_(vk_app*);

char** get_required_extensions_(uint32_t* count);
int init_instance_(vk_app*);

int setup_debug_messenger_(vk_app*);

int create_surface_(vk_app*);

int pick_physical_device_(vk_app*);
int is_device_suitable_(VkPhysicalDevice device);
queue_families find_queue_families_(VkPhysicalDevice device);

int create_logical_device_(vk_app*);

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_cb(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
);

/**
 * Initializes the vulkan app struct.
 * Params:
 *   app - vulkan app struct
 */
void init_vk_app(vk_app* app) {
    init_window_(app);

    int success = init_vulkan_(app);

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

    vkDestroyDevice(app->device, NULL);

    vkDestroySurfaceKHR(app->instance, app->surface, NULL);
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

    app->app_window = glfwCreateWindow(800, 600, "Vulkan Window", NULL, NULL);
}

/**
 * Initializes all of the necessary vulkan structs and
 * data.
 * 
 * Params:
 *   app - vulkan app
 */
int init_vulkan_(vk_app* app) {
    int success = VK_SUCCESS;

    success = init_instance_(app);
    
    if(success && ENABLE_VALIDATION_LAYERS) {
        setup_debug_messenger_(app);
    }

    if(success) success &= pick_physical_device_(app);
    if(success) success &= create_logical_device_(app);

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

        exts[total_count] = (char*)malloc(sizeof(char) * ext_len);
        strcpy(exts[total_count], VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return exts;
}

/**
 * Initializes the vulkan instance.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   int indicating success.
 */
int init_instance_(vk_app* app) {
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
 *   int indicating success
 */
int setup_debug_messenger_(vk_app* app) {
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
        return 0;
    }
}

/**
 * Creates the window surface.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   int indicating success
 */
int create_surface(vk_app* app) {
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
int pick_physical_device_(vk_app* app) {
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
        if(is_device_suitable_(devices[i])) {
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
 *   int indicating validity
 */
int is_device_suitable_(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(device, &props);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    int valid = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        features.geometryShader;

    queue_families families = find_queue_families_(device);

    valid &= families.is_complete;

    printf("  - Checking device %s - %i\n", props.deviceName, valid);

    return valid;
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
queue_families find_queue_families_(VkPhysicalDevice device) {
    queue_families indices;
    indices.is_complete = 0;

    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, NULL);

    VkQueueFamilyProperties* families = (VkQueueFamilyProperties*)malloc(
        family_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families);

    printf("  - Found %i queue families\n", family_count);
    for(uint32_t i = 0; i < family_count; i++) {
        printf("    - Family %i has: %i queues, ", i, families[i].queueCount);

        if(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            printf("graphics bit\n");

            if(!indices.is_complete) {
                indices.graphics_family_index = i;
                indices.is_complete = 1;
            }
        }
        else {
            printf("no graphics bit\n");
        }
    }

    return indices;
}

/**
 * Creates the logical vulkan device for use in the app.
 * 
 * Params:
 *   app - vulkan app
 * 
 * Returns:
 *   int indicating success.
 */
int create_logical_device_(vk_app* app) {
    queue_families indices = find_queue_families_(app->physical_device);

    float queue_priority = 1.0f; // still necessary even tho only 1 queue
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = indices.graphics_family_index;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    // don't need anything special, leave default
    VkPhysicalDeviceFeatures device_features = {};

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pEnabledFeatures = &device_features;    

    device_create_info.enabledExtensionCount = 0;

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

    return success == VK_SUCCESS;
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
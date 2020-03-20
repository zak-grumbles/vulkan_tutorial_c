#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdbool.h>

/**
 * Holds indices of graphics families for a single physical device.
 */
typedef struct {
    int32_t graphics_family_index;
    int32_t present_family_index;
    bool is_complete;
} queue_families;

/**
 * Represents a vulkan application. Holds all relevant structs and
 * data.
 */
typedef struct {
    GLFWwindow* app_window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue graphics_queue;
    VkQueue present_queue;
} vk_app;

// "Public" interface
void init_vk_app(vk_app*);
void cleanup_vk_app(vk_app*);

void run_vk_app(vk_app*);


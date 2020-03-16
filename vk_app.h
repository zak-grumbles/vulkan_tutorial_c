#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

typedef struct {
    uint32_t graphics_family_index;
    int is_complete;
} queue_families;

typedef struct {
    GLFWwindow* app_window;
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkDebugUtilsMessengerEXT debug_messenger;
} vk_app;

// "Public" interface
void init_vk_app(vk_app*);
void cleanup_vk_app(vk_app*);

void run_vk_app(vk_app*);


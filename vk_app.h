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
 * Holds data about swapchain support.
 */
typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;

    VkSurfaceFormatKHR* formats;
    uint32_t num_formats;

    VkPresentModeKHR* present_modes;
    uint32_t num_present_modes;
} swapchain_details;

void cleanup_swapchain_details(swapchain_details*);

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

    VkSurfaceFormatKHR swapchain_format;
    VkExtent2D swapchain_extent;
    VkSwapchainKHR swapchain;
    VkImage* swapchain_images;
    uint32_t swapchain_image_count;

    VkImageView* swapchain_image_views;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkFramebuffer* framebuffers;
    uint32_t framebuffer_count;

    VkCommandPool cmd_pool;
    VkCommandBuffer* cmd_buffers;
    uint32_t cmd_buffer_count;
} vk_app;

// "Public" interface
bool init_vk_app(vk_app*);
void cleanup_vk_app(vk_app*);

void run_vk_app(vk_app*);


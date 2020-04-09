
#include "vk_app.h"

#include <stdio.h>

int main() {
    glfwInit();

    vk_app app;
    int initialized = init_vk_app(&app);

    if(initialized) {
        run_vk_app(&app);
        cleanup_vk_app(&app);
    }
    else {
        fprintf(stderr, "Unable to initialize vulkan. Exiting\n");
    }

    return 0;
}

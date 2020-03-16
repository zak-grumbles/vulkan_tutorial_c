
#include "vk_app.h"

#include <stdio.h>

int main() {
    glfwInit();

    vk_app app;
    init_vk_app(&app);

    run_vk_app(&app);
    cleanup_vk_app(&app);

    return 0;
}

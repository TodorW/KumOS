#include <zephyr/kernel.h>       // Kernel API
#include <zephyr/logging/log.h>   // Logging API
#include <zephyr/device.h>        // Device API
#include <zephyr/drivers/gpio.h>  // GPIO API for hardware
#include <zephyr/sys/printk.h>    // Basic printk support
#include <zephyr/shell/shell.h>   // Shell API for user prompt
#include <zephyr/sys/reboot.h>    // Reboot API

/* Initialize logger */
LOG_MODULE_REGISTER(boot, LOG_LEVEL_DBG);

/* Hardware detection constants */
#define LED0_NODE DT_ALIAS(led0)    // Alias to LED node in device tree
#define BUTTON_NODE DT_ALIAS(sw0)   // Alias to Button node in device tree

/* Memory Initialization parameters */
#define MEMORY_SIZE 1024            // 1 KB for memory buffer

/* Kernel Parameter Parsing */
static const char *kernel_param = "default_param";  // Default kernel param

/* Memory buffer */
static uint8_t memory_buffer[MEMORY_SIZE];

/* Function Prototypes */
void log_initialization(void);
void parse_kernel_parameters(void);
void detect_hardware(void);
void init_memory(void);
void user_prompt(void);

/* Enhanced Logging */
void log_initialization(void) {
    LOG_INF("System booting...");
    LOG_DBG("Logger initialized with debug level.");
}

/* Kernel Parameter Parsing */
void parse_kernel_parameters(void) {
    /* Replace this with actual parameter fetching if available */
    kernel_param = CONFIG_BOOT_KERNEL_PARAM;  // Example, would be set in Kconfig
    if (kernel_param) {
        LOG_INF("Kernel parameter parsed: %s", kernel_param);
    } else {
        LOG_WRN("No kernel parameters found.");
    }
}

/* Hardware Detection */
void detect_hardware(void) {
#ifdef LED0_NODE
    const struct device *led = DEVICE_DT_GET(LED0_NODE);
    if (device_is_ready(led)) {
        LOG_INF("LED detected and ready.");
    } else {
        LOG_ERR("LED device not found or not ready.");
    }
#endif

#ifdef BUTTON_NODE
    const struct device *button = DEVICE_DT_GET(BUTTON_NODE);
    if (device_is_ready(button)) {
        LOG_INF("Button detected and ready.");
    } else {
        LOG_ERR("Button device not found or not ready.");
    }
#endif
}

/* Memory Initialization */
void init_memory(void) {
    memset(memory_buffer, 0, sizeof(memory_buffer));
    LOG_INF("Memory initialized with size: %d bytes", MEMORY_SIZE);
}

/* User Prompt - Utilizing Zephyr Shell */
static int cmd_reboot(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    LOG_INF("Rebooting system...");
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

SHELL_CMD_REGISTER(reboot, NULL, "Reboot the system", cmd_reboot);

/* User prompt setup */
void user_prompt(void) {
    LOG_INF("Type 'reboot' to reboot the system.");
}

/* Main boot function */
void main(void) {
    log_initialization();

    LOG_INF("Parsing kernel parameters...");
    parse_kernel_parameters();

    LOG_INF("Detecting hardware...");
    detect_hardware();

    LOG_INF("Initializing memory...");
    init_memory();

    LOG_INF("Boot sequence completed.");
    user_prompt();
}

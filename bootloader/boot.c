#include <zephyr/kernel.h>       // Kernel API
#include <zephyr/logging/log.h>   // Logging API
#include <zephyr/device.h>        // Device API
#include <zephyr/drivers/gpio.h>  // GPIO API for hardware
#include <zephyr/sys/printk.h>    // Basic printk support
#include <zephyr/shell/shell.h>   // Shell API for user prompt
#include <zephyr/sys/reboot.h>    // Reboot API
#include <zephyr/version.h>       // Zephyr version API
#include <zephyr/arch/cpu.h>      // CPU architecture support

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

/* Debug Mode Flag */
static bool debug_mode = false;

/* Function Prototypes */
void log_initialization(void);
void parse_kernel_parameters(void);
void detect_hardware(void);
void init_memory(void);
void display_kernel_version(void);
void user_prompt(void);
void check_architecture(void);

/* Enhanced Logging */
void log_initialization(void) {
    LOG_INF("System booting...");
    LOG_DBG("Logger initialized with debug level.");
}

/* Kernel Parameter Parsing */
void parse_kernel_parameters(void) {
    /* Replace this with actual parameter fetching if available */
#ifdef CONFIG_BOOT_KERNEL_PARAM
    kernel_param = CONFIG_BOOT_KERNEL_PARAM;  // Example, would be set in Kconfig
#endif
    if (kernel_param) {
        LOG_INF("Kernel parameter parsed: %s", kernel_param);
    } else {
        LOG_WRN("No kernel parameters found.");
    }
}

/* Hardware Detection with Error Handling */
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

/* Display Kernel Version */
void display_kernel_version(void) {
    LOG_INF("Zephyr OS version: %d.%d.%d", KERNEL_VERSION_MAJOR, KERNEL_VERSION_MINOR, KERNEL_PATCHLEVEL);
}

/* Check and Display Architecture */
void check_architecture(void) {
#if defined(CONFIG_SOC_FAMILY_NRF)
    LOG_INF("Architecture: NRF (Nordic Semiconductor)");
#elif defined(CONFIG_SOC_FAMILY_STM32)
    LOG_INF("Architecture: STM32 (STMicroelectronics)");
#else
    LOG_INF("Architecture: Unsupported or unknown");
#endif
}

/* User Prompt - Utilizing Zephyr Shell */
static int cmd_reboot(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    LOG_INF("Rebooting system...");
    sys_reboot(SYS_REBOOT_COLD);
    return 0;
}

static int cmd_debug(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    debug_mode = !debug_mode;
    if (debug_mode) {
        LOG_INF("Debug mode enabled.");
    } else {
        LOG_INF("Debug mode disabled.");
    }
    return 0;
}

SHELL_CMD_REGISTER(reboot, NULL, "Reboot the system", cmd_reboot);
SHELL_CMD_REGISTER(debug, NULL, "Toggle debug mode", cmd_debug);

/* User prompt setup */
void user_prompt(void) {
    LOG_INF("Type 'reboot' to reboot the system, or 'debug' to toggle debug mode.");
}

/* Boot Sequence Customization */
void boot_sequence(void) {
    log_initialization();

    if (debug_mode) {
        LOG_DBG("Debug mode is ON. Extra information will be logged.");
    }

    LOG_INF("Parsing kernel parameters...");
    parse_kernel_parameters();

    LOG_INF("Detecting hardware...");
    detect_hardware();

    LOG_INF("Initializing memory...");
    init_memory();

    LOG_INF("Displaying kernel version...");
    display_kernel_version();

    LOG_INF("Checking system architecture...");
    check_architecture();

    LOG_INF("Boot sequence completed.");
    user_prompt();
}

/* Main boot function */
void main(void) {
    /* Set debug mode based on a config flag, if defined */
#ifdef CONFIG_BOOT_DEBUG_MODE
    debug_mode = true;
#endif

    /* Begin the customized boot sequence */
    boot_sequence();
}

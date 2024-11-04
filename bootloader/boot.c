#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/version.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/drivers/watchdog.h>    // Watchdog API

/* Initialize logger */
LOG_MODULE_REGISTER(boot, LOG_LEVEL_DBG);

/* Hardware detection constants */
#define LED0_NODE DT_ALIAS(led0)
#define BUTTON_NODE DT_ALIAS(sw0)

/* Memory and Kernel Parameters */
#define MEMORY_SIZE 1024
static const char *kernel_param = "default_param";

/* Memory buffer */
static uint8_t memory_buffer[MEMORY_SIZE];

/* Debug Mode Flag */
static bool debug_mode = false;

/* Watchdog Device */
const struct device *wdt_dev;

/* Function Prototypes */
void log_initialization(void);
void parse_kernel_parameters(void);
void detect_hardware(void);
void init_memory(void);
void display_kernel_version(void);
void user_prompt(void);
void check_architecture(void);
void setup_watchdog(void);
void shell_command_list(void);
void boot_sequence(void);

/* Enhanced Logging */
void log_initialization(void) {
    LOG_INF("System booting...");
    LOG_DBG("Logger initialized with debug level.");
}

/* Kernel Parameter Parsing */
void parse_kernel_parameters(void) {
#ifdef CONFIG_BOOT_KERNEL_PARAM
    kernel_param = CONFIG_BOOT_KERNEL_PARAM;
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

/* Watchdog Setup for System Safety */
void setup_watchdog(void) {
    wdt_dev = device_get_binding(DT_LABEL(DT_NODELABEL(wdt0)));
    if (!wdt_dev) {
        LOG_ERR("Watchdog device not found");
        return;
    }
    struct wdt_timeout_cfg wdt_cfg = {
        .window = { .min = 0, .max = 1000 },
        .callback = NULL,
        .flags = WDT_FLAG_RESET_SOC,
    };
    int err = wdt_install_timeout(wdt_dev, &wdt_cfg);
    if (err != 0) {
        LOG_ERR("Failed to install watchdog timeout, error: %d", err);
        return;
    }
    err = wdt_setup(wdt_dev, 0);
    if (err != 0) {
        LOG_ERR("Failed to setup watchdog, error: %d", err);
    } else {
        LOG_INF("Watchdog initialized and started.");
    }
}

/* Shell Commands */
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

/* Additional Shell Commands */
static int cmd_memory_status(const struct shell *shell, size_t argc, char **argv) {
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    LOG_INF("Memory buffer status: %s", memory_buffer ? "Initialized" : "Uninitialized");
    return 0;
}

SHELL_CMD_REGISTER(reboot, NULL, "Reboot the system", cmd_reboot);
SHELL_CMD_REGISTER(debug, NULL, "Toggle debug mode", cmd_debug);
SHELL_CMD_REGISTER(mem_status, NULL, "Check memory buffer status", cmd_memory_status);

void user_prompt(void) {
    LOG_INF("Type 'reboot' to reboot the system, 'debug' to toggle debug mode, 'mem_status' for memory buffer status.");
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

    LOG_INF("Setting up watchdog...");
    setup_watchdog();

    LOG_INF("Boot sequence completed.");
    user_prompt();
}

/* Main Boot Function */
void main(void) {
#ifdef CONFIG_BOOT_DEBUG_MODE
    debug_mode = true;
#endif
    boot_sequence();
}

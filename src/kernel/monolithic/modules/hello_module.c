/* Test module: hello_module
 * Demonstrates basic module functionality with printk integration
 */

#include <kernel/log.h>

/* Module initialization function */
static int module_init(void)
{
    printk(LOG_INFO, "hello_module: module loaded and initialized!");
    return 0;
}

/* Module exit function */
static void module_exit(void)
{
    printk(LOG_INFO, "hello_module: module unloading");
}

/* Module metadata - must be exported with these exact names */

/* Init function pointer - will be called after relocation */
int (*module_init_fn)(void) = &module_init;

/* Exit function pointer - will be called on unload */
void (*module_exit_fn)(void) = &module_exit;

/* Module name string */
const char *module_name_str = "hello_module";

/* Module version string */
const char *module_version_str = "1.0";

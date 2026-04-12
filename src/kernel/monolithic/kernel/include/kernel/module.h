#ifndef KERNEL_MODULE_H
#define KERNEL_MODULE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __GNUC__
#define MODULE_VISIBLE __attribute__((visibility("default")))
#else
#define MODULE_VISIBLE
#endif

typedef int (*module_init_func_t)(void);
typedef void (*module_exit_func_t)(void);

typedef enum module_state
{
    MODULE_STATE_UNUSED = 0,
    MODULE_STATE_LOADED,
    MODULE_STATE_UNLOADED,
    MODULE_STATE_FAILED,
} module_state_t;

typedef struct kernel_symbol
{
    const char *name;
    void *address;
} kernel_symbol_t;

#define MAX_MODULE_PAGES 16

typedef struct module
{
    const char *name;
    const char *version;
    module_init_func_t init;
    module_exit_func_t exit;
    void *data;
    uint32_t load_base_vaddr;
    uint32_t load_size;
    uint32_t page_count;
    uint32_t frames[MAX_MODULE_PAGES];
    module_state_t state;
} module_t;

MODULE_VISIBLE int module_register_symbol(const char *name, void *address);
MODULE_VISIBLE void *module_get_symbol(const char *name);
MODULE_VISIBLE int module_load(const void *elf_data, size_t elf_size);
MODULE_VISIBLE int module_unload(const char *name);
MODULE_VISIBLE module_t *module_find(const char *name);

#define EXPORT_SYMBOL(symbol) module_register_symbol(#symbol, (void *)(symbol))

#endif /* KERNEL_MODULE_H */

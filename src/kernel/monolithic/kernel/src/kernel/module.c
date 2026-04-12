#include <kernel/log.h>
#include <kernel/module.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <stddef.h>

#define MAX_KERNEL_SYMBOLS 64
#define MAX_LOADED_MODULES 16

extern int module_loader_load(module_t *module, const void *elf_data, size_t elf_size);

static int kstrcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }

    return (int)((unsigned char)*a) - (int)((unsigned char)*b);
}

static kernel_symbol_t kernel_symbols[MAX_KERNEL_SYMBOLS];
static size_t kernel_symbol_count = 0;

static module_t loaded_modules[MAX_LOADED_MODULES];
static size_t loaded_module_count = 0;

int module_register_symbol(const char *name, void *address)
{
    if (name == NULL || address == NULL)
    {
        return -1;
    }

    if (kernel_symbol_count >= MAX_KERNEL_SYMBOLS)
    {
        printk(LOG_ERROR, "module_register_symbol: symbol table full");
        return -1;
    }

    kernel_symbols[kernel_symbol_count].name = name;
    kernel_symbols[kernel_symbol_count].address = address;
    kernel_symbol_count++;
    return 0;
}

void *module_get_symbol(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < kernel_symbol_count; i++)
    {
        if (kstrcmp(kernel_symbols[i].name, name) == 0)
        {
            return kernel_symbols[i].address;
        }
    }

    return NULL;
}

module_t *module_find(const char *name)
{
    if (name == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < loaded_module_count; i++)
    {
        if (loaded_modules[i].name != NULL && kstrcmp(loaded_modules[i].name, name) == 0)
        {
            return &loaded_modules[i];
        }
    }

    return NULL;
}

int module_load(const void *elf_data, size_t elf_size)
{
    if (elf_data == NULL || elf_size == 0)
    {
        printk(LOG_ERROR, "module_load: invalid ELF data");
        return -1;
    }

    if (loaded_module_count >= MAX_LOADED_MODULES)
    {
        printk(LOG_ERROR, "module_load: no free module slots");
        return -1;
    }

    // Allocate a temporary module slot for loading
    module_t *module = &loaded_modules[loaded_module_count];
    module->name = NULL;
    module->version = NULL;
    module->init = NULL;
    module->exit = NULL;
    module->data = NULL;
    module->load_base_vaddr = 0;
    module->load_size = 0;
    module->page_count = 0;
    module->state = MODULE_STATE_UNUSED;

    // Load and relocate the ELF module
    if (module_loader_load(module, elf_data, elf_size) != 0)
    {
        printk(LOG_ERROR, "module_load: ELF loader failed");
        module->state = MODULE_STATE_UNUSED;
        return -1;
    }

    // Module name should now be populated by module_loader_load from module_name_str symbol
    if (module->name != NULL)
    {
        // Check for duplicate module names
        for (size_t i = 0; i < loaded_module_count; ++i)
        {
            if (loaded_modules[i].name != NULL && kstrcmp(loaded_modules[i].name, module->name) == 0)
            {
                printk(LOG_ERROR, "module_load: module '%s' already loaded", module->name);
                for (uint32_t j = 0; j < module->page_count; ++j)
                {
                    vmm_unmap_page(module->load_base_vaddr + j * PAGE_SIZE);
                    pmm_free_frame((void *)module->frames[j]);
                }
                module->page_count = 0;
                module->state = MODULE_STATE_UNUSED;
                return -1;
            }
        }
    }

    // Call module init if present
    if (module->init != NULL)
    {
        printk(LOG_INFO, "module_load: calling module_init for '%s'", module->name ? module->name : "(unnamed)");
        int init_result = module->init();
        if (init_result != 0)
        {
            printk(LOG_ERROR, "module_load: module_init failed with result %d", init_result);
            for (uint32_t i = 0; i < module->page_count; ++i)
            {
                vmm_unmap_page(module->load_base_vaddr + i * PAGE_SIZE);
                pmm_free_frame((void *)module->frames[i]);
            }

            module->page_count = 0;
            module->state = MODULE_STATE_FAILED;
            return -1;
        }
    }

    // Increment module count only after successful init
    loaded_module_count++;
    printk(LOG_INFO, "module_load: module loaded successfully, %u modules total", (unsigned)loaded_module_count);
    return 0;
}

int module_unload(const char *name)
{
    module_t *module = module_find(name);
    if (module == NULL)
    {
        printk(LOG_WARNING, "module_unload: module '%s' not found", name);
        return -1;
    }

    if (module->state != MODULE_STATE_LOADED)
    {
        printk(LOG_WARNING, "module_unload: module '%s' is not loaded", name);
        return -1;
    }

    if (module->exit != NULL)
    {
        module->exit();
    }

    module->state = MODULE_STATE_UNLOADED;
    printk(LOG_INFO, "module_unload: module '%s' unloaded", name);
    return 0;
}

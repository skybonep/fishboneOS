#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/module.h>
#include <kernel/log.h>

#define EI_NIDENT 16

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define EV_CURRENT 1

#define ET_REL 1
#define EM_386 3

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOBITS 8
#define SHT_REL 9

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

#define R_386_NONE 0
#define R_386_32 1
#define R_386_PC32 2
#define R_386_PLT32 4

#define MODULE_LOAD_BASE 0xC0900000

typedef struct __attribute__((packed))
{
    unsigned char e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf32_Ehdr;

typedef struct __attribute__((packed))
{
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
} Elf32_Shdr;

typedef struct __attribute__((packed))
{
    uint32_t st_name;
    uint32_t st_value;
    uint32_t st_size;
    unsigned char st_info;
    unsigned char st_other;
    uint16_t st_shndx;
} Elf32_Sym;

typedef struct __attribute__((packed))
{
    uint32_t r_offset;
    uint32_t r_info;
} Elf32_Rel;

static inline unsigned char elf32_st_bind(unsigned char info)
{
    return info >> 4;
}

static inline unsigned char elf32_st_type(unsigned char info)
{
    return info & 0x0F;
}

static inline uint32_t elf32_r_sym(uint32_t info)
{
    return info >> 8;
}

static inline uint32_t elf32_r_type(uint32_t info)
{
    return info & 0xFF;
}

static const char *strtab_string(const char *strtab, uint32_t strtab_size, uint32_t offset)
{
    if (offset >= strtab_size)
        return NULL;

    return strtab + offset;
}

typedef struct
{
    uint32_t section_index;
    uint32_t section_base_vaddr;
} section_mapping_t;

static bool str_equal(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }

    return *a == *b;
}

static bool validate_elf_header(const Elf32_Ehdr *ehdr, size_t elf_size)
{
    if (elf_size < sizeof(Elf32_Ehdr))
        return false;

    if (ehdr->e_ident[0] != ELF_MAGIC0 || ehdr->e_ident[1] != ELF_MAGIC1 ||
        ehdr->e_ident[2] != ELF_MAGIC2 || ehdr->e_ident[3] != ELF_MAGIC3)
    {
        return false;
    }

    if (ehdr->e_ident[4] != ELFCLASS32 || ehdr->e_ident[5] != ELFDATA2LSB ||
        ehdr->e_version != EV_CURRENT || ehdr->e_type != ET_REL ||
        ehdr->e_machine != EM_386)
    {
        return false;
    }

    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0 || ehdr->e_shentsize != sizeof(Elf32_Shdr))
        return false;

    if ((uint32_t)ehdr->e_shoff + (uint32_t)ehdr->e_shnum * sizeof(Elf32_Shdr) > elf_size)
        return false;

    if (ehdr->e_shstrndx >= ehdr->e_shnum)
        return false;

    return true;
}

static const Elf32_Shdr *section_header(const Elf32_Ehdr *ehdr, const uint8_t *elf_bytes, uint32_t index)
{
    if (index >= ehdr->e_shnum)
        return NULL;

    return (const Elf32_Shdr *)(elf_bytes + ehdr->e_shoff + index * sizeof(Elf32_Shdr));
}

static const char *section_name(const Elf32_Shdr *sh, const char *shstrtab, uint32_t shstrtab_size)
{
    if (sh->sh_name >= shstrtab_size)
        return NULL;

    return shstrtab + sh->sh_name;
}

static bool allocate_module_frames(module_t *module, uint32_t pages)
{
    for (uint32_t i = 0; i < pages; ++i)
    {
        if (module->page_count >= MAX_MODULE_PAGES)
            return false;

        void *frame = pmm_alloc_frame();
        if (frame == NULL)
            return false;

        module->frames[module->page_count++] = (uint32_t)frame;
    }

    return true;
}

static void cleanup_module_frames(module_t *module)
{
    for (uint32_t i = 0; i < module->page_count; ++i)
    {
        uint32_t frame = module->frames[i];
        vmm_unmap_page(module->load_base_vaddr + i * PAGE_SIZE);
        pmm_free_frame((void *)frame);
    }

    module->page_count = 0;
    module->load_size = 0;
    module->load_base_vaddr = 0;
}

static uint32_t lookup_module_symbol(const char *symbol_name, const Elf32_Ehdr *ehdr, const uint8_t *elf_bytes,
                                     size_t elf_size, const section_mapping_t *section_mappings, uint32_t section_count)
{
    if (symbol_name == NULL || ehdr == NULL || elf_bytes == NULL)
    {
        return 0;
    }

    const Elf32_Shdr *symtab_sh = NULL;
    const Elf32_Shdr *strtab_sh = NULL;

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *sh = section_header(ehdr, elf_bytes, i);
        const char *name = section_name(sh, (const char *)(elf_bytes + section_header(ehdr, elf_bytes, ehdr->e_shstrndx)->sh_offset),
                                        section_header(ehdr, elf_bytes, ehdr->e_shstrndx)->sh_size);
        if (name == NULL)
            continue;

        if (sh->sh_type == SHT_SYMTAB && str_equal(name, ".symtab"))
            symtab_sh = sh;
        else if (sh->sh_type == SHT_STRTAB && str_equal(name, ".strtab"))
            strtab_sh = sh;
    }

    if (symtab_sh == NULL || strtab_sh == NULL)
    {
        printk(LOG_WARNING, "lookup_module_symbol: .symtab or .strtab not found");
        return 0;
    }

    if (symtab_sh->sh_offset + symtab_sh->sh_size > elf_size || strtab_sh->sh_offset + strtab_sh->sh_size > elf_size)
    {
        printk(LOG_WARNING, "lookup_module_symbol: symbol or string table out of range");
        return 0;
    }

    const Elf32_Sym *symtab = (const Elf32_Sym *)(elf_bytes + symtab_sh->sh_offset);
    uint32_t sym_count = symtab_sh->sh_size / sizeof(Elf32_Sym);
    const char *strtab = (const char *)(elf_bytes + strtab_sh->sh_offset);
    uint32_t strtab_size = strtab_sh->sh_size;

    for (uint32_t i = 0; i < sym_count; ++i)
    {
        const Elf32_Sym *sym = &symtab[i];
        const char *sym_name = strtab_string(strtab, strtab_size, sym->st_name);
        if (sym_name == NULL)
            continue;

        if (!str_equal(sym_name, symbol_name))
            continue;

        for (uint32_t j = 0; j < section_count; ++j)
        {
            if (section_mappings[j].section_index == sym->st_shndx)
            {
                uint32_t resolved = section_mappings[j].section_base_vaddr + sym->st_value;
                printk(LOG_DEBUG, "lookup_module_symbol: found '%s' at 0x%08x", symbol_name, resolved);
                return resolved;
            }
        }

        printk(LOG_WARNING, "lookup_module_symbol: symbol '%s' found but section not mapped", symbol_name);
        return 0;
    }

    printk(LOG_DEBUG, "lookup_module_symbol: symbol '%s' not found in module");
    return 0;
}

static uint32_t resolve_symbol_value(const Elf32_Sym *sym, uint32_t sym_index,
                                     const section_mapping_t *section_mappings, uint32_t section_count,
                                     const char *strtab, uint32_t strtab_size)
{
    (void)sym_index;
    if (sym->st_shndx == 0)
    {
        const char *sym_name = strtab_string(strtab, strtab_size, sym->st_name);
        if (sym_name == NULL)
        {
            printk(LOG_ERROR, "resolve_symbol_value: invalid symbol name offset");
            return 0;
        }

        void *kernel_sym = module_get_symbol(sym_name);
        if (kernel_sym == NULL)
        {
            printk(LOG_WARNING, "resolve_symbol_value: undefined symbol '%s' not found in kernel", sym_name);
            return 0;
        }

        printk(LOG_DEBUG, "resolve_symbol_value: resolved external symbol '%s' to 0x%08x", sym_name, (uint32_t)kernel_sym);
        return (uint32_t)kernel_sym;
    }

    for (uint32_t i = 0; i < section_count; ++i)
    {
        if (section_mappings[i].section_index == sym->st_shndx)
        {
            uint32_t resolved = section_mappings[i].section_base_vaddr + sym->st_value;
            printk(LOG_DEBUG, "resolve_symbol_value: resolved local symbol to 0x%08x", resolved);
            return resolved;
        }
    }

    printk(LOG_WARNING, "resolve_symbol_value: symbol section %u not found in section mappings", sym->st_shndx);
    return 0;
}

static int process_relocations(module_t *module, const Elf32_Ehdr *ehdr, const uint8_t *elf_bytes,
                               size_t elf_size, const section_mapping_t *section_mappings, uint32_t section_count)
{
    (void)module;
    const Elf32_Shdr *shstr_sh = section_header(ehdr, elf_bytes, ehdr->e_shstrndx);
    if (shstr_sh == NULL || shstr_sh->sh_offset + shstr_sh->sh_size > elf_size)
    {
        printk(LOG_ERROR, "process_relocations: invalid .shstrtab");
        return -1;
    }

    const char *shstrtab = (const char *)(elf_bytes + shstr_sh->sh_offset);
    uint32_t shstrtab_size = shstr_sh->sh_size;

    const Elf32_Shdr *symtab_sh = NULL;
    const Elf32_Shdr *strtab_sh = NULL;
    const char *strtab = NULL;
    uint32_t strtab_size = 0;
    const Elf32_Sym *symtab = NULL;
    uint32_t symtab_entcount = 0;

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *sh = section_header(ehdr, elf_bytes, i);
        if (sh == NULL)
            continue;

        if (sh->sh_type == SHT_SYMTAB)
        {
            symtab_sh = sh;
        }
        else if (sh->sh_type == SHT_STRTAB)
        {
            const char *name = section_name(sh, shstrtab, shstrtab_size);
            if (name != NULL && str_equal(name, ".strtab"))
                strtab_sh = sh;
        }
    }

    if (symtab_sh != NULL)
    {
        if (symtab_sh->sh_offset + symtab_sh->sh_size > elf_size)
        {
            printk(LOG_ERROR, "process_relocations: symtab out of range");
            return -1;
        }

        symtab = (const Elf32_Sym *)(elf_bytes + symtab_sh->sh_offset);
        symtab_entcount = symtab_sh->sh_size / sizeof(Elf32_Sym);
    }

    if (strtab_sh != NULL)
    {
        if (strtab_sh->sh_offset + strtab_sh->sh_size > elf_size)
        {
            printk(LOG_ERROR, "process_relocations: strtab out of range");
            return -1;
        }

        strtab = (const char *)(elf_bytes + strtab_sh->sh_offset);
        strtab_size = strtab_sh->sh_size;
    }

    uint32_t relocation_count = 0;

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *rel_sh = section_header(ehdr, elf_bytes, i);
        if (rel_sh == NULL || rel_sh->sh_type != SHT_REL)
            continue;

        if (symtab == NULL || symtab_sh == NULL)
        {
            printk(LOG_WARNING, "process_relocations: SHT_REL found but no SHT_SYMTAB");
            continue;
        }

        uint32_t target_section = rel_sh->sh_info;
        if (target_section >= ehdr->e_shnum)
        {
            printk(LOG_WARNING, "process_relocations: relocation target section %u out of range", target_section);
            continue;
        }

        const Elf32_Rel *relocations = (const Elf32_Rel *)(elf_bytes + rel_sh->sh_offset);
        uint32_t reloc_count = rel_sh->sh_size / sizeof(Elf32_Rel);

        uint32_t target_base_vaddr = 0;
        for (uint32_t j = 0; j < section_count; ++j)
        {
            if (section_mappings[j].section_index == target_section)
            {
                target_base_vaddr = section_mappings[j].section_base_vaddr;
                break;
            }
        }

        if (target_base_vaddr == 0)
        {
            printk(LOG_WARNING, "process_relocations: target section %u not found in mappings", target_section);
            continue;
        }

        for (uint32_t j = 0; j < reloc_count; ++j)
        {
            const Elf32_Rel *rel = &relocations[j];
            uint32_t sym_index = elf32_r_sym(rel->r_info);
            uint32_t rel_type = elf32_r_type(rel->r_info);

            if (rel_type != R_386_32 && rel_type != R_386_PC32 && rel_type != R_386_PLT32)
                continue;

            if (sym_index >= symtab_entcount)
            {
                printk(LOG_ERROR, "process_relocations: symbol index %u out of range", sym_index);
                return -1;
            }

            const Elf32_Sym *sym = &symtab[sym_index];
            uint32_t sym_value = resolve_symbol_value(sym, sym_index, section_mappings, section_count, strtab, strtab_size);
            if (sym_value == 0 && sym->st_shndx == 0)
            {
                printk(LOG_ERROR, "process_relocations: failed to resolve symbol index %u", sym_index);
                return -1;
            }

            uint32_t reloc_target_va = target_base_vaddr + rel->r_offset;
            uint32_t addend = *(uint32_t *)reloc_target_va;
            uint32_t relocated_value = 0;

            if (rel_type == R_386_32)
            {
                relocated_value = sym_value + addend;
            }
            else if (rel_type == R_386_PC32 || rel_type == R_386_PLT32)
            {
                relocated_value = sym_value + addend - reloc_target_va;
            }
            else
            {
                continue;
            }

            *(uint32_t *)reloc_target_va = relocated_value;
            relocation_count++;
        }
    }

    printk(LOG_INFO, "process_relocations: applied %u relocations", relocation_count);
    return 0;
}

int module_loader_load(module_t *module, const void *elf_data, size_t elf_size)
{
    if (module == NULL || elf_data == NULL || elf_size < sizeof(Elf32_Ehdr))
    {
        printk(LOG_ERROR, "module_loader_load: invalid arguments");
        return -1;
    }

    const uint8_t *elf_bytes = (const uint8_t *)elf_data;
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)elf_bytes;

    if (!validate_elf_header(ehdr, elf_size))
    {
        printk(LOG_ERROR, "module_loader_load: invalid ELF header");
        return -1;
    }

    const Elf32_Shdr *shstr_sh = section_header(ehdr, elf_bytes, ehdr->e_shstrndx);
    if (shstr_sh == NULL || shstr_sh->sh_offset + shstr_sh->sh_size > elf_size)
    {
        printk(LOG_ERROR, "module_loader_load: missing .shstrtab");
        return -1;
    }

    const char *shstrtab = (const char *)(elf_bytes + shstr_sh->sh_offset);
    uint32_t shstrtab_size = shstr_sh->sh_size;

    const Elf32_Shdr *strtab_sh = NULL;
    const Elf32_Shdr *symtab_sh = NULL;

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *sh = section_header(ehdr, elf_bytes, i);
        const char *name = section_name(sh, shstrtab, shstrtab_size);
        if (name == NULL)
            continue;

        if (str_equal(name, ".strtab"))
            strtab_sh = sh;
        else if (str_equal(name, ".symtab"))
            symtab_sh = sh;
    }

    if (strtab_sh == NULL || symtab_sh == NULL)
    {
        printk(LOG_WARNING, "module_loader_load: .strtab or .symtab not found; continuing with section allocation");
    }

    module->page_count = 0;
    module->load_base_vaddr = MODULE_LOAD_BASE;
    module->load_size = 0;

    section_mapping_t section_mappings[64];
    uint32_t section_mapping_count = 0;

    uint32_t current_offset = 0;

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *sh = section_header(ehdr, elf_bytes, i);
        if (sh == NULL)
            continue;

        if (!(sh->sh_flags & SHF_ALLOC) || sh->sh_size == 0)
            continue;

        uint32_t align = sh->sh_addralign ? sh->sh_addralign : PAGE_SIZE;
        if (align < PAGE_SIZE)
            align = PAGE_SIZE;

        uint32_t aligned_offset = (current_offset + align - 1) & ~(align - 1);
        uint32_t section_base = module->load_base_vaddr + aligned_offset;
        uint32_t pages = (sh->sh_size + PAGE_SIZE - 1) / PAGE_SIZE;

        if (module->page_count + pages > MAX_MODULE_PAGES)
        {
            printk(LOG_ERROR, "module_loader_load: too many module pages");
            cleanup_module_frames(module);
            module->state = MODULE_STATE_FAILED;
            return -1;
        }

        uint32_t first_frame_index = module->page_count;
        if (!allocate_module_frames(module, pages))
        {
            printk(LOG_ERROR, "module_loader_load: physical frame allocation failed");
            cleanup_module_frames(module);
            module->state = MODULE_STATE_FAILED;
            return -1;
        }

        for (uint32_t page = 0; page < pages; ++page)
        {
            uint32_t page_vaddr = section_base + page * PAGE_SIZE;
            uint32_t page_paddr = module->frames[first_frame_index + page];
            vmm_map_kernel_page(page_vaddr, page_paddr);
        }

        if (sh->sh_type == SHT_NOBITS)
        {
            uint8_t *dest = (uint8_t *)section_base;
            memset(dest, 0, pages * PAGE_SIZE);
        }
        else
        {
            if (sh->sh_offset + sh->sh_size > elf_size)
            {
                printk(LOG_ERROR, "module_loader_load: section data out of range");
                cleanup_module_frames(module);
                module->state = MODULE_STATE_FAILED;
                return -1;
            }

            const uint8_t *src = elf_bytes + sh->sh_offset;
            uint8_t *dest = (uint8_t *)section_base;
            for (uint32_t byte = 0; byte < sh->sh_size; ++byte)
            {
                dest[byte] = src[byte];
            }

            if (sh->sh_size < pages * PAGE_SIZE)
                memset(dest + sh->sh_size, 0, pages * PAGE_SIZE - sh->sh_size);
        }

        if (section_mapping_count < 64)
        {
            section_mappings[section_mapping_count].section_index = i;
            section_mappings[section_mapping_count].section_base_vaddr = section_base;
            section_mapping_count++;
        }

        current_offset = aligned_offset + pages * PAGE_SIZE;
    }

    module->load_size = current_offset;
    printk(LOG_INFO, "module_loader_load: loaded module sections at 0x%08x size %u pages %u",
           module->load_base_vaddr, module->load_size, module->page_count);

    if (module->page_count == 0)
    {
        printk(LOG_WARNING, "module_loader_load: no allocatable sections found");
    }

    if (process_relocations(module, ehdr, elf_bytes, elf_size, section_mappings, section_mapping_count) != 0)
    {
        printk(LOG_ERROR, "module_loader_load: relocation processing failed");
        cleanup_module_frames(module);
        module->state = MODULE_STATE_FAILED;
        return -1;
    }

    // Discover module entry points from the loaded module's symbol table
    printk(LOG_INFO, "module_loader_load: discovering module entry points");

    // Look up module_init_fn - function pointer
    uint32_t init_fn_addr = lookup_module_symbol("module_init_fn", ehdr, elf_bytes, elf_size, section_mappings, section_mapping_count);
    if (init_fn_addr != 0)
    {
        module->init = *(module_init_func_t *)init_fn_addr;
        printk(LOG_INFO, "module_loader_load: module_init_fn discovered at 0x%08x", init_fn_addr);
    }

    // Look up module_exit_fn - function pointer
    uint32_t exit_fn_addr = lookup_module_symbol("module_exit_fn", ehdr, elf_bytes, elf_size, section_mappings, section_mapping_count);
    if (exit_fn_addr != 0)
    {
        module->exit = *(module_exit_func_t *)exit_fn_addr;
        printk(LOG_INFO, "module_loader_load: module_exit_fn discovered at 0x%08x", exit_fn_addr);
    }

    // Look up module_name_str - string pointer
    uint32_t name_str_addr = lookup_module_symbol("module_name_str", ehdr, elf_bytes, elf_size, section_mappings, section_mapping_count);
    if (name_str_addr != 0)
    {
        module->name = *(const char **)name_str_addr;
        printk(LOG_INFO, "module_loader_load: module_name_str discovered at 0x%08x -> '%s'", name_str_addr, module->name ? module->name : "(null)");
    }

    // Look up module_version_str - string pointer
    uint32_t version_str_addr = lookup_module_symbol("module_version_str", ehdr, elf_bytes, elf_size, section_mappings, section_mapping_count);
    if (version_str_addr != 0)
    {
        module->version = *(const char **)version_str_addr;
        printk(LOG_INFO, "module_loader_load: module_version_str discovered at 0x%08x -> '%s'", version_str_addr, module->version ? module->version : "(null)");
    }

    module->state = MODULE_STATE_LOADED;
    printk(LOG_INFO, "module_loader_load: module loaded, relocated, and entry points discovered");

    return 0;
}

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include <kernel/elf_loader.h>
#include <kernel/log.h>
#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>

#define ELF_TEMP_PAGE 0xC0800000

#define EI_NIDENT 16

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define EV_CURRENT 1

#define ET_EXEC 2
#define EM_386 3

#define PT_NULL 0
#define PT_LOAD 1

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

#define SHT_NULL 0
#define SHT_PROGBITS 1
#define SHT_SYMTAB 2
#define SHT_STRTAB 3
#define SHT_RELA 4
#define SHT_HASH 5
#define SHT_DYNAMIC 6
#define SHT_NOTE 7
#define SHT_NOBITS 8
#define SHT_REL 9

#define SHF_WRITE 0x1
#define SHF_ALLOC 0x2
#define SHF_EXECINSTR 0x4

#define R_386_NONE 0
#define R_386_32 1
#define R_386_PC32 2

#define SHN_UNDEF 0
#define SHN_ABS 0xFFF1

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
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

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

static inline uint32_t elf32_r_sym(uint32_t info)
{
    return info >> 8;
}

static inline uint32_t elf32_r_type(uint32_t info)
{
    return info & 0xFF;
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
        ehdr->e_version != EV_CURRENT || ehdr->e_type != ET_EXEC ||
        ehdr->e_machine != EM_386)
    {
        return false;
    }

    if (ehdr->e_phoff == 0 || ehdr->e_phnum == 0 || ehdr->e_phentsize != sizeof(Elf32_Phdr))
        return false;

    if ((uint32_t)ehdr->e_phoff + (uint32_t)ehdr->e_phnum * sizeof(Elf32_Phdr) > elf_size)
        return false;

    if (ehdr->e_shoff == 0 || ehdr->e_shnum == 0 || ehdr->e_shentsize != sizeof(Elf32_Shdr))
        return false;

    if ((uint32_t)ehdr->e_shoff + (uint32_t)ehdr->e_shnum * sizeof(Elf32_Shdr) > elf_size)
        return false;

    if (ehdr->e_shstrndx >= ehdr->e_shnum)
        return false;

    return true;
}

static const Elf32_Phdr *program_header(const Elf32_Ehdr *ehdr, const uint8_t *elf_bytes, uint32_t index)
{
    if (index >= ehdr->e_phnum)
        return NULL;

    return (const Elf32_Phdr *)(elf_bytes + ehdr->e_phoff + index * sizeof(Elf32_Phdr));
}

static const Elf32_Shdr *section_header(const Elf32_Ehdr *ehdr, const uint8_t *elf_bytes, uint32_t index)
{
    if (index >= ehdr->e_shnum)
        return NULL;

    return (const Elf32_Shdr *)(elf_bytes + ehdr->e_shoff + index * sizeof(Elf32_Shdr));
}

static const char *section_name(const Elf32_Shdr *sh, const char *shstrtab, uint32_t shstrtab_size)
{
    if (sh == NULL || shstrtab == NULL)
        return NULL;

    if (sh->sh_name >= shstrtab_size)
        return NULL;

    return shstrtab + sh->sh_name;
}

static uint32_t page_align_down(uint32_t addr)
{
    return addr & ~(PAGE_SIZE - 1);
}

static uint32_t page_align_up(uint32_t addr)
{
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static bool str_equal(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b)
    {
        a++;
        b++;
    }

    return *a == *b;
}

typedef struct elf_page_mapping
{
    uint32_t vaddr;
    uint32_t paddr;
} elf_page_mapping_t;

static uint32_t get_frame_for_vaddr(const elf_page_mapping_t *pages,
                                    uint32_t count,
                                    uint32_t vaddr)
{
    uint32_t page_base = page_align_down(vaddr);
    for (uint32_t i = 0; i < count; ++i)
    {
        if (pages[i].vaddr == page_base)
            return pages[i].paddr;
    }
    return 0;
}

static uint32_t resolve_symbol_address(const Elf32_Sym *sym,
                                       const Elf32_Ehdr *ehdr,
                                       const uint8_t *elf_bytes)
{
    if (sym == NULL)
        return 0;

    if (sym->st_shndx == SHN_UNDEF)
        return 0;

    if (sym->st_shndx == SHN_ABS)
        return sym->st_value;

    if (sym->st_shndx >= ehdr->e_shnum)
        return 0;

    const Elf32_Shdr *target_sh = section_header(ehdr, elf_bytes, sym->st_shndx);
    if (target_sh == NULL)
        return 0;

    return target_sh->sh_addr + sym->st_value;
}

static int process_relocations(const Elf32_Ehdr *ehdr,
                               const uint8_t *elf_bytes,
                               size_t elf_size,
                               const Elf32_Shdr *symtab_sh,
                               const Elf32_Shdr *strtab_sh,
                               const char *strtab,
                               uint32_t strtab_size,
                               const Elf32_Shdr *shstr_sh,
                               const char *shstrtab,
                               const uint32_t user_pdt,
                               const elf_page_mapping_t *pages,
                               uint32_t page_count)
{
    (void)strtab_size;
    (void)shstr_sh;
    (void)shstrtab;
    (void)user_pdt;

    (void)strtab_size;
    (void)shstr_sh;
    (void)shstrtab;
    (void)user_pdt;

    if (symtab_sh == NULL || strtab_sh == NULL || strtab == NULL)
        return 0;

    const Elf32_Sym *symtab = (const Elf32_Sym *)(elf_bytes + symtab_sh->sh_offset);
    uint32_t sym_count = symtab_sh->sh_size / sizeof(Elf32_Sym);

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *rel_sh = section_header(ehdr, elf_bytes, i);
        if (rel_sh == NULL || rel_sh->sh_type != SHT_REL)
            continue;

        if (rel_sh->sh_offset + rel_sh->sh_size > elf_size)
            return -1;

        const Elf32_Shdr *target_sh = section_header(ehdr, elf_bytes, rel_sh->sh_info);
        if (target_sh == NULL || !(target_sh->sh_flags & SHF_ALLOC))
            continue;

        const Elf32_Rel *relocations = (const Elf32_Rel *)(elf_bytes + rel_sh->sh_offset);
        uint32_t reloc_count = rel_sh->sh_size / sizeof(Elf32_Rel);

        for (uint32_t j = 0; j < reloc_count; ++j)
        {
            const Elf32_Rel *rel = &relocations[j];
            uint32_t sym_index = elf32_r_sym(rel->r_info);
            uint32_t rel_type = elf32_r_type(rel->r_info);

            if (sym_index >= sym_count)
            {
                printk(LOG_ERROR, "load_elf: relocation symbol index %u out of range", sym_index);
                return -1;
            }

            const Elf32_Sym *sym = &symtab[sym_index];
            uint32_t sym_addr = resolve_symbol_address(sym, ehdr, elf_bytes);
            if (sym_addr == 0)
            {
                printk(LOG_ERROR, "load_elf: undefined relocation symbol index %u", sym_index);
                return -1;
            }

            uint32_t target_va = target_sh->sh_addr + rel->r_offset;
            uint32_t frame_paddr = get_frame_for_vaddr(pages, page_count, target_va);
            if (frame_paddr == 0)
            {
                printk(LOG_ERROR, "load_elf: relocation target 0x%08x not mapped", target_va);
                return -1;
            }

            vmm_map_kernel_page(ELF_TEMP_PAGE, frame_paddr);
            uint32_t page_offset = target_va & (PAGE_SIZE - 1);
            uint32_t addend = *(uint32_t *)(ELF_TEMP_PAGE + page_offset);
            uint32_t relocated = 0;

            if (rel_type == R_386_32)
            {
                relocated = sym_addr + addend;
            }
            else if (rel_type == R_386_PC32)
            {
                relocated = sym_addr + addend - target_va;
            }
            else
            {
                vmm_unmap_page(ELF_TEMP_PAGE);
                continue;
            }

            *(uint32_t *)(ELF_TEMP_PAGE + page_offset) = relocated;
            vmm_unmap_page(ELF_TEMP_PAGE);
        }
    }

    return 0;
}

int load_elf(const void *elf_data,
             size_t elf_size,
             uint32_t user_pdt,
             uint32_t *entry_point,
             uint32_t *page_frames,
             uint32_t *page_count)
{
    if (elf_data == NULL || elf_size < sizeof(Elf32_Ehdr) || user_pdt == 0 || entry_point == NULL || page_frames == NULL || page_count == NULL)
    {
        return -1;
    }

    const uint8_t *elf_bytes = (const uint8_t *)elf_data;
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)elf_bytes;

    if (!validate_elf_header(ehdr, elf_size))
    {
        printk(LOG_ERROR, "load_elf: invalid ELF header");
        return -1;
    }

    const Elf32_Shdr *shstr_sh = section_header(ehdr, elf_bytes, ehdr->e_shstrndx);
    if (shstr_sh == NULL || shstr_sh->sh_offset + shstr_sh->sh_size > elf_size)
    {
        printk(LOG_ERROR, "load_elf: invalid section header string table");
        return -1;
    }

    const char *shstrtab = (const char *)(elf_bytes + shstr_sh->sh_offset);
    uint32_t shstrtab_size = shstr_sh->sh_size;

    const Elf32_Shdr *symtab_sh = NULL;
    const Elf32_Shdr *strtab_sh = NULL;
    const char *strtab = NULL;
    uint32_t strtab_size = 0;

    for (uint32_t i = 0; i < ehdr->e_shnum; ++i)
    {
        const Elf32_Shdr *sh = section_header(ehdr, elf_bytes, i);
        if (sh == NULL)
            continue;

        const char *name = section_name(sh, shstrtab, shstrtab_size);
        if (name == NULL)
            continue;

        if (sh->sh_type == SHT_SYMTAB)
            symtab_sh = sh;
        else if (sh->sh_type == SHT_STRTAB && str_equal(name, ".strtab"))
            strtab_sh = sh;
    }

    if (strtab_sh != NULL)
    {
        if (strtab_sh->sh_offset + strtab_sh->sh_size > elf_size)
        {
            printk(LOG_ERROR, "load_elf: invalid .strtab");
            return -1;
        }

        strtab = (const char *)(elf_bytes + strtab_sh->sh_offset);
        strtab_size = strtab_sh->sh_size;
    }

    uint32_t loaded_pages_count = 0;
    elf_page_mapping_t loaded_pages[MAX_USER_CODE_PAGES];

    /* Load PT_LOAD segments into the user page directory */
    for (uint32_t i = 0; i < ehdr->e_phnum; ++i)
    {
        const Elf32_Phdr *ph = program_header(ehdr, elf_bytes, i);
        if (ph == NULL || ph->p_type != PT_LOAD || ph->p_memsz == 0)
            continue;

        if ((uint32_t)ph->p_offset + ph->p_filesz > elf_size)
        {
            printk(LOG_ERROR, "load_elf: segment data out of range");
            return -1;
        }

        uint32_t segment_start = ph->p_vaddr;
        uint32_t segment_end = ph->p_vaddr + ph->p_memsz;
        uint32_t page_start = page_align_down(segment_start);
        uint32_t page_end = page_align_up(segment_end);

        uint32_t page_flags = PAGE_PRESENT | PAGE_USER;
        if (ph->p_flags & PF_W)
            page_flags |= PAGE_WRITE;

        for (uint32_t vaddr = page_start; vaddr < page_end; vaddr += PAGE_SIZE)
        {
            if (loaded_pages_count >= MAX_USER_CODE_PAGES)
            {
                printk(LOG_ERROR, "load_elf: too many user code pages");
                return -1;
            }

            uint32_t frame = (uint32_t)(uintptr_t)pmm_alloc_frame();
            if (frame == 0)
            {
                printk(LOG_ERROR, "load_elf: physical frame allocation failed");
                return -1;
            }

            vmm_map_page_for_pdt(user_pdt, vaddr, frame, page_flags);
            vmm_map_kernel_page(ELF_TEMP_PAGE, frame);

            uint8_t *dest = (uint8_t *)ELF_TEMP_PAGE;
            memset(dest, 0, PAGE_SIZE);

            uint32_t offset_in_segment = 0;
            if (vaddr < segment_start)
            {
                offset_in_segment = segment_start - vaddr;
            }

            if (offset_in_segment < ph->p_filesz)
            {
                uint32_t copy_start = vaddr + offset_in_segment - segment_start;
                uint32_t copy_len = ph->p_filesz - copy_start;
                if (copy_len > PAGE_SIZE - offset_in_segment)
                    copy_len = PAGE_SIZE - offset_in_segment;

                const uint8_t *src_data = elf_bytes + ph->p_offset + copy_start;
                for (uint32_t byte = 0; byte < copy_len; ++byte)
                {
                    dest[offset_in_segment + byte] = src_data[byte];
                }
            }

            vmm_unmap_page(ELF_TEMP_PAGE);

            loaded_pages[loaded_pages_count].vaddr = vaddr;
            loaded_pages[loaded_pages_count].paddr = frame;
            page_frames[loaded_pages_count] = frame;
            loaded_pages_count++;
        }
    }

    if (process_relocations(ehdr, elf_bytes, elf_size, symtab_sh, strtab_sh, strtab, strtab_size, shstr_sh, shstrtab, user_pdt, loaded_pages, loaded_pages_count) != 0)
    {
        for (uint32_t i = 0; i < loaded_pages_count; ++i)
            pmm_free_frame((void *)loaded_pages[i].paddr);
        return -1;
    }

    *entry_point = ehdr->e_entry;
    *page_count = loaded_pages_count;
    return 0;
}

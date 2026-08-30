// ApexKernel/Arch/x86_64/MTRR.c
// ApexOS -- Memory Type Range Registers (variable-range MTRR setup)

#include "Arch/x86_64/MTRR.h"
#include "Logs/Log.h"

#define MSR_IA32_MTRRCAP 0xFE
#define MSR_IA32_MTRR_DEF_TYPE 0x2FF
#define MSR_IA32_MTRR_PHYSBASE0 0x200 // PHYSBASEn/PHYSMASKn pairs are
#define MSR_IA32_MTRR_PHYSMASK0 0x201 // interleaved: base0, mask0, base1, mask1, ...

#define MTRRCAP_VCNT_MASK 0xFF // bits 0-7: number of variable-range register pairs
#define MTRRCAP_WC_SUPPORTED (1ULL << 10)

#define MTRR_PHYSMASK_VALID (1ULL << 11)

// CPUID.01H:EDX bit 12 -- MTRR support.
#define CPUID_FEAT_EDX_MTRR (1 << 12)

static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf));
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t val)
{
    uint32_t lo = (uint32_t)val;
    uint32_t hi = (uint32_t)(val >> 32);
    __asm__ volatile("wrmsr" ::"c"(msr), "a"(lo), "d"(hi));
}

static inline uint64_t read_cr0(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr0" ::"r"(val));
}

static inline uint64_t read_cr3(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void write_cr3(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr3" ::"r"(val));
}

static inline uint64_t read_cr4(void)
{
    uint64_t val;
    __asm__ volatile("mov %%cr4, %0" : "=r"(val));
    return val;
}

static inline void write_cr4(uint64_t val)
{
    __asm__ volatile("mov %0, %%cr4" ::"r"(val));
}

static bool mtrr_supported(void)
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    return (edx & CPUID_FEAT_EDX_MTRR) != 0;
}

// Rounds size up to the next power of two -- variable-range MTRRs can
// only describe power-of-two-sized, naturally-aligned regions (the
// PhysMask mechanism is a bitmask test, not an arbitrary range check:
// Address_Within_Range AND PhysMask == PhysBase AND PhysMask -- see
// MTRR.h). A framebuffer is rarely an exact power of two (e.g.
// 1600x900x4 bytes/pixel = 5,760,000 bytes), so the WC region ends up
// somewhat larger than the framebuffer itself; the extra bytes belong
// to whatever physical memory follows it, which becoming WC too is
// harmless for a dedicated VRAM-adjacent range on real hardware and a
// non-issue under QEMU/virtualized memory maps.
static uint64_t round_up_pow2(uint64_t size)
{
    if (size == 0)
        return 0;
    uint64_t p = 1;
    while (p < size)
        p <<= 1;
    return p;
}

bool mtrr_set_range(uint64_t phys_base, uint64_t size, uint8_t type)
{
    if (!mtrr_supported())
    {
        klog_warn("mtrr_set_range: CPU does not report MTRR support (CPUID.01H:EDX.MTRR) -- skipping");
        return false;
    }

    uint64_t mtrrcap = rdmsr(MSR_IA32_MTRRCAP);
    uint32_t vcnt = (uint32_t)(mtrrcap & MTRRCAP_VCNT_MASK);

    if (type == MTRR_TYPE_WC && !(mtrrcap & MTRRCAP_WC_SUPPORTED))
    {
        klog_warn("mtrr_set_range: CPU does not support the WC memory type (IA32_MTRRCAP bit 10 clear) -- skipping");
        return false;
    }

    uint64_t aligned_size = round_up_pow2(size);
    // Natural alignment requirement: the base address must itself be
    // a multiple of the (rounded-up) size, or no single PhysMask
    // value can express "everything in this range and nothing else"
    // -- see the AND-mask relationship in MTRR.h's comment. Framebuffer
    // physical bases from VBE/firmware aren't guaranteed to already
    // satisfy this, so this isn't just a sanity check on caller
    // input: it's a real case that needs handling, not one that
    // "shouldn't happen".
    if (phys_base % aligned_size != 0)
    {
        // Try growing the range until an aligned power-of-two window
        // starting at or before phys_base and covering [phys_base,
        // phys_base + size) exists. Doubling at most a handful of
        // times before giving up -- past that the WC region would
        // cover an unreasonable amount of unrelated physical memory.
        for (int attempt = 0; attempt < 8; attempt++)
        {
            uint64_t candidate_base = phys_base & ~(aligned_size - 1);
            if (candidate_base + aligned_size >= phys_base + size)
            {
                phys_base = candidate_base;
                break;
            }
            aligned_size <<= 1;
        }
        if (phys_base % aligned_size != 0)
        {
            klog_warn("mtrr_set_range: could not find an aligned power-of-two range covering the requested region -- skipping");
            return false;
        }
    }

    // Find a free variable-range register pair -- PhysMask's V (valid)
    // bit, bit 11, is 0 on any pair the firmware/OS hasn't already
    // claimed.
    int free_reg = -1;
    for (uint32_t i = 0; i < vcnt; i++)
    {
        uint64_t physmask = rdmsr(MSR_IA32_MTRR_PHYSMASK0 + i * 2);
        if (!(physmask & MTRR_PHYSMASK_VALID))
        {
            free_reg = (int)i;
            break;
        }
    }
    if (free_reg < 0)
    {
        klog_warn("mtrr_set_range: no free variable-range MTRR register pair -- skipping");
        return false;
    }

    // PhysMask covers bits 12 and up of the physical address
    // (MAXPHYADDR-1) -- everything below bit 12 is implicitly part of
    // the 4KB page granularity MTRRs operate at. ~(aligned_size - 1)
    // is exactly this mask for a power-of-two-sized, aligned range.
    // The mask is intentionally not clamped to a specific MAXPHYADDR
    // width here -- undefined high bits above what the CPU actually
    // implements are simply ignored by hardware (reserved-but-ignored
    // for this MSR, unlike some others that #GP on set reserved
    // bits), so this stays correct without needing a CPUID leaf
    // 80000008H query for MAXPHYADDR just for this.
    uint64_t phys_mask = ~(aligned_size - 1) & 0x000FFFFFFFFFF000ULL;

    // ---- Intel-recommended safe MTRR programming sequence ----
    // (SDM Vol 3, 11.11.7 "MTRR Considerations in MP Systems" /
    // 11.5.3 "Preventing Caching") -- writing MTRRs while the cache
    // is active and other memory types might already be cached for
    // this range risks the CPU observing inconsistent data for the
    // same physical line under two memory types at once. Single-CPU
    // sequence (this kernel has no SMP support yet -- see roadmap):
    //   1. Disable interrupts.
    //   2. Enter no-fill cache mode (CR0.CD=1, CR0.NW=0).
    //   3. Flush all caches (WBINVD).
    //   4. Disable global pages (CR4.PGE) and flush the TLB, so no
    //      stale translation survives with the old memory type baked
    //      into a cached PTE walk.
    //   5. Write the new MTRR.
    //   6. Flush the TLB again, restore CR4.PGE.
    //   7. Flush caches again (WBINVD) -- the SDM calls this out
    //      separately because of aggressive prefetch during step 5-6.
    //   8. Re-enable caching (CR0.CD=0).
    //   9. Restore interrupts.
    __asm__ volatile("cli");

    uint64_t cr0 = read_cr0();
    uint64_t cr0_nofill = (cr0 | (1ULL << 30)) & ~(1ULL << 29); // CD=1, NW=0
    write_cr0(cr0_nofill);
    __asm__ volatile("wbinvd");

    uint64_t cr4 = read_cr4();
    bool pge_was_set = (cr4 & (1ULL << 7)) != 0;
    if (pge_was_set)
        write_cr4(cr4 & ~(1ULL << 7));
    write_cr3(read_cr3()); // reload CR3: flushes the TLB

    wrmsr(MSR_IA32_MTRR_PHYSBASE0 + free_reg * 2, (phys_base & 0x000FFFFFFFFFF000ULL) | type);
    wrmsr(MSR_IA32_MTRR_PHYSMASK0 + free_reg * 2, phys_mask | MTRR_PHYSMASK_VALID);

    write_cr3(read_cr3());
    if (pge_was_set)
        write_cr4(cr4);

    __asm__ volatile("wbinvd");
    write_cr0(cr0); // restores original CD/NW (re-enables caching)

    __asm__ volatile("sti");

    klog_ok_n("mtrr_set_range: WC range configured, register", (uint64_t)free_reg);

    return true;
}

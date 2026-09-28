// ApexKernel/Core/Syscalls/Handlers/SysRead.c
// ApexOS -- SYS_READ handler

#include "Core/Syscalls/Handlers/SysRead.h"
#include "Core/Syscalls/Validation/UserPointer.h"
#include "Drivers/Input/Keyboard/KeyBuffer.h"
#include "Core/Interrupts/PIC/PICControl.h"

uint64_t sys_read(uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
    if (arg1 != 0) // Only stdin supported in V1 -- see SysRead.h's own comment.
        return (uint64_t)-1;

    uint8_t *buf = (uint8_t *)arg2;
    uint64_t len = arg3;

    if (len == 0)
        return 0;

    // arg2/arg3 come straight from the calling Ring 3 process,
    // completely unvalidated by SyscallEntry.asm -- without this
    // check, a process could point buf at kernel memory and have this
    // syscall's own buf[0]/buf[count] writes below corrupt it, an
    // arbitrary kernel-memory-write primitive. See UserPointer.h for
    // what counts as a valid range. Checked before the blocking
    // keyboard_getchar() call further down, so an invalid buf never
    // gets a chance to be written to at all.
    if (!is_valid_user_range(arg2, arg3))
        return (uint64_t)-1;

    // Every syscall entry runs with interrupts disabled -- the
    // syscall instruction itself masks IF automatically via
    // Core/Syscalls/ABI/SyscallSetup.c's MSR_SFMASK, specifically to
    // prevent a nested syscall/interrupt reentrancy hazard while the
    // kernel is still on its way into the handler. That's the correct
    // default for every OTHER syscall handler in this kernel (none of
    // which block), but it's fatal here: keyboard_getchar() below
    // busy-waits for Drivers/Input/Keyboard/Keyboard.c's
    // keyboard_handler() to fill the ring buffer, and that handler can
    // only ever run on an IRQ -- which cannot be delivered to this CPU
    // while IF is clear. Without an `sti` here, the very first
    // blocking sys_read() call in this kernel's history deadlocked
    // immediately -- waiting on an interrupt it had itself made
    // impossible to receive.
    //
    // A bare `sti` alone caused a DIFFERENT, more subtle bug once
    // tested for real (confirmed via a live boot: typing "3" "+" "4"
    // into calc.axp -- Formats/AXF/AXP/ProcessLaunch.c's own .axp
    // execution path -- produced "3 + 4 = 3", operand_b silently
    // reading back as 0 despite echoing "4" correctly on screen): `sti`
    // re-enables ALL currently-unmasked interrupt sources, not just
    // the keyboard this handler actually needs, and any OTHER handler
    // firing during this window can touch memory (or CPU state) this
    // one is mid-computation with. IRQ2 (the PIC's own slave cascade
    // wire -- not a real device on its own, but still an unmasked PIC
    // input, see Core/Interrupts/PIC/PICSetup.h) and IRQ12 (the PS/2
    // mouse, actively unmasked in Init.c) were the two PIC-routed
    // sources still unmasked at this point in boot -- IRQ1 itself is
    // no longer PIC-routed at all (migrated to the I/O APIC, see
    // Core/SMP/IOAPIC/IoApic.h), and IRQ0 (timer) is already masked by
    // ProcessLaunch.c's own pic_mask_irq(0) for the entire Ring 3
    // window this syscall runs inside of.
    //
    // Masking both explicitly here (and restoring their PRIOR mask
    // state afterward, not just unconditionally re-unmasking --
    // ProcessLaunch.c already left IRQ2 unmasked before this call, so
    // simply calling pic_unmask_irq() on it after would be correct by
    // coincidence today but wrong if a future caller path reached this
    // function with IRQ2 already masked for its own reasons) leaves
    // IRQ1 (I/O APIC-routed keyboard) as the only interrupt source
    // this CPU can actually take during the window `sti` opens -- IRQ2
    // and IRQ12 (PIC) are explicitly closed, and IRQ0 was already
    // closed before this function was ever called.
    bool was_irq2_masked = pic_irq_is_masked(2);
    bool was_irq12_masked = pic_irq_is_masked(12);
    pic_mask_irq(2);
    pic_mask_irq(12);

    __asm__ volatile("sti");

    // keyboard_getchar() itself blocks (busy-wait) until a key is
    // available -- this first call is what makes sys_read() a
    // blocking syscall from the calling process's point of view. Every
    // call after this one only drains what's ALREADY sitting in the
    // ring buffer (keyboard_has_key()), never blocks again -- see
    // SysRead.h's own comment on why this returns "what's ready right
    // now" rather than waiting to fill the entire buffer.
    buf[0] = keyboard_getchar();
    uint64_t count = 1;

    while (count < len && keyboard_has_key())
    {
        buf[count] = keyboard_getchar();
        count++;
    }

    __asm__ volatile("cli");

    // Restores exactly the mask state each of these two IRQs had
    // before this call, rather than unconditionally unmasking --
    // see this function's own comment above on why.
    if (!was_irq2_masked)
        pic_unmask_irq(2);
    if (!was_irq12_masked)
        pic_unmask_irq(12);

    return count;
}

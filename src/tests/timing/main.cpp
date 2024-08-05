// Copyright © 2024 Cyberus Technology GmbH <contact@cyberus-technology.de>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <toyos/baretest/baretest.hpp>
#include <toyos/x86/x86asm.hpp>

#if 0
TEST_CASE(tsc_is_monotonous)
{
    // Run for at least some minutes.
    static constexpr size_t REPETITIONS{ 200 };

    for (size_t round = 0; round < REPETITIONS; round++) {
        // This _very roughly_ aims for REPETITIONS seconds at this many GHz.
        static constexpr size_t LOOPS{ 1 /* Giga */ * 1000000000 };

        const uint64_t tsc_before{ rdtsc() };

        for (size_t i = 0; i < LOOPS; i++) {
            // During this time, the VM can be paused and unpaused or similar.
            //
            // We cannot use PAUSE here, because its execution time is wildly
            // different for different processors and we are aiming for a
            // somewhat constant delay. Instead we use this empty assembler
            // statement to force the compiler to actually loop.
            asm volatile("");
        }

        const uint64_t tsc_after{ rdtsc() };

        // Be verbose for debugging.
        info("{}: TSC {} - {} = {}", round, tsc_after, tsc_before, tsc_after - tsc_before);

        BARETEST_ASSERT(tsc_before < tsc_after);
    }
}
#endif

TEST_CASE(register_fill)
{
    uint64_t* mem_ptr_0 = (uint64_t*)0x0;
    uint64_t* mem_ptr = (uint64_t*)0x1000;

    //*mem_ptr_0 = 0x00c0fefe13381338;

    for (int i = 0; i < 1000; ++i) {
	*(mem_ptr + i) = 0xdeadbeef13371337ull;
    }


    uint64_t cr0 {get_cr0()};
    uint64_t cr3 {get_cr3()};
    uint64_t cr4 {get_cr4()};
    auto gdtr {get_current_gdtr()};
    auto idtr {get_current_idtr()};
    info("CR0:{x} CR3:{x} CR4:{x} GDTR:{x}+{x} IDTR:{x}+{x} {x}", cr0, cr3, cr4, gdtr.base, gdtr.limit, idtr.base, idtr.limit, *mem_ptr_0);
    //*mem_ptr_0 = 0x00c0fefe13381338;
    asm volatile(
	    "push %%rbp;"
	    "mov $0xa0a0a0a0a0a0a0a0, %%rax;"
	    "mov %%rax, %%cr2;"
	    "mov $0x1111111111111111, %%rax;"
	    "mov $0x2222222222222222, %%rbx;"
	    "mov $0x3333333333333333, %%rcx;"
	    "mov $0x4444444444444444, %%rdx;"
	    "mov $0x5555555555555555, %%rsi;"
	    "mov $0x6666666666666666, %%rdi;"
	    "mov $0xb1b1b1b1b1b1b1b1, %%r8; "
	    "mov $0x7777777777777777, %%r9; "
	    "mov $0x8888888888888888, %%r10;"
	    "mov $0x9999999999999999, %%r11;"
	    "mov $0xaaaaaaaaaaaaaaaa, %%r12;"
	    "mov $0xbbbbbbbbbbbbbbbb, %%r13;"
	    "mov $0xcccccccccccccccc, %%r14;"
	    "mov $0xdddddddddddddddd, %%r15;"
	    "mov $0xeeeeeeeeeeeeeeee, %%rbp;"
	    "mov $0xc2c2c2c2c2c2c2c2, %%rsp;"
	    "jmp .;"
	    ::: "memory");
}


BARETEST_RUN;

// Copyright © 2024 Cyberus Technology GmbH <contact@cyberus-technology.de>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstddef>
#include <toyos/baretest/baretest.hpp>
#include <toyos/x86/x86asm.hpp>
#include "img.h"
#include "img_sap.h"

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

#if 0
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
        "mov $0xc, %%rax;"
        "mov %%rax, %%cr8;"
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
#endif

#if 0
TEST_CASE(fill_mem_mmap_vmware_2g)
{
    std::pair<uint64_t,uint64_t> LOAD_ADDR {0xc00000, 0xc00000 + 2 * 1024 * 1024};
    std::vector<std::pair<uint64_t, uint64_t>> memmap = {
	    {0x0000000000000000ull, 0x0000000000097bffull},
	    {0x0000000000100000ull, 0x000000007fedffffull},
	    {0x000000007ff00000ull, 0x000000007fffffffull} };

    std::vector<std::pair<uint64_t, uint64_t>> memmap_page_aligned;

    info("Original memory map:");
    for (auto& range : memmap) {
	info("    {016x} - {016x}", range.first, range.second + 1);
	memmap_page_aligned.emplace_back(range.first, (range.second + 1) & ~0xFFFull);
    }

#if 0
    uint64_t total_size {0};
    info("Page aligned memory map:");
    for (auto& range : memmap_page_aligned) {
	info("    {016x} - {016x}", range.first, range.second);
	total_size += range.second - range.first;
    }
    info("Total usable memory: {} which is {} MiB", total_size, total_size / 1024 / 1024);

    info("Writing patterns:");
    for (auto& range : memmap_page_aligned) {
	    uint64_t num_pages {(range.second - range.first) >> 12};
	    info("Filling {016x} - {016x}", range.first, range.second);
	    for (uint64_t page {0}; page < num_pages; page++) {
		uint64_t start_addr {range.first + page * PAGE_SIZE};
		if (start_addr < LOAD_ADDR.first || start_addr > LOAD_ADDR.second) {
		    uint64_t* start_ptr {reinterpret_cast<uint64_t*>(start_addr)};
		    *start_ptr = start_addr;
		    memset(start_ptr + 1, 0, PAGE_SIZE - sizeof(uint64_t));
		}
	    }
    }
    info("Finished");
#endif

    uint16_t* framebuffer = reinterpret_cast<uint16_t*>(0xb8000);
    uint64_t loop {1};

    while (true) {
        for (unsigned i = 0; i < (80 * 25); ++i) {
	    uint8_t col = (((loop % 2) ? pic[i] : pic_sap[i]) % 0x10);
	    framebuffer[i] = uint16_t(col) << 12;

	    for (uint64_t j=0; j<50000ul; j++) {asm("pause");}
        }
	    loop++;
    }

    while (true) {}
}
#endif

#if 0
volatile uint16_t* fb {reinterpret_cast<uint16_t*>(0xb8000)};
static unsigned char foo_str[] = "foo";
static unsigned char bar_str[] = "bar";
volatile unsigned fake {0};

[[gnu::noinline]] void foo()
{
    while (fake == 0) {
        *(fb + 0) = foo_str[0] | (0xF0 << 8);
        *(fb + 1) = foo_str[1] | (0xF0 << 8);
        *(fb + 2) = foo_str[2] | (0xF0 << 8);
    }
}

[[gnu::noinline]] void bar()
{
    while (fake == 0) {
        *(fb + 0) = bar_str[0] | (0xF0 << 8);
        *(fb + 1) = bar_str[1] | (0xF0 << 8);
        *(fb + 2) = bar_str[2] | (0xF0 << 8);
    }
}

TEST_CASE(simple_migrate)
{
    auto tsc {rdtsc()};
    if (tsc & 0x1) {
        foo();
    } else {
        bar();
    }
}
#endif

#if 1
TEST_CASE(fill_vga_pattern_static)
{
    static constexpr char fill_string[] = "Test";

    uint16_t* fb {reinterpret_cast<uint16_t*>(0xb8000)};

    for (unsigned i = 0; i < 2000; ++i) {
        uint16_t ch {uint16_t(uint16_t(fill_string[i % 4]) | (0x70 << 8))};
        *(fb + i) = ch;
    }
    while (true) {
        asm volatile("nop");
    }
}
#endif

BARETEST_RUN;

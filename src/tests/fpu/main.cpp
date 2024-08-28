// Copyright © 2024 Cyberus Technology GmbH <contact@cyberus-technology.de>
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include <toyos/baretest/baretest.hpp>
#include <toyos/testhelper/idt.hpp>
#include <toyos/testhelper/irq_handler.hpp>
#include <toyos/testhelper/irqinfo.hpp>
#include <toyos/util/cast_helpers.hpp>
#include <toyos/util/cpuid.hpp>
#include <toyos/util/trace.hpp>
#include <toyos/x86/x86asm.hpp>
#include <toyos/x86/x86fpu.hpp>

using namespace x86;

static constexpr uint64_t TEST_VAL{ 0x42 };
static constexpr xmm_t TEST_VAL_128{ 0x23, 0x42 };
static constexpr ymm_t TEST_VAL_256{ 0x1111111111111100, 0x2222222222222200, 0x3333333333333300, 0x4444444444444400 };
static constexpr zmm_t TEST_VAL_512{ 0x1111111111111100, 0x2222222222222200, 0x3333333333333300, 0x4444444444444400,
                                     0x5555555555555500, 0x6666666666666600, 0x7777777777777700, 0x8888888888888800 };

static constexpr uint64_t DESTROY_VAL{ ~0ull };
static constexpr xmm_t DESTROY_VAL_128{ DESTROY_VAL, DESTROY_VAL };
static constexpr ymm_t DESTROY_VAL_256{ DESTROY_VAL, DESTROY_VAL, DESTROY_VAL, DESTROY_VAL };
static constexpr zmm_t DESTROY_VAL_512{ DESTROY_VAL, DESTROY_VAL, DESTROY_VAL, DESTROY_VAL, DESTROY_VAL, DESTROY_VAL, DESTROY_VAL, DESTROY_VAL };

#define CHECK_FEATURE(features, f) info(#f ": %u", !!(features & f));

TEST_CASE(vector_support)
{
    auto features1{ cpuid(CPUID_LEAF_FAMILY_FEATURES) };

    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_SSE3);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_SSSE3);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_FMA);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_SSE41);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_SSE42);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_XSAVE);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_OSXSAVE);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_AVX);
    CHECK_FEATURE(features1.ecx, LVL_0000_0001_ECX_F16C);

    info("------");

    CHECK_FEATURE(features1.edx, LVL_0000_0001_EDX_MMX);
    CHECK_FEATURE(features1.edx, LVL_0000_0001_EDX_FXSR);
    CHECK_FEATURE(features1.edx, LVL_0000_0001_EDX_SSE);
    CHECK_FEATURE(features1.edx, LVL_0000_0001_EDX_SSE2);

    auto features7{ cpuid(CPUID_LEAF_EXTENDED_FEATURES) };

    info("------");

    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX2);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512F);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512DQ);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512IFMA);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512PF);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512ER);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512CD);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512BW);
    CHECK_FEATURE(features7.ebx, LVL_0000_0007_EBX_AVX512VL);

    CHECK_FEATURE(features7.ecx, LVL_0000_0007_ECX_AVX512VBMI);
    CHECK_FEATURE(features7.ecx, LVL_0000_0007_ECX_AVX512VPDQ);

    CHECK_FEATURE(features7.edx, LVL_0000_0007_EDX_AVX512QVNNIW);
    CHECK_FEATURE(features7.edx, LVL_0000_0007_EDX_AVX512QFMA);
}

static uint64_t get_supported_xstate()
{
    auto res{ cpuid(CPUID_LEAF_EXTENDED_STATE, CPUID_EXTENDED_STATE_MAIN) };

    return static_cast<uint64_t>(res.edx) << 32 | res.eax;
}

static void print_cpuid(uint32_t leaf, uint32_t subleaf)
{
    const auto res{ cpuid(leaf, subleaf) };

    info("{#08x} {#08x}: eax={#08x} ebx={#08x} ecx={#08x} edx={#08x}", leaf, subleaf, res.eax, res.ebx, res.ecx, res.edx);
}

TEST_CASE_CONDITIONAL(xstate_features, xsave_supported())
{
    uint64_t supported_xstate{ get_supported_xstate() };

    print_cpuid(CPUID_LEAF_EXTENDED_STATE, CPUID_EXTENDED_STATE_MAIN);
    print_cpuid(CPUID_LEAF_EXTENDED_STATE, CPUID_EXTENDED_STATE_SUB);

    info("xstate {x}", supported_xstate);
    for (unsigned bit = 2; bit <= 62; bit++) {
        if (supported_xstate & (static_cast<uint64_t>(1) << bit)) {
            print_cpuid(CPUID_LEAF_EXTENDED_STATE, bit);
        }
    }
}

static uint64_t xsave_mask{ 0 };

alignas(CPU_CACHE_LINE_SIZE) static uint8_t xsave_area[PAGE_SIZE];

void prologue()
{
    // Everything from MMX onwards is only supported if FPU emulation is disabled.
    set_cr0(get_cr0() & ~math::mask_from(cr0::EM));

    if (xsave_supported()) {
        xsave_mask = get_supported_xstate() & XCR0_MASK;

        set_cr4(get_cr4() | math::mask_from(cr4::OSXSAVE, cr4::OSFXSR));
        set_xcr(xsave_mask);

        auto xsave_size{ cpuid(CPUID_LEAF_EXTENDED_STATE, CPUID_EXTENDED_STATE_MAIN).ecx };
        if (xsave_size > sizeof(xsave_area)) {
            baretest::hello(1);
            baretest::fail("Size of XSAVE area greater than allocated space!\n");
            baretest::goodbye();
            disable_interrupts_and_halt();
        }
    }

    asm volatile("fninit");
}

static irqinfo irq_info;
static jmp_buf jump_buffer;

void irq_handler_fn(intr_regs* regs)
{
    irq_info.record(regs->vector, regs->error_code);
    longjmp(jump_buffer, 1);
}
#if 0

TEST_CASE(fxsave_fxrstor_default)
{
    set_mm0(TEST_VAL);

    fxsave(xsave_area);

    set_mm0(DESTROY_VAL);

    fxrstor(xsave_area);

    BARETEST_ASSERT(get_mm0() == TEST_VAL);
}

TEST_CASE_CONDITIONAL(xstate_size_checks, xsave_supported())
{
    // This is the test as seen in the Linux kernel. We basically sum up all the
    // state components and compare it to the maximum size reported by
    // CPUID(0xD).ebx. See arch/x86/kernel/fpu/xstate.c

    auto xsave_size{ cpuid(CPUID_LEAF_EXTENDED_STATE).ebx };
    auto calculated_size{ x86::FXSAVE_AREA_SIZE + x86::XSAVE_HEADER_SIZE };

    for (unsigned feat{ 2 }; feat <= 62; feat++) {
        if (not(xsave_mask & (static_cast<uint64_t>(1) << feat))) {
            continue;
        }

        auto feature_info{ cpuid(CPUID_LEAF_EXTENDED_STATE, feat) };

        // Round the current size up to the offset of this feature.
        calculated_size = feature_info.ebx;

        calculated_size += feature_info.eax;
    }

    info("Reported: {#x} vs. calculated {#x}", xsave_size, calculated_size);
    BARETEST_ASSERT(calculated_size == xsave_size);
}

static bool check_xcr_exception(uint64_t val, uint32_t xcrN = 0)
{
    irq_handler::guard _(irq_handler_fn);
    irq_info.reset();

    if (setjmp(jump_buffer) == 0) {
        set_xcr(val, xcrN);
    }

    return irq_info.valid and irq_info.vec == to_underlying(exception::GP);
}

// We need AVX for this test, so we have one feature to turn off and see the
// state area size shrink.
TEST_CASE_CONDITIONAL(cpuid_reports_xstate_size, xsave_supported() and avx_supported())
{
    uint32_t reported_max_size{ cpuid(CPUID_LEAF_EXTENDED_STATE, CPUID_EXTENDED_STATE_MAIN).ecx };
    auto current_size = []() { return cpuid(CPUID_LEAF_EXTENDED_STATE, CPUID_EXTENDED_STATE_MAIN).ebx; };

    set_xcr(XCR0_FPU | XCR0_SSE);
    uint32_t fpu_size{ current_size() };

    set_xcr(xsave_mask);
    uint32_t all_enabled_size{ current_size() };

    BARETEST_ASSERT(fpu_size < all_enabled_size);
    BARETEST_ASSERT(all_enabled_size <= reported_max_size);
}

TEST_CASE_CONDITIONAL(setting_invalid_xcr0_causes_gp, xsave_supported())
{
    std::vector<uint64_t> values;

    // Clearing bit 0 (Legacy FPU state) is always invalid
    values.push_back(0);

    // If AVX is supported, clearing SSE while enabling AVX is invalid
    if (xsave_mask & XCR0_AVX) {
        values.push_back(XCR0_FPU | XCR0_AVX);
    }

    // If AVX-512 is supported, setting any of those bits while not setting AVX is invalid,
    // as is not setting all of the AVX-512 bits together
    if (xsave_mask & XCR0_OPMASK) {
        values.push_back(XCR0_FPU | XCR0_AVX);
        values.push_back(XCR0_FPU | XCR0_OPMASK | XCR0_ZMM_Hi256 | XCR0_Hi16_ZMM);
        values.push_back(XCR0_FPU | XCR0_OPMASK | XCR0_Hi16_ZMM);
        values.push_back(XCR0_FPU | XCR0_OPMASK | XCR0_ZMM_Hi256);
        values.push_back(XCR0_FPU | XCR0_ZMM_Hi256 | XCR0_Hi16_ZMM);
        values.push_back(XCR0_FPU | XCR0_AVX | XCR0_ZMM_Hi256 | XCR0_Hi16_ZMM);
        values.push_back(XCR0_FPU | XCR0_AVX | XCR0_OPMASK | XCR0_Hi16_ZMM);
        values.push_back(XCR0_FPU | XCR0_AVX | XCR0_OPMASK | XCR0_ZMM_Hi256);
    }

    for (const auto v : values) {
        info("Setting XCR0 to {x}", v);
        BARETEST_ASSERT(check_xcr_exception(v));
    }
}

TEST_CASE_CONDITIONAL(setting_valid_xcr0_works, xsave_supported())
{
    std::vector<uint64_t> values;

    values.push_back(XCR0_FPU);
    values.push_back(XCR0_FPU | XCR0_SSE);

    if (avx_supported()) {
        values.push_back(XCR0_FPU | XCR0_SSE | XCR0_AVX);
    }
    if (avx512_supported()) {
        values.push_back(XCR0_FPU | XCR0_SSE | XCR0_AVX | XCR0_AVX512);
    }

    for (const auto v : values) {
        set_xcr(v);
        BARETEST_ASSERT(get_xcr() == v);
    }

    set_xcr(xsave_mask);
}

TEST_CASE_CONDITIONAL(invalid_xcrN_causes_gp, xsave_supported())
{
    BARETEST_ASSERT(check_xcr_exception(XCR0_FPU, 1));
}

TEST_CASE_CONDITIONAL(xsave_xrstor_full, xsave_supported())
{
    set_mm0(TEST_VAL);
    set_xmm0(TEST_VAL_128);

    xsave(xsave_area, xsave_mask);

    set_mm0(DESTROY_VAL);
    set_xmm0(DESTROY_VAL_128);

    xrstor(xsave_area, xsave_mask);

    BARETEST_ASSERT(get_mm0() == TEST_VAL);
    BARETEST_ASSERT(get_xmm0() == TEST_VAL_128);
}

TEST_CASE_CONDITIONAL(xsave_xrstor_full_avx, avx_supported())
{
    set_ymm0(TEST_VAL_256);

    xsave(xsave_area, xsave_mask);

    set_ymm0(DESTROY_VAL_256);

    xrstor(xsave_area, xsave_mask);

    BARETEST_ASSERT(get_ymm0() == TEST_VAL_256);
}

TEST_CASE_CONDITIONAL(xsave_xrstor_full_avx512, avx512_supported())
{
    set_k0(TEST_VAL);
    set_zmm0(TEST_VAL_512);
    set_zmm23(TEST_VAL_512);

    xsave(xsave_area, xsave_mask);

    set_k0(DESTROY_VAL);
    set_zmm0(DESTROY_VAL_512);
    set_zmm23(DESTROY_VAL_512);

    xrstor(xsave_area, xsave_mask);

    BARETEST_ASSERT(get_k0() == TEST_VAL);
    BARETEST_ASSERT(get_zmm0() == TEST_VAL_512);
    BARETEST_ASSERT(get_zmm23() == TEST_VAL_512);
}

TEST_CASE_CONDITIONAL(xsavec, xsavec_supported())
{
    set_mm0(TEST_VAL);

    xsavec(xsave_area, xsave_mask);

    set_mm0(DESTROY_VAL);

    xrstor(xsave_area, xsave_mask);

    BARETEST_ASSERT(get_mm0() == TEST_VAL);
}

TEST_CASE_CONDITIONAL(xsaveopt, xsaveopt_supported())
{
    set_mm0(TEST_VAL);

    xsaveopt(xsave_area, xsave_mask);

    set_mm0(DESTROY_VAL);

    xrstor(xsave_area, xsave_mask);

    BARETEST_ASSERT(get_mm0() == TEST_VAL);
}

TEST_CASE_CONDITIONAL(xsaves, xsaves_supported())
{
    set_mm0(TEST_VAL);

    xsaves(xsave_area, xsave_mask);

    set_mm0(DESTROY_VAL);

    xrstors(xsave_area, xsave_mask);

    BARETEST_ASSERT(get_mm0() == TEST_VAL);
}

// AMD does not allow to automatically inject a #UD for XSAVES
// in case the VMM does not want to expose this feature.
static bool is_virtualized_amd{ util::cpuid::is_amd_cpu() and util::cpuid::hv_bit_present() };
TEST_CASE_CONDITIONAL(xsaves_raises_ud, not xsaves_supported() and not is_virtualized_amd)
{
    irq_handler::guard irq_guard{ irq_handler_fn };
    irq_info.reset();

    if (setjmp(jump_buffer) == 0) {
        xsaves(xsave_area, XCR0_FPU);
    }

    BARETEST_ASSERT(irq_info.valid);
    BARETEST_ASSERT(irq_info.vec == to_underlying(exception::UD));
}

static bool fma_supported()
{
    return cpuid(CPUID_LEAF_FAMILY_FEATURES).ecx & LVL_0000_0001_ECX_FMA;
}

TEST_CASE_CONDITIONAL(fused_multiply_add, fma_supported() and xsave_supported())
{
    const uint64_t float_one{ 0x3f800000 };
    xmm_t a{}, b{}, c{ float_one };

    // Compute as double precision float: a := a*b + c
    asm("movdqu %[a], %%xmm0\n"
        "movdqu %[b], %%xmm1\n"
        "movdqu %[c], %%xmm2\n"
        "vfmadd132pd %%xmm1, %%xmm2, %%xmm0\n"
        "movdqu %%xmm0, %[a]\n"
        : [a] "+m"(a)
        : [b] "m"(b), [c] "m"(c));

    BARETEST_ASSERT(a[0] == float_one);
}

TEST_CASE_CONDITIONAL(cpuid_reflects_correct_osxsave_value, xsave_supported())
{
    set_cr4(get_cr4() & ~math::mask_from(cr4::OSXSAVE));
    BARETEST_ASSERT((cpuid(CPUID_LEAF_FAMILY_FEATURES).ecx & LVL_0000_0001_ECX_OSXSAVE) == 0);

    set_cr4(get_cr4() | math::mask_from(cr4::OSXSAVE));
    BARETEST_ASSERT((cpuid(CPUID_LEAF_FAMILY_FEATURES).ecx & LVL_0000_0001_ECX_OSXSAVE) != 0);
}
#endif
#if 0

#define set_ymm(num, values) asm volatile("vmovdqu %0, %%ymm" num ::"m"(values));

void set_val_for(unsigned num, ymm_t& val)
{
    val[0] &= ~0xFFull;
    val[1] &= ~0xFFull;
    val[2] &= ~0xFFull;
    val[3] &= ~0xFFull;

    val[0] |= num;
    val[1] |= num;
    val[2] |= num;
    val[3] |= num;
}

TEST_CASE_CONDITIONAL(fill_ymm_regs, avx_supported())
{
    ymm_t val {TEST_VAL_256};

    set_val_for(0, val);
    set_ymm("0", val);

    set_val_for(1, val);
    set_ymm("1", val);

    set_val_for(2, val);
    set_ymm("2", val);

    set_val_for(3, val);
    set_ymm("3", val);

    set_val_for(4, val);
    set_ymm("4", val);

    set_val_for(5, val);
    set_ymm("5", val);

    set_val_for(6, val);
    set_ymm("6", val);

    set_val_for(7, val);
    set_ymm("7", val);

    set_val_for(8, val);
    set_ymm("8", val);

    set_val_for(9, val);
    set_ymm("9", val);

    set_val_for(10, val);
    set_ymm("10", val);

    set_val_for(11, val);
    set_ymm("11", val);

    set_val_for(12, val);
    set_ymm("12", val);

    set_val_for(13, val);
    set_ymm("13", val);

    set_val_for(14, val);
    set_ymm("14", val);

    set_val_for(15, val);
    set_ymm("15", val);

    while (true) {}
}
#endif

#if 1

#define set_zmm(num, values) asm volatile("vmovdqu64 %0, %%zmm" num ::"m"(values));

void set_val_for(unsigned num, zmm_t& val)
{
    val[0] &= ~0xFFull;
    val[1] &= ~0xFFull;
    val[2] &= ~0xFFull;
    val[3] &= ~0xFFull;
    val[4] &= ~0xFFull;
    val[5] &= ~0xFFull;
    val[6] &= ~0xFFull;
    val[7] &= ~0xFFull;

    val[0] |= num;
    val[1] |= num;
    val[2] |= num;
    val[3] |= num;
    val[4] |= num;
    val[5] |= num;
    val[6] |= num;
    val[7] |= num;
}

TEST_CASE_CONDITIONAL(fill_zmm_regs, avx512_supported())
{
    zmm_t val {TEST_VAL_512};

    set_val_for(0, val);
    set_zmm("0", val);

    set_val_for(1, val);
    set_zmm("1", val);

    set_val_for(2, val);
    set_zmm("2", val);

    set_val_for(3, val);
    set_zmm("3", val);

    set_val_for(4, val);
    set_zmm("4", val);

    set_val_for(5, val);
    set_zmm("5", val);

    set_val_for(6, val);
    set_zmm("6", val);

    set_val_for(7, val);
    set_zmm("7", val);

    set_val_for(8, val);
    set_zmm("8", val);

    set_val_for(9, val);
    set_zmm("9", val);

    set_val_for(10, val);
    set_zmm("10", val);

    set_val_for(11, val);
    set_zmm("11", val);

    set_val_for(12, val);
    set_zmm("12", val);

    set_val_for(13, val);
    set_zmm("13", val);

    set_val_for(14, val);
    set_zmm("14", val);

    set_val_for(15, val);
    set_zmm("15", val);

    set_val_for(16, val);
    set_zmm("16", val);

    set_val_for(17, val);
    set_zmm("17", val);

    set_val_for(18, val);
    set_zmm("18", val);

    set_val_for(19, val);
    set_zmm("19", val);

    set_val_for(20, val);
    set_zmm("20", val);

    set_val_for(21, val);
    set_zmm("21", val);

    set_val_for(22, val);
    set_zmm("22", val);

    set_val_for(23, val);
    set_zmm("23", val);

    set_val_for(24, val);
    set_zmm("24", val);

    set_val_for(25, val);
    set_zmm("25", val);

    set_val_for(26, val);
    set_zmm("26", val);

    set_val_for(27, val);
    set_zmm("27", val);

    set_val_for(28, val);
    set_zmm("28", val);

    set_val_for(29, val);
    set_zmm("29", val);

    set_val_for(30, val);
    set_zmm("30", val);

    set_val_for(31, val);
    set_zmm("31", val);

    while (true) {}
}
#endif

BARETEST_RUN;

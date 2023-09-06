/* Copyright Cyberus Technology GmbH *
 *        All rights reserved        */

#include "printf/backend.hpp"
#include "segmentation.hpp"
#include "console-serial/console_serial.hpp"
#include "xhci/debug_device.hpp"

#include "heap.hpp"

#include "cbl/algorithm.hpp"
#include "arch.hpp"
#include "memory/buddy.hpp"
#include "cbl/interval.hpp"
#include "cbl/order_range.hpp"
#include <compiler.hpp>
#include <config.hpp>
#include <multiboot/multiboot.hpp>
#include <multiboot2/multiboot2.hpp>
#include "toyos/optionparser.hpp"
#include "pci/bus.hpp"
#include "memory/simple_buddy.hpp"
#include "toyos/xen-pvh.hpp"

#include "acpi.hpp"
#include "boot_cmdline.hpp"
#include "lapic_test_tools.hpp"
#include "pic.hpp"
#include "toyos/xhci_console.hpp"

#include "codecvt"
#include "locale"
#include "string"
#include "vector"

extern int main();

extern uint64_t gdt_start asm("gdt");
extern uint64_t gdt_tss;

first_fit_heap<HEAP_ALIGNMENT>* current_heap {nullptr};
simple_buddy* aligned_heap {nullptr};

static constexpr size_t DMA_POOL_SIZE {0x100000};
alignas(PAGE_SIZE) static char dma_pool_data[DMA_POOL_SIZE];
alignas(PAGE_SIZE) static x86::tss tss; // alignment only used to avoid avoid crossing page boundaries
static buddy dma_pool {32};

cbl::interval allocate_dma_mem(size_t ord)
{
    auto begin = dma_pool.alloc(ord);
    ASSERT(begin, "not enough DMA memory");

    return cbl::interval::from_order(*begin, ord);
}

EXTERN_C void init_heap()
{
    static constexpr unsigned HEAP_SIZE {1 /* MiB */ * 1024 * 1024};
    alignas(CPU_CACHE_LINE_SIZE) static char heap_data[HEAP_SIZE];
    static fixed_memory heap_mem(size_t(heap_data), HEAP_SIZE);
    static first_fit_heap<HEAP_ALIGNMENT> heap(heap_mem);
    current_heap = &heap;
    static simple_buddy align_buddy {31 + math::order_max(HEAP_ALIGNMENT)};
    aligned_heap = &align_buddy;
}

/**
 * \brief Set up a 64 bit TSS.
 *
 * After activating IA-32e mode, we have to create a 64 bit TSS (Intel SDM, Vol. 3, 7.7).
 */
EXTERN_C void init_tss()
{
    static const uint16_t TSS_GDT_INDEX {static_cast<uint16_t>(&gdt_tss - &gdt_start)};
    x86::segment_selector tss_selector {static_cast<uint16_t>(TSS_GDT_INDEX << x86::segment_selector::INDEX_SHIFT)};
    x86::gdt_entry* gdte {x86::get_gdt_entry(get_current_gdtr(), tss_selector)};

    gdte->set_system(true); // TSS descriptor is a system descriptor
    gdte->set_g(false);     // interpret limit as bytes
    gdte->set_base(ptr_to_num(&tss));
    gdte->set_limit(sizeof(x86::tss));
    gdte->set_present(true);
    gdte->set_type(x86::gdt_entry::segment_type::TSS_64BIT_AVAIL);

    asm volatile("ltr %0" ::"r"(tss_selector.value()));
}

namespace
{
enum option_index
{
    SERIAL,
    XHCI,
    XHCI_POWER
};

static constexpr const option::Descriptor usage[] = {
    // index, type, shorthand, name, checkarg, help
    {SERIAL, 0, "", "serial", option::Arg::Optional, "Enable serial console, with an optional port <port> in hex."},
    {XHCI, 0, "", "xhci", option::Arg::Optional, "Enable xHCI debug console, with optional custom serial number."},
    {XHCI_POWER, 0, "", "xhci_power", option::Arg::Optional,
     "Set the USB power cycle method (0=nothing, 1=powercycle)."},

    {0, 0, 0, 0, 0, 0}};
}; // namespace

static std::u16string get_xhci_identifier(const std::string& arg)
{
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
    return converter.from_bytes(arg == "" ? "CBS0001" : arg);
}

static std::string boot_cmdline;
std::string get_boot_cmdline()
{
    return boot_cmdline;
}

static void initialize_cmdline(const std::string& cmdline, acpi_mcfg* mcfg)
{
    boot_cmdline = cmdline;

    optionparser p(cmdline, usage);

    auto serial_option = p.option_value(option_index::SERIAL);
    auto xhci_option = p.option_value(option_index::XHCI);

    if (serial_option) {
        uint16_t port = *serial_option != "" ? std::stoull(*serial_option, nullptr, 16) : find_serial_port_in_bda();
        serial_init(port);
        add_printf_backend(console_serial::putchar);
    } else if (xhci_option) {
        // If we don't have an MCFG pointer here, there's not much we can do
        // to communicate. Let's just stop in a deterministic way.
        PANIC_UNLESS(mcfg, "No valid MCFG pointer given!");

        pci_bus pcibus(phy_addr_t(mcfg->base), mcfg->busses());

        auto is_xhci_fn = [](const pci_device& dev) { return dev.is_xhci(); };
        auto xhci_pci_dev = std::find_if(pcibus.begin(), pcibus.end(), is_xhci_fn);

        if (xhci_pci_dev != pcibus.end()) {
            const auto& pci_dev(*xhci_pci_dev);

            auto address = pci_dev.bar(0)->address();
            auto size = pci_dev.bar(0)->bar_size();

            auto mmio_region = cbl::interval::from_size(address, size);
            auto dma_region = pn2addr(allocate_dma_mem(math::order_envelope(XHCI_DMA_BUFFER_PAGES)));

            dummy_driver_adapter adapter;
            xhci_device xhci_dev(adapter, phy_addr_t(address));
            if (not xhci_dev.find_cap<xhci_debug_device::dbc_capability>()) {
                PANIC("No debug capability present!");
            }

            auto xhci_power_option = p.option_value(option_index::XHCI_POWER);
            auto power_method = xhci_power_option == "1" ? xhci_debug_device::power_cycle_method::POWERCYCLE
                                                         : xhci_debug_device::power_cycle_method::NONE;
            static xhci_console_baremetal xhci_cons(get_xhci_identifier(*xhci_option), mmio_region, dma_region,
                                                    power_method);
            xhci_console_init(xhci_cons);
        }
    } else {
        serial_init(find_serial_port_in_bda());
        add_printf_backend(console_serial::putchar);
    }

    // Some mainboards have a flaky serial, i.e. there is some noise on the line during bootup.
    // We work around this by simply sending a few newlines to have the actual data at the beginning of a line.
    printf("\n\n");
}

static void initialize_dma_pool()
{
    memset(dma_pool_data, 0, DMA_POOL_SIZE);

    auto dma_ival = cbl::interval::from_size(uintptr_t(&dma_pool_data), DMA_POOL_SIZE);
    buddy_reclaim_range(addr2pn(dma_ival), dma_pool);
}

EXTERN_C void init_interrupt_controllers()
{
    constexpr uint8_t PIC_BASE {32};
    pic pic {PIC_BASE}; // Initialize and mask PIC.

    ioapic ioapic {};

    // Mask all IRTs.
    for (uint8_t idx {0u}; idx < ioapic.max_irt(); idx++) {
        auto irt {ioapic.get_irt(idx)};
        if (not irt.masked()) {
            irt.mask();
            ioapic.set_irt(irt);
        }
    }

    lapic_test_tools::software_apic_disable();
}

EXTERN_C void entry64(uint32_t magic, uintptr_t boot_info)
{
    initialize_dma_pool();

    std::string cmdline;
    acpi_mcfg* mcfg {nullptr};

    if (magic == xen_pvh::MAGIC) {
        const auto* info = reinterpret_cast<xen_pvh::hvm_start_info*>(boot_info);
        cmdline = reinterpret_cast<const char*>(info->cmdline_paddr);

        const auto rsdp {reinterpret_cast<const acpi_rsdp*>(info->rsdp_paddr)};
        mcfg = find_mcfg(rsdp);
    } else if (magic == multiboot::multiboot_module::MAGIC_LDR) {
        cmdline = reinterpret_cast<multiboot::multiboot_info*>(boot_info)->get_cmdline().value_or("");

        // On legacy systems (where we use Multiboot1), the ACPI tables can be
        // found with the legacy way (see find_mcfg()).
        mcfg = find_mcfg();
    } else if (magic == multiboot2::MB2_MAGIC) {
        auto reader {multiboot2::mbi2_reader(reinterpret_cast<const uint8_t*>(boot_info))};
        const auto cmdline_tag {reader.find_tag(multiboot2::mbi2_cmdline::TYPE)};

        if (cmdline_tag) {
            const auto cmdline_full_tag {cmdline_tag->get_full_tag<multiboot2::mbi2_cmdline>()};

            const auto* cmdline_begin {cmdline_tag->addr + sizeof(cmdline_full_tag)};
            const auto* cmdline_end {cmdline_tag->addr + cmdline_tag->generic.size};

            PANIC_UNLESS(cmdline_end >= cmdline_begin, "Malformed cmdline tag");
            std::copy(cmdline_begin, cmdline_end, std::back_inserter(cmdline));
        }

        // With UEFI firmware, the ACPI tables are passed to the kernel via the
        // Multiboot2 information structure. In order to use that, we extract
        // the embedded RSDP table and pass the pointer on to the discovery
        // function.
        const auto acpi_tag {reader.find_tag(multiboot2::mbi2_rsdp2::TYPE)};

        if (acpi_tag) {
            const auto acpi_full_tag {acpi_tag->get_full_tag<multiboot2::mbi2_rsdp2>()};
            const auto rsdp {reinterpret_cast<const acpi_rsdp*>(acpi_tag->addr + sizeof(acpi_full_tag))};
            mcfg = find_mcfg(rsdp);
        }
    } else {
        __builtin_trap();
    }

    initialize_cmdline(cmdline, mcfg);

    main();
}

void* operator new(size_t size)
{
    ASSERT(current_heap, "heap not initialized");
    auto tmp {current_heap->alloc(size)};
    ASSERT(tmp, "out of memory");
    return tmp;
}

void* operator new(size_t size, std::align_val_t alignment) // C++17, nodiscard in C++2a
{
    // use default heap if possible
    ASSERT(current_heap, "heap not initialized");
    if (alignment <= static_cast<std::align_val_t>(current_heap->alignment())) {
        return current_heap->alloc(size);
    }

    ASSERT(aligned_heap, "aligned heap not initialized");

    if (math::order_max(static_cast<size_t>(alignment)) > aligned_heap->max_order) {
        PANIC("Requested alignment bigger than available alignment {} > {}", static_cast<size_t>(alignment),
              aligned_heap->max_order);
        alignment = static_cast<std::align_val_t>(aligned_heap->max_order);
    }

    auto tmp = aligned_heap->alloc(math::order_max(std::max(size, static_cast<size_t>(alignment))));
    ASSERT(tmp, "Failed to allocate aligned memory.");
    return reinterpret_cast<void*>(*tmp);
}

void* operator new(size_t size, const std::nothrow_t&) noexcept // nodiscard in C++2a
{
    return operator new(size);
}

void* operator new(size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept // C++17, nodiscard in C++2a
{
    return operator new(size, alignment);
}

void* operator new[](size_t size)
{
    return operator new(size);
}

void* operator new[](size_t size, std::align_val_t alignment) // C++17, nodiscard in C++2a
{
    return operator new(size, alignment);
}

void* operator new[](size_t size, const std::nothrow_t&) noexcept // nodiscard in C++2a
{
    return operator new(size);
}

void* operator new[](size_t size, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept // C++17, nodiscard in C++2a
{
    return operator new(size, alignment);
}

void operator delete(void* p) noexcept
{
    ASSERT(current_heap, "heap not initialized");
    current_heap->free(p);
}

void operator delete(void* p, std::align_val_t alignment) noexcept // C++17
{
    // use default heap if possible
    ASSERT(current_heap, "heap not initialized");
    if (alignment <= static_cast<std::align_val_t>(current_heap->alignment())) {
        return current_heap->free(p);
    }

    ASSERT(aligned_heap, "aligned heap not initialized");
    aligned_heap->free(reinterpret_cast<uintptr_t>(p));
}

void operator delete(void* p, size_t, std::align_val_t alignment) noexcept // C++17
{
    operator delete(p, alignment);
}

void operator delete(void* p, const std::nothrow_t&) noexcept
{
    operator delete(p);
}

void operator delete(void* p, std::align_val_t alignment, const std::nothrow_t&) noexcept // C++17
{
    operator delete(p, alignment);
}

void operator delete(void* p, size_t) noexcept
{
    operator delete(p);
}

void operator delete[](void* p) noexcept
{
    operator delete(p);
}

void operator delete[](void* p, size_t) noexcept
{
    operator delete[](p);
}

void operator delete[](void* p, std::align_val_t alignment) noexcept // C++17
{
    operator delete(p, alignment);
}

void operator delete[](void* p, size_t, std::align_val_t alignment) noexcept // C++17
{
    operator delete[](p, alignment);
}

void abort()
{
    PANIC("abort() called");
}

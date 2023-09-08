/* Copyright Cyberus Technology GmbH *
 *        All rights reserved        */

#pragma once

enum vmx_intercepts
{
   VMX_PT_EXCEPTION = 0,
   VMX_PT_INTR,
   VMX_PT_TRIPLE_FAULT,
   VMX_PT_INIT,
   VMX_PT_SIPI,
   VMX_PT_SMI_IO,
   VMX_PT_SMI_OTHER,
   VMX_PT_INT_WIN,
   VMX_PT_NMI_WIN,
   VMX_PT_TASK_SWITCH,
   VMX_PT_CPUID,
   VMX_PT_GETSEC,
   VMX_PT_HLT,
   VMX_PT_INVD,
   VMX_PT_INVLPG,
   VMX_PT_RDPMC,
   VMX_PT_RDTSC,
   VMX_PT_RSM,
   VMX_PT_VMCALL,
   VMX_PT_VMCLEAR,
   VMX_PT_VMLAUNCH,
   VMX_PT_VMPTRLD,
   VMX_PT_VMPTRST,
   VMX_PT_VMREAD,
   VMX_PT_VMRESUME,
   VMX_PT_VMWRITE,
   VMX_PT_VMXOFF,
   VMX_PT_VMXON,
   VMX_PT_CR_ACCESS,
   VMX_PT_DR_ACCESS,
   VMX_PT_IO_ACCESS,
   VMX_PT_RDMSR,
   VMX_PT_WRMSR,
   VMX_PT_INVALID_GUEST_STATE,
   VMX_PT_MSR_LOAD_FAIL,
   VMX_PT_RESERVED1,
   VMX_PT_MWAIT,
   VMX_PT_MTF,
   VMX_PT_RESERVED2,
   VMX_PT_MONITOR,
   VMX_PT_PAUSE,
   VMX_PT_MCA,
   VMX_PT_RESERVED3,
   VMX_PT_TPR_THRESHOLD,
   VMX_PT_APIC_ACCESS,
   VMX_PT_VEOI,
   VMX_PT_GDTR_IDTR,
   VMX_PT_LDTR_TR,
   VMX_PT_EPT_VIOLATION,
   VMX_PT_EPT_MISCONF,
   VMX_PT_INVEPT,
   VMX_PT_RDTSCP,
   VMX_PT_PREEMPTION_TIMER,
   VMX_PT_INVVPID,
   VMX_PT_WBINVD,
   VMX_PT_XSETBV,
   VMX_PT_APIC_WRITE,
   VMX_PT_RDRAND,
   VMX_PT_INVPCID,
   VMX_PT_VMFUNC,
   VMX_PT_RESERVED4,
   VMX_PT_RESERVED5,

   VMX_PT_VMENTRY_FAIL = 0xfd,
   VMX_PT_STARTUP = 0xfe,
   VMX_PT_POKED = 0xff,

   VMX_PT_RANGE = 0x100,
};

enum vmx_execution_controls : uint32_t
{
   VMX_CTRL0_INTR_WIN = 1u << 2,
   VMX_CTRL0_TSC_OFF = 1u << 3,
   VMX_CTRL0_HLT = 1u << 7,
   VMX_CTRL0_INVLPG = 1u << 9,
   VMX_CTRL0_MWAIT = 1u << 10,
   VMX_CTRL0_RDPMC = 1u << 11,
   VMX_CTRL0_RDTSC = 1u << 12,
   VMX_CTRL0_CR3_LOAD = 1u << 15,
   VMX_CTRL0_CR3_STORE = 1u << 16,
   VMX_CTRL0_CR8_LOAD = 1u << 19,
   VMX_CTRL0_CR8_STORE = 1u << 20,
   VMX_CTRL0_TPR_SHADOW = 1u << 21,
   VMX_CTRL0_NMI_WIN = 1u << 22,
   VMX_CTRL0_MOV_DR = 1u << 23,
   VMX_CTRL0_IO = 1u << 24,
   VMX_CTRL0_IO_BITMAP = 1u << 25,
   VMX_CTRL0_MTF = 1u << 27,
   VMX_CTRL0_MSR_BITMAP = 1u << 28,
   VMX_CTRL0_MONITOR = 1u << 29,
   VMX_CTRL0_PAUSE = 1u << 30,
   VMX_CTRL0_SECONDARY = 1u << 31,

   VMX_CTRL1_APIC_ACCESS = 1u << 0,
   VMX_CTRL1_EPT = 1u << 1,
   VMX_CTRL1_xDT = 1u << 2,
   VMX_CTRL1_ENABLE_RDTSCP = 1u << 3,
   VMX_CTRL1_X2APIC = 1u << 4,
   VMX_CTRL1_ENABLE_VPID = 1u << 5,
   VMX_CTRL1_WBINVD = 1u << 6,
   VMX_CTRL1_URG = 1u << 7,
   VMX_CTRL1_APIC_REGISTER = 1u << 8,
   VMX_CTRL1_VIRQ = 1u << 9,
   VMX_CTRL1_PAUSE_LOOP = 1u << 10,
   VMX_CTRL1_RDRAND = 1u << 11,
   VMX_CTRL1_ENABLE_INVPCID = 1u << 12,
   VMX_CTRL1_ENABLE_VMFUNC = 1u << 13,
   VMX_CTRL1_VMCS_SHADOW = 1u << 14,
   VMX_CTRL1_ENCLS = 1u << 15,
   VMX_CTRL1_RDSEED = 1u << 16,
   VMX_CTRL1_ENABLE_PML = 1u << 17,
   VMX_CTRL1_EPTV_VE = 1u << 18,
   VMX_CTRL1_VMX_PT = 1u << 19,
   VMX_CTRL1_ENABLE_XSAVES = 1u << 20,
   VMX_CTRL1_EPT_EXEC_MODE = 1u << 22,
   VMX_CTRL1_TSC_SCALING = 1u << 25,
};

namespace vmx
{
   void initialize_portals();
}  // namespace vmx
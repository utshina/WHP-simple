/******************************************************************************

  The MIT License (MIT)

  Copyright (c) 2020 Takahiro Shinagawa (The University of Tokyo)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

******************************************************************************/

/** ***************************************************************************
 * @file main.c
 * @brief A simple example of using Windows Hypervisor Platform (WHP)
 * @copyright Copyright (c) 2020 Takahiro Shinagawa (The University of Tokyo)
 * @license The MIT License (http://opensource.org/licenses/MIT)
 *************************************************************************** */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <windows.h>
#include <WinHvPlatform.h>
#ifdef _MSC_VER
#define aligned_alloc(alignment, size) _aligned_malloc(size, alignment)
#undef offsetof
#define offsetof(type, member) ((size_t)&((type*)0)->member)
#endif

#define OR >=0?(void)0:
#define panic(x) (void)(fputs("panic: " x "\n",stderr),exit(1))

#define KiB *1L*1024
#define MiB *1L*1024*1024
#define GiB *1L*1024*1024*1024

const UINT64 PTE_P  = 1ULL <<  0; // Present
const UINT64 PTE_RW = 1ULL <<  1; // Read/Write
const UINT64 PTE_US = 1ULL <<  2; // User/Supervisor
const UINT64 PTE_PS = 1ULL <<  7; // Page Size
const UINT64 PTE_G  = 1ULL <<  8; // Global

const UINT64 CR0_PE = 1ULL <<  0; // Protection Enable
const UINT64 CR0_PG = 1ULL << 31; // Paging Enable
const UINT64 CR4_PSE = 1ULL <<  4; // Page Size Extensions
const UINT64 CR4_PAE = 1ULL <<  5; // Physical Address Extension
const UINT64 EFER_LME = 1ULL <<  8; // IA-32e Mode Enable
const UINT64 EFER_LMA = 1ULL << 10; // IA-32e Mode Active

const UINT64 user_start = 4 KiB;
const UINT64 kernel_start = 1 GiB;
struct kernel {
	UINT64 pml4[512];
	UINT64 pdpt[512];
} *kernel;

const UINT8 user_code[] = {
	0x0f, 0x01, 0xc1, // vmcall
};


int
main(int argc, char *argv[])
{
	WHV_PARTITION_HANDLE handle;
	UINT16 vcpu = 0;

	// Is Windows Hypervisor Platform (WHP) enabled?
	UINT32 size;
	WHV_CAPABILITY capability;
	WHvGetCapability(
		WHvCapabilityCodeHypervisorPresent,
		&capability, sizeof(capability), &size);
	if (!capability.HypervisorPresent)
		panic("Windows Hypervisor Platform is not enabled");

	// create a VM
	WHvCreatePartition(&handle)
		OR panic("create partition");

	// set the VM properties
	UINT32 cpu_count = 1;
	WHvSetPartitionProperty(
		handle,
		WHvPartitionPropertyCodeProcessorCount,
		&cpu_count, sizeof(cpu_count))
		OR panic("set partition property (cpu count)");

	WHV_EXTENDED_VM_EXITS vmexits = { 0 };
	vmexits.HypercallExit = 1;
	WHvSetPartitionProperty(
		handle,
		WHvPartitionPropertyCodeExtendedVmExits,
		&vmexits, sizeof(vmexits))
		OR panic("set partition property (vmexits)");

	WHvSetupPartition(handle)
		OR panic("setup partition");

	// prepare and map kernel data structures
	assert((sizeof(*kernel) & (4 KiB - 1)) == 0); // 4 KiB align
	kernel = (struct kernel *)aligned_alloc(4 KiB, sizeof(*kernel));
	if (!kernel)
		panic("aligned_alloc");
	memset(kernel, 0, sizeof(*kernel));
	kernel->pml4[0] = (kernel_start + offsetof(struct kernel, pdpt))
		| (PTE_P | PTE_RW | PTE_US);
	kernel->pdpt[0] = 0x0
		| (PTE_P | PTE_RW | PTE_US | PTE_PS);
	WHvMapGpaRange(handle, kernel, kernel_start, sizeof(*kernel),
		       WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagWrite)
		OR panic("map the kernel region");

	// map user space
	void *user_page = aligned_alloc(4 KiB, 4 KiB);
	if (!user_page)
		panic("aligned_alloc");
	memcpy(user_page, user_code, sizeof(user_code));
	WHvMapGpaRange(handle, user_page, user_start, 4 KiB,
		       WHvMapGpaRangeFlagRead | WHvMapGpaRangeFlagExecute)
		OR panic("map the user region");

	// create a vCPU
	WHvCreateVirtualProcessor(handle, vcpu, 0)
		OR panic("create virtual processor");

	// setup vCPU registers
	enum {
		Cr0, Cr3, Cr4, Efer,
		Cs, Ss, Ds, Es, Rip,
		RegNum
	};
	WHV_REGISTER_NAME regname[RegNum];
	regname[Cr0] =  WHvX64RegisterCr0;
	regname[Cr3] =  WHvX64RegisterCr3;
	regname[Cr4] =  WHvX64RegisterCr4;
	regname[Efer] = WHvX64RegisterEfer;
	regname[Cs] =   WHvX64RegisterCs;
	regname[Ss] =   WHvX64RegisterSs;
	regname[Ds] =   WHvX64RegisterDs;
	regname[Es] =   WHvX64RegisterEs;
	regname[Rip] =  WHvX64RegisterRip;
	WHV_REGISTER_VALUE regvalue[RegNum];
	regvalue[Cr0].Reg64 = (CR0_PE | CR0_PG);
	regvalue[Cr3].Reg64 = kernel_start + offsetof(struct kernel, pml4);
	regvalue[Cr4].Reg64 = (CR4_PSE | CR4_PAE);
	regvalue[Efer].Reg64 = (EFER_LME | EFER_LMA);
	WHV_X64_SEGMENT_REGISTER CodeSegment;
	CodeSegment.Base = 0;
	CodeSegment.Limit = 0xffff;
	CodeSegment.Selector = 0x08;
	CodeSegment.Attributes = 0xa0fb;
	regvalue[Cs].Segment = CodeSegment;
	WHV_X64_SEGMENT_REGISTER DataSegment;
	DataSegment.Base = 0;
	DataSegment.Limit = 0xffff;
	DataSegment.Selector = 0x10;
	DataSegment.Attributes = 0xc0f3;
	regvalue[Ss].Segment = DataSegment;
	regvalue[Ds].Segment = DataSegment;
	regvalue[Es].Segment = DataSegment;
	regvalue[Rip].Reg64 = user_start;
	WHvSetVirtualProcessorRegisters(
		handle, vcpu, regname, RegNum, regvalue)
		OR panic("set virtual processor registers");

	// run the VM
	WHV_RUN_VP_EXIT_CONTEXT context;
	WHvRunVirtualProcessor(handle, vcpu, &context, sizeof(context))
		OR panic("run virtual processor");

	printf("Exit reason: %x\n", context.ExitReason);
	if (context.ExitReason == WHvRunVpExitReasonHypercall)
		puts("The vmcall instruction is executed");
	return 0;
}

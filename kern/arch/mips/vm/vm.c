/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 * The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
size_t frametablesize;
vaddr_t vkpagetable;
vaddr_t frametable;
void
vm_bootstrap(void)
{
	// allocate page table
	// frame table
	// kpage table
	paddr_t lomem, himem, pmem;
	size_t framecount;
	
	// kpage table, 1 page
	pmem = ram_stealmem(1);
	KASSERT(pmem != 0);
	vkpagetable = PADDR_TO_KVADDR(pmem);

	ram_getsize(&lomem, &himem);
	// page aligned?
	KASSERT(lomem & PAGE_FRAME == lomem);
	KASSERT(himem & PAGE_FRAME == himem);

	// Doesn't include kpage table!
	framecount = (himem - lomem) / PAGE_SIZE;
	
	// get frame table size, in frames.
	frametablesize = (framecount + 7)/8;
	frametablesize = (frametablesize + PAGE_SIZE - 1) / PAGE_SIZE;
	// ensure there are at least enough frames for one addrspace.
	KASSERT(frametablesize + 1 + 2 < framecount);
	

	// stealmem the frame table
	pmem = ram_stealmem(frametablesize);
	KASSERT(pmem != 0);
	frametable = PADDR_TO_KVADDR(pmem);
	// zero pages
	
}

vaddr_t
alloc_kpages(int npages)
{
	// find n **contiguous** pages
	// if failure:
	// 	return 0;
	// 	OR
	// 	compress
	// while more pages && not at end of frames
	// 	if frame is empty:
	// 		map page
	// if end of frames and more pages:
	// 	return 0;
	// else:
	// 	return page1
}

void 
free_kpages(vaddr_t addr)
{
	// while pages remain:
	// 	decref frame
	// 	unmap page
	// 	Invalidate TLB entry
	// 	if empty secondary page table:
	// 		dealloc secondary table
	//
}

void 
vm_tlbshootdown_all(void)
{
	// frob tlb
	panic("TLB Shootdown unsupported.\n");
}
void 
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void) ts;
	// inc TLBSHOOTDOWN count
	// if count >= TLBSHOOTDOWNMAX:
	// 	vm_tlbshootdown_all();
	// else:
	// 	tlb_probe
	// 	tlb_write(TLBHI_INVALID(entryno), TLBLO_INVALID())
	// 	unmap page?
	panic("TLB Shootdown unsupported.\n");
}

/*
 * Handler for three traps:
 * 	EX_MOD : Write to read only
 * 	EX_TLBL: TLB miss on load
 * 	EX_TLBS: TLB miss on store
 * This is where the page table is used.
 */
int 
vm_fault(int faulttype, vaddr_t faultaddress)
{
	// 	
	// if (curproc == NULL) {
	// 	// Kernel fault, early in boot.
	// 	return EFAULT;
	// }
	//
	// as = curproc_getas();
	//
	// if (as == NULL) {
	// 	// as above.
	// 	return EFAULT;
	// }
	//
	// switch(faulttype):
	// 	case readonly : 
	// 		if as not completed load, pretend.
	// 		else, EFAULT
	// 		break;
	// 	case read: 
	// 		if as not marked read, EFAULT
	// 		break;
	// 	case write:
	// 		if as not marked write, EFAULT
	// 		break;
	//
	// 	case default:
	// 		return EINVAL
	//
	// Ensure valid as
	// 
	// page table happens here.
	// Make it part of as?
	// segment seg = NULL;
	// for segment in as->segments:
	// 	if (faultaddress >= segement.vbase && faultaddress < segment.vtop) {
	// 		paddr = faultaddress - segment.vbase + segment.as_pbase;
	// 		seg = segment
	// 	}
	// if (paddr == 0) {
	// 	return EFAULT
	// }
	//
	// Ensure page aligned
	//
	// spl = splhigh();
	//
	// for i in range(NUM_TLB):
	// 	tlb_read(&ehi, &elo, i)
	// 	if (!elo & TLBLO_VALID)
	// 		continue;
	// 	ehi = faultaddress
	// 	elo = 0;
	// 	if (segment.flags & PF_W):
	// 		elo |= TLBLO_DIRTY;
	// 	elo |= paddr | TLBLO_VALID;
	// 	tlb_write(ehi, elo, i);
	// 	splx(spl);
	// 	return 0;
	// 
	// ehi = faultaddress;
	// elo = 0;
	// if (segment.flags & PF_W) :
	// 	elo |= TLBLO_DIRTY;
	// elo |= paddr | TLBLO_VALID;
	// tlb_random(ehi, elo);
	// splx(spl);
	// return 0;
}


struct addrspace *
as_create(void)
{}
int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	// *deep* copy?
}
void
as_destroy(struct addrspace *as)
{}

void
as_activate(void)
{
	// if as == null (kernel)
	// 	return
	//
	// frob tlb
}
void
as_deactivate(void)
{
	// idk
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
       int readable, int writeable, int executable)
{
	// get a vm function to mark pages.
	// align region
	// append vaddr and npages onto as *array*
}

int
as_prepare_load(struct addrspace *as)
{
	// get required pages for memory and stack
}
int
as_complete_load(struct addrspace *as)
{
	// frob tlb?
	// ldcomplete = true
}
int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	// define as USERSTACK?
}

/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
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
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 *
 * UNSW: If you use ASST3 config as required, then this file forms
 * part of the VM subsystem.
 *
 */
//dev
// #define VM_READ 0x4  // 对应 MIPS 的 TLB_VALID
// #define VM_WRITE 0x2  // 对应 MIPS 的 TLB_DIRTY
// #define VM_EXEC 0x4  // MIPS 没有独立的执行权限，使用 VM_READ 来模拟

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}
    
	/*
	 * Initialize as needed.
	 */

    as->region_start = NULL;
    as->heap_start = 0;
    as->heap_end = 0;
    as->stack_top = 0;
	
    as->pagetable = pagetable_create_l1();
    if(as->pagetable == NULL){
        kfree(as);
        return NULL;
    }
    
    for (uint16_t i = 0; i < L1_PAGETABLE_NUM; ++i) {
        as->pagetable[i] = NULL;
    }

    as->as_loaded = false;  // 标记地址空间尚未加载任何内容 dev

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret) //!TODO
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */

	if (!old->as_loaded) {
        // 如果源地址空间未加载，则无法复制
        as_destroy(newas);
        return EFAULT;
    }

	// 遍历源地址空间的页表
    for (uint16_t i = 0; i < L1_PAGETABLE_NUM; i++) {
        if (old->pagetable[i] != NULL) {
            // 为目标页表分配内存
            newas->pagetable[i] = kmalloc(sizeof(paddr_t) * L2_PAGETABLE_NUM);
            if (newas->pagetable[i] == NULL) {
                // 分配失败，清理已分配的内存并返回错误
                as_destroy(newas);
                return ENOMEM;
            }
            
            // 复制每个页
            for (uint16_t j = 0; j < L2_PAGETABLE_NUM; j++) {
                if (old->pagetable[i][j] != 0) {
                    // 为目标页框分配内存
                    paddr_t new_frame = alloc_kpages(1);
                    if (new_frame == 0) {
                        // 分配失败，清理已分配的内存并返回错误
                        as_destroy(newas);
                        return ENOMEM;
                    }

					paddr_t new_paddr = KVADDR_TO_PADDR(new_frame);
                    
                    // 复制页内容
                    memmove((void *)PADDR_TO_KVADDR(new_paddr),
                            (const void *)PADDR_TO_KVADDR(old->pagetable[i][j]),
                            PAGE_SIZE);
                    
                    // 更新新地址空间的页表条目
                    newas->pagetable[i][j] = new_paddr;
                }
            }
        }
    }

	*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	if (as == NULL) {
        return;
    }

    pagetable_destroy(as->pagetable);

	region_ptr *current = as->region_start;
    while (current != NULL) {
        region_ptr *temp = current;
        current = current->next;
        kfree(temp);
    }

	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable) //!TODO
{
	/*
	 * Write this.
	 */

	if (as == NULL) return EINVAL;

    struct region *new_region = kmalloc(sizeof(struct region));
    if (new_region == NULL) return ENOMEM;

    new_region->base = vaddr;
    new_region->size = memsize;
    new_region->permission = 0;

    if (readable) SET_FLAG(new_region->permission, FLAG_READ);
    if (writeable) SET_FLAG(new_region->permission, FLAG_WRITE);
    if (executable) SET_FLAG(new_region->permission, FLAG_EXECUTE);

    new_region->next = as->region_start;
    as->region_start = new_region;

    return 0;

	// (void)as;
	// (void)vaddr;
	// (void)memsize;
	// (void)readable;
	// (void)writeable;
	// (void)executable;
	// return ENOSYS; /* Unimplemented */
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	if (as == NULL) {
        return EFAULT;
    }
    region_ptr *current = as->region_start;
    while (current != NULL) {
        current->permission = SET_FLAG(current->permission, FLAG_WRITE);
        current = current->next;
    }

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
    if (as == NULL){
        return EFAULT;
    }
    region_ptr *current = as->region_start;
    while (current != NULL) {
        uint32_t permission = current->permission;
        if(OLD_PERMISSION(permission) != CURR_PERMISSION(permission)){
            current->permission = REGION_PERMISSION(permission);
        }
        current = current->next;
    }
	int spl = splhigh();
    for(uint16_t i = 0; i<NUM_TLB; ++i){
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    splx(spl);
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}


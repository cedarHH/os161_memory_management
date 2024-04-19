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

#ifndef _VM_H_
#define _VM_H_

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */


#include <machine/vm.h>

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

#define L1_PAGE_MASK 0xFFE00000 /* mask for getting level 1 page number from addr */
#define L2_PAGE_MASK   0x1FF000 /* mask for getting level 2 page number from addr */

#define L1_PAGETABLE_NUM 2048 
#define L2_PAGETABLE_NUM 512

//The upper 20 bits of the page table entry are the physical page number
//The remaining 12 bits are unused
#define FLAG_READ         0x001 /* mask for getting readable bit from page table entry */ 
#define FLAG_WRITE        0x002 /* mask for getting writable bit from page table entry */ 
#define FLAG_EXECUTE      0x004 /* mask for getting executable bit from page table entry */
#define FLAG_INMEMORY     0x008 /* mask for getting in-memory bit from page table entry */  //!unused
#define FLAG_DIRTY        0x010 /* mask for getting dirty bit from page table entry */      //!unused
#define FLAG_VALID        0x020 /* mask for getting valid bit from page table entry */      //!unused
#define FLAG_USED         0x040 /* mask for getting used bit from page table entry */       

#define PAGE_NUM(paddr) (paddr & PAGE_FRAME)        /* getting page number from addr */
#define L1_PAGE_NUM(vaddr) (((vaddr) & L1_PAGE_MASK) >> 21)  /* getting level 1 page number from addr */
#define L2_PAGE_NUM(vaddr) (((vaddr) & L2_PAGE_MASK) >> 12)  /* getting level 2 page number from addr */

#define ZERO_FILLED_PAGE(vaddr_base) bzero(vaddr_base, PAGE_SIZE)

#define SET_FLAG(entry, flag) ((entry) |= (flag))       /* set flag */
#define CLEAR_FLAG(entry, flag) ((entry) &= ~(flag))    /* clear flag */ 
#define IS_FLAG_SET(entry, flag) ((entry) & (flag))     /* check flag */


typedef uint32_t page_table_entry, *l2_page_tabel, **l1_page_table;

l1_page_table pagetable_create_l1(void);

uint32_t pagetable_create_l2(l1_page_table pagetable, uint16_t l1_ptable_num);

uint32_t pagetable_insert(l1_page_table pagetable, uint16_t l1_ptable_num, uint16_t l2_page_num, uint32_t dirty_bit);

uint32_t pagetable_copy(l1_page_table src_ptable, l1_page_table dest_ptable);

void pagetable_destroy(l1_page_table pagetable);

/* Initialization function */
void vm_bootstrap(void);

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress);

/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown *);


#endif /* _VM_H_ */

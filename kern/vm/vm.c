#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>

/* Place your page table functions here */
l1_page_table 
pagetable_create_l1()
{
    //2048*4B = 8K>PAGE_SIZE
    l1_page_table pagetable = kmalloc(L1_PAGETABLE_NUM * sizeof(l2_page_tabel));
    return pagetable;
}

uint32_t 
pagetable_create_l2(l1_page_table pagetable, uint16_t l1_ptable_num)
{
    //512*4B = 2K<PAGE_SIZE
    l2_page_tabel sub_pagetable = kmalloc(L2_PAGETABLE_NUM * sizeof(paddr_t));
    if(sub_pagetable == NULL){
        return ENOMEM;
    }
    pagetable[l1_ptable_num] = sub_pagetable;
    for(uint16_t i = 0; i < L2_PAGETABLE_NUM; ++i){
        pagetable[l1_ptable_num] = 0;
    }
    return 0;
}

uint32_t 
pagetable_insert(l1_page_table pagetable, uint16_t l1_ptable_num, uint16_t l2_page_num)
{
    vaddr_t vaddr_base = alloc_kpages(1);
    if(vaddr_base = 0){
        return ENOMEM;
    }
    ZERO_FILLED_PAGE(vaddr_base);
    paddr_t paddr_base = KVADDR_TO_PADDR(vaddr_base);
    pagetable[l1_ptable_num][l2_page_num] = PHYSICAL_PAGE_NUM(paddr_base);
    SET_FLAG(pagetable[l1_ptable_num][l2_page_num], TLBLO_VALID); //!TODO Other flag
    return 0;
}

uint32_t 
pagetable_copy(l1_page_table src_ptable, l1_page_table dest_ptable)
{
    for(uint16_t i = 0; i < L1_PAGETABLE_NUM; ++i)
    {
        if(src_ptable[i] != NULL){
            if(pagetable_create_l2(dest_ptable, i)) return ENOMEM; 
            for(uint16_t j = 0; j < L2_PAGETABLE_NUM; ++j){
                if(src_ptable[i][j] != 0){
                    vaddr_t vaddr_base = alloc_kpages(1);
                    if(vaddr_base == 0) return ENOMEM;
                    ZERO_FILLED_PAGE(vaddr_base);
                    if(!memmove(vaddr_base, PADDR_TO_KVADDR(PHYSICAL_PAGE_NUM(src_ptable[i][j])),PAGE_SIZE)){
                        pagetable_destroy(dest_ptable);
                        return ENOMEM;
                    }
                    dest_ptable[i][j] = PHYSICAL_PAGE_NUM(KVADDR_TO_PADDR(vaddr_base));
                    SET_FLAG(dest_ptable[i][j], TLBLO_VALID); //!TODO Other flag
                }
            }
        }
    }
    return 0;
}

void 
pagetable_destroy(l1_page_table pagetable)
{
    if (pagetable) {
        for (uint16_t i = 0; i < L1_PAGETABLE_NUM; ++i) {
            if (pagetable[i] != NULL) {
                for(uint16_t j = 0; j < L2_PAGETABLE_NUM; ++j){
                    if(IS_FLAG_SET(pagetable[i][j],FLAG_USED)){
                        free_kpages(PADDR_TO_KVADDR(PHYSICAL_PAGE_NUM(pagetable[i][j])));
                    }
                }
                kfree(pagetable[i]);
            }
        }
        kfree(pagetable);
    }
}

void vm_bootstrap(void)
{
    /* Initialise any global components of your VM sub-system here.  
     *  
     * You may or may not need to add anything here depending what's
     * provided or required by the assignment spec.
     */
}

int
vm_fault(int faulttype, vaddr_t faultaddress)//!TODO
{
    struct addrspace *as;
    region_ptr rg;
    paddr_t ppage_base;

    if(!faultaddress) return EFAULT;
    faultaddress = PHYSICAL_PAGE_NUM(faultaddress);
    switch (faulttype)
    {
    case VM_FAULT_READ:
        break;
    case VM_FAULT_WRITE:
        break;
    case VM_FAULT_READONLY:
        return EFAULT;
    default:
        return EINVAL;
    }
    as = proc_getas();
    panic("vm_fault hasn't been written yet\n");

    return EFAULT;
}

/*
 * SMP-specific functions.  Unused in our UNSW configuration.
 */

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("vm tried to do tlb shootdown?!\n");
}


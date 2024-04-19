#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/tlb.h>
#include <proc.h>
#include <spl.h>

/* Place your page table functions here */
l1_page_table 
pagetable_create_l1(void)
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
        pagetable[l1_ptable_num][i] = 0;
    }
    return 0;
}

uint32_t 
pagetable_insert(l1_page_table pagetable, uint16_t l1_ptable_num, uint16_t l2_page_num, uint32_t dirty_bit)
{
    vaddr_t vaddr_base = alloc_kpages(1);
    if(vaddr_base == 0){
        return ENOMEM;
    }
    ZERO_FILLED_PAGE((void *)vaddr_base);
    paddr_t paddr_base = KVADDR_TO_PADDR(vaddr_base);
    pagetable[l1_ptable_num][l2_page_num] = PAGE_NUM(paddr_base);
    SET_FLAG(pagetable[l1_ptable_num][l2_page_num], TLBLO_VALID);
    SET_FLAG(pagetable[l1_ptable_num][l2_page_num], dirty_bit); 
    SET_FLAG(pagetable[l1_ptable_num][l2_page_num], FLAG_USED); //!TODO Other flag
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
                    ZERO_FILLED_PAGE((void *)vaddr_base);
                    if(!memmove((void *)vaddr_base, (const void *)PADDR_TO_KVADDR(PAGE_NUM(src_ptable[i][j])),PAGE_SIZE)){
                        pagetable_destroy(dest_ptable);
                        return ENOMEM;
                    }
                    uint32_t dirty_bit = IS_FLAG_SET(src_ptable[i][j], TLBLO_DIRTY);
                    dest_ptable[i][j] = PAGE_NUM(KVADDR_TO_PADDR(vaddr_base));
                    SET_FLAG(dest_ptable[i][j], TLBLO_VALID); 
                    SET_FLAG(dest_ptable[i][j], dirty_bit);
                    SET_FLAG(dest_ptable[i][j], FLAG_USED); //!TODO Other flag
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
                        free_kpages(PADDR_TO_KVADDR(PAGE_NUM(pagetable[i][j])));
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
vm_fault(int faulttype, vaddr_t faultaddress)
{
    struct addrspace *as;
    region_ptr curRegion;
    paddr_t ppage_base = 0;
    uint32_t l1_index, l2_index;
    uint32_t entry_hi, entry_lo;
    uint32_t dirty_bit = 0;
    int spl;

    as = proc_getas();

    if((faulttype == VM_FAULT_READONLY)| !faultaddress | !as){
        return EFAULT;
    }    
    
    faultaddress = PAGE_NUM(faultaddress);
    curRegion = as->region_start;
    
    while(curRegion){
        if(faultaddress >= curRegion->base && 
            faultaddress < curRegion->base + curRegion->size
        ){
            break;
        }
        curRegion = curRegion->next;
    }
    if(!curRegion) {
        return EFAULT;
    }
    ppage_base = KVADDR_TO_PADDR(faultaddress);
    l1_index = L1_PAGE_NUM(ppage_base);
    l2_index = L2_PAGE_NUM(ppage_base);
    
    if(!as->pagetable[l1_index]){
        if(pagetable_create_l2(as->pagetable, l1_index)){
            return ENOMEM;
        }
    }

    if(!as->pagetable[l1_index][l2_index]){
        region_ptr curr = as->region_start;
        while(curr){
            if(faultaddress >= curr->base &&
                faultaddress < curr->base + curr->size
            ){
                if(IS_FLAG_SET(curr->permission, FLAG_WRITE)){
                    dirty_bit = TLBLO_DIRTY;
                }
                break;
            }
            curr = curr->next;
        }
        if(!curr){
            return EFAULT;
        }
        if(pagetable_insert(as->pagetable,l1_index,l2_index,dirty_bit)){
            return ENOMEM;
        }
    }

    entry_hi = PAGE_NUM(faultaddress);
    entry_lo = as->pagetable[l1_index][l2_index];

    spl = splhigh();
    tlb_random(entry_hi, entry_lo);
    splx(spl);
    return 0;
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


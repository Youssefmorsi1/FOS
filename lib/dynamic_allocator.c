/*
 * dynamic_allocator.c
 *
 *  Created on: Sep 21, 2023
 *      Author: HP
 */
#include <inc/assert.h>
#include <inc/string.h>
#include "../inc/dynamic_allocator.h"


//==================================================================================//
//============================== GIVEN FUNCTIONS ===================================//
//==================================================================================//

//=====================================================
// 1) GET BLOCK SIZE (including size of its meta data):
//=====================================================
uint32 get_block_size(void* va)
{
	struct BlockMetaData *curBlkMetaData = ((struct BlockMetaData *)va - 1) ;
	return curBlkMetaData->size ;
}

//===========================
// 2) GET BLOCK STATUS:
//===========================
int8 is_free_block(void* va)
{
	struct BlockMetaData *curBlkMetaData = ((struct BlockMetaData *)va - 1) ;
	return curBlkMetaData->is_free ;
}

//===========================================
// 3) ALLOCATE BLOCK BASED ON GIVEN STRATEGY:
//===========================================
void *alloc_block(uint32 size, int ALLOC_STRATEGY)
{
	void *va = NULL;
	switch (ALLOC_STRATEGY)
	{
	case DA_FF:
		va = alloc_block_FF(size);
		break;
	case DA_NF:
		va = alloc_block_NF(size);
		break;
	case DA_BF:
		va = alloc_block_BF(size);
		break;
	case DA_WF:
		va = alloc_block_WF(size);
		break;
	default:
		cprintf("Invalid allocation strategy\n");
		break;
	}
	return va;
}

//===========================
// 4) PRINT BLOCKS LIST:
//===========================

void print_blocks_list(struct MemBlock_LIST list)
{
	cprintf("=========================================\n");
	struct BlockMetaData* blk ;
	cprintf("\nDynAlloc Blocks List:\n");
	LIST_FOREACH(blk, &list)
	{
		cprintf("(size: %d, isFree: %d)\n", blk->size, blk->is_free) ;
	}
	cprintf("=========================================\n");

}
//
////********************************************************************************//
////********************************************************************************//

//==================================================================================//
//============================ REQUIRED FUNCTIONS ==================================//
//==================================================================================//
bool is_initialized = 0;
//==================================
// [1] INITIALIZE DYNAMIC ALLOCATOR:
//==================================
void initialize_dynamic_allocator(uint32 daStart, uint32 initSizeOfAllocatedSpace)
{
    if (initSizeOfAllocatedSpace == 0)
        return ;
    is_initialized = 1;
  //struct MemBlock_LIST blocks;
    LIST_INIT(&blocks);
    struct BlockMetaData *tempBlock = (struct BlockMetaData*) daStart;
    tempBlock->size = initSizeOfAllocatedSpace;
    tempBlock->is_free = 1;
    LIST_INSERT_HEAD(&blocks, tempBlock);
}

//=========================================
// [4] ALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *alloc_block_FF(uint32 size)
{
    if(size == 0)
        return NULL;

    if (!is_initialized)
    {
    uint32 required_size = size + sizeOfMetaData();
    uint32 da_start = (uint32)sbrk(required_size);
    //get new break since it's page aligned! thus, the size can be more than the required one
    uint32 da_break = (uint32)sbrk(0);
    initialize_dynamic_allocator(da_start, da_break - da_start);
    }

    struct BlockMetaData *ptr;
    //iterate over blocks to find free block
    for(ptr = LIST_FIRST(&blocks); ptr != 0; ptr = LIST_NEXT(ptr)){
        if(ptr->is_free && ptr->size >= size + sizeOfMetaData()){
            //allocate to current block
            ptr->is_free = 0;
            //check for residue
            if(ptr->size - size - sizeOfMetaData() > sizeOfMetaData()){
                struct BlockMetaData *tempBlock = (struct BlockMetaData *) ((uint32)ptr + size + sizeOfMetaData());
                tempBlock->is_free = 1;
                tempBlock->size = ptr->size - size - sizeOfMetaData();
                ptr->size -= tempBlock->size;
                LIST_INSERT_AFTER(&blocks, ptr, tempBlock);
            }

            ptr = (struct BlockMetaData *) ((uint32) ptr + sizeOfMetaData());
            return ptr;
        }
    }


    //if not found then raise brk
    uint32 oldBrk = (uint32) sbrk(size + sizeOfMetaData());
    //could not raise brk
    if(oldBrk == -1)
       return NULL;

    ptr = (struct BlockMetaData *) oldBrk;
    ptr->is_free = 0;
    ptr->size = (uint32) sbrk(0) - (uint32) oldBrk;
    LIST_INSERT_TAIL(&blocks, ptr);

    //check for residue
    if(ptr->size - size - sizeOfMetaData() > sizeOfMetaData()){
        struct BlockMetaData *tempBlock = (struct BlockMetaData *) ((uint32)ptr + size + sizeOfMetaData());
        tempBlock->is_free = 1;
        tempBlock->size = ptr->size - size - sizeOfMetaData();
        ptr->size -= tempBlock->size;
        LIST_INSERT_AFTER(&blocks, ptr, tempBlock);
    }

    ptr = (struct BlockMetaData *) ((uint32) ptr + sizeOfMetaData());
    return ptr;
}

//=========================================
// [5] ALLOCATE BLOCK BY BEST FIT:
//=========================================
void *alloc_block_BF(uint32 size)
{
    if(size == 0)
    {
    	return NULL;
    }

    struct BlockMetaData *ptr;
    struct BlockMetaData *head = blocks.lh_first;
    uint32 min_size_diff;
    struct BlockMetaData *best_fit_ptr = head;
    bool flag = 0;
    bool place_found = 0;
    bool true_flag = 1;
    for(ptr = LIST_FIRST(&blocks); ptr != 0; ptr = LIST_NEXT(ptr))
    {
    	if(ptr->is_free)
    	{
        if(ptr->size -sizeOfMetaData() == size){
        	ptr->is_free=0;
        	    	return(void *)(uint32) ptr+sizeOfMetaData();
        }
        else if(( (!flag) ||(min_size_diff  + size + sizeOfMetaData()> ptr->size) )&& (ptr->size >=  size + sizeOfMetaData()) )
    	{

    		min_size_diff=ptr->size - size - sizeOfMetaData();
    		best_fit_ptr=(struct BlockMetaData *)(uint32)ptr;
    		best_fit_ptr->size=ptr->size;
    		flag=1;
    		place_found = 1;
    	}
    	}
    }

	if(place_found==1)
			{
             bool c1=sizeOfMetaData()<min_size_diff;
             bool c2=sizeOfMetaData()>min_size_diff;
						if(c1)
						{
								struct BlockMetaData* newBlock = (struct BlockMetaData *)((uint32)(best_fit_ptr) + sizeOfMetaData() + size);
								newBlock->is_free = 1;
								newBlock->size = min_size_diff;
								LIST_INSERT_AFTER(&blocks, best_fit_ptr, newBlock);
						}
						else if(c2)
						{
							//remaining size is less than metaData size -> don't create a new block, just make the original one not free and return ptr
							best_fit_ptr->is_free = 0;
							struct BlockMetaData *newBlock = (struct BlockMetaData *)((uint32)best_fit_ptr + sizeOfMetaData());
							return newBlock;
						}
			}
			if(place_found==1 && true_flag ==1)
			{

				best_fit_ptr->size = size + sizeOfMetaData();
				best_fit_ptr->is_free = 0;
				struct BlockMetaData *newBlock = (struct BlockMetaData *)((uint32)best_fit_ptr + sizeOfMetaData());
				return newBlock;
			}

     else{
//if not found then raise brk
int * oldBrk =  sbrk(size + sizeOfMetaData());
//could not raise brk
if(oldBrk == (void*)-1)
    return NULL;

best_fit_ptr = (struct BlockMetaData *) oldBrk;
best_fit_ptr->is_free = 0;
best_fit_ptr->size = size + sizeOfMetaData();
LIST_INSERT_TAIL(&blocks, best_fit_ptr);
return best_fit_ptr;
      }
}

//=========================================
// [6] ALLOCATE BLOCK BY WORST FIT:
//=========================================
void *alloc_block_WF(uint32 size)
{
	panic("alloc_block_WF is not implemented yet");
	return NULL;
}

//=========================================
// [7] ALLOCATE BLOCK BY NEXT FIT:
//=========================================
void *alloc_block_NF(uint32 size)
{
	panic("alloc_block_NF is not implemented yet");
	return NULL;
}

//===================================================
// [8] FREE BLOCK WITH COALESCING:
//===================================================
void free_block(void *va)
{
  //first case: Va equals NULL
	if(va==NULL)
		return ;


	struct BlockMetaData *ptr =va-sizeOfMetaData();
	struct BlockMetaData *prev = LIST_PREV(ptr);
	struct BlockMetaData *next = LIST_NEXT(ptr);
	struct BlockMetaData *head = blocks.lh_first;
	struct BlockMetaData *tail = blocks.lh_last;

	//Case 1: no merge
	if((ptr == tail || next->is_free == 0) && (ptr == head || prev->is_free == 0)){
		ptr->is_free = 1;

		return;
	}

	//Case 2: merge with next and prev
	if((ptr != tail && next->is_free == 1) && (ptr != head && prev->is_free == 1)){
		prev->size += ptr->size + next->size;
		ptr->is_free = 0;
		ptr->size = 0;
		next->is_free = 0;
		next->size = 0;

		LIST_REMOVE(&blocks, ptr);
		LIST_REMOVE(&blocks, next);
		return;
	}

	//Case 3: merge with next only;
	if((ptr != tail && next->is_free == 1)){
		ptr->size += next->size;
		ptr->is_free = 1;
		next->size = 0;
		next->is_free = 0;

		LIST_REMOVE(&blocks, next);
		return;
	}

	//Case 4: merge with prev only
	if((ptr != head && prev->is_free == 1)){
		prev->size += ptr->size;
		ptr->is_free = 0;
		ptr->size = 0;

		LIST_REMOVE(&blocks, ptr);
		return;
	}
}



//=========================================
// [4] REALLOCATE BLOCK BY FIRST FIT:
//=========================================
void *realloc_block_FF(void *va, uint32 new_size)
{
    struct BlockMetaData *blkptr = (struct BlockMetaData *)((uint32) va - sizeOfMetaData());

    if (va && new_size == 0){
    	free_block(va);
        return NULL ;
    }

    if (va == NULL && new_size )
    {
    	return alloc_block_FF(new_size);
    }

    if (va == NULL && new_size == 0 )
    {
    	return NULL;

    }

    //if already size is set
    if (new_size + sizeOfMetaData() ==  blkptr->size )
    {
    	return va;
    }

    // reallocate at the same place with smaller size
    if(new_size + sizeOfMetaData() < blkptr->size){
    	uint32 remSz = blkptr->size - new_size - sizeOfMetaData();
    	struct BlockMetaData *nxt = LIST_NEXT(blkptr);

    	//merge with upper and clear upper
    	if(nxt && nxt->is_free){
    		remSz += nxt->size;
    		nxt->size = 0;
    		nxt->is_free = 0;
    		LIST_REMOVE(&blocks, nxt);
    	}

    	//add residue block
    	if(remSz > sizeOfMetaData()){
        	struct BlockMetaData *tempBlock;
        	uint32 tempAd = (uint32) blkptr + sizeOfMetaData() + new_size;
        	tempBlock = (struct BlockMetaData *) tempAd;
        	tempBlock->size = remSz;
        	tempBlock->is_free = 1;
        	blkptr->size -= remSz;
        	LIST_INSERT_AFTER(&blocks, blkptr, tempBlock);
    	}

    	return (void *) ((uint32)blkptr + sizeOfMetaData());
    }
    //new size > old size
    void * address  = NULL ;

    uint32 f_new_size = new_size + sizeOfMetaData() ;

    if(f_new_size > blkptr->size){
    	struct BlockMetaData * nxt = LIST_NEXT(blkptr) ;
    	struct BlockMetaData * prv = LIST_PREV(blkptr) ;

    	uint32 with_nxt = blkptr->size + nxt->size  ;
    	uint32 with_prv = blkptr->size + prv->size  ;
    	// try merge with the next block
    	if(nxt != NULL && nxt->is_free  == 1 && nxt->size > f_new_size ){
    		nxt->size = 0;
    		nxt->is_free = 0;
    		LIST_REMOVE(&blocks, nxt);

    		if(with_nxt - f_new_size > sizeOfMetaData()){
    			struct BlockMetaData *tempBlock;
    			uint32 tempAd = (uint32) blkptr +  f_new_size ;
    			tempBlock = (struct BlockMetaData *) tempAd;
    			tempBlock->size = with_nxt - f_new_size ;
    			tempBlock->is_free = 1;
    			blkptr->size = f_new_size ;
    			LIST_INSERT_AFTER(&blocks, blkptr, tempBlock);
    		}else {
    			blkptr->size = with_nxt;
    		}

        	address = (void *) ((uint32)blkptr + sizeOfMetaData());
    	}else {
    		address = (void *)alloc_block_FF(new_size) ;

    		if(address){
    			if(nxt && nxt->is_free){
    	    		nxt->size = 0;
    	    		nxt->is_free = 0;
    				LIST_REMOVE(&blocks, nxt);
    				blkptr->size = with_nxt ;
    			}
    			if(prv && prv->is_free){
    				prv->size = blkptr->size + prv->size ;
    	    		blkptr->size = 0;
    	    		blkptr->is_free = 0;
    				LIST_REMOVE(&blocks, blkptr);
    			}
    		}

    	}
    	return address ;
    }

    //redundant
    return NULL;
}

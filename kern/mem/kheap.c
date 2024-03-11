#include "kheap.h"
#include <inc/memlayout.h>
#include <inc/dynamic_allocator.h>
#include "memory_manager.h"

//TODO: [PROJECT'23.MS2 - #01] [1] KERNEL HEAP - initialize_kheap_dynamic_allocator()
int initialize_kheap_dynamic_allocator(uint32 daStart, uint32 initSizeToAllocate, uint32 daLimit)
{
	start=daStart;
	initSizeToAllocate = ROUNDUP(initSizeToAllocate, PAGE_SIZE);
	segment_break =start+initSizeToAllocate;
    hard_limit=daLimit;

   if(segment_break > hard_limit  )
   {
	   return E_NO_MEM;
   }
   else
   {

	//All pages in the given range should be allocated
    for(uint32 i=start ; i<segment_break ; i+=PAGE_SIZE)
    {
     uint32 *ptr_page_table=NULL;
     int ret = get_page_table(ptr_page_directory,i,&ptr_page_table);
     struct FrameInfo * ptr_frame_info = get_frame_info(ptr_page_directory,i,&ptr_page_table);
     int c = allocate_frame(&ptr_frame_info);

     ptr_frame_info->va = i;
     c = map_frame(ptr_page_directory,ptr_frame_info,i,PERM_USER|PERM_WRITEABLE);
    }

    initialize_dynamic_allocator(daStart,initSizeToAllocate);

    return 0;
   }

	return 0;
}

//TODO: [PROJECT'23.MS2 - #02] [1] KERNEL HEAP - sbrk()
void* sbrk(int increment)
{
	if(increment == 0)return (void *)segment_break ;
	else if(increment > 0){
		uint32 va = segment_break;
		uint32 newSbrk = segment_break + increment;
		if(newSbrk >= hard_limit){
			panic("exeeded hard limit") ;
		}

		segment_break = ROUNDDOWN(segment_break, PAGE_SIZE);
		while(segment_break <= newSbrk){
			uint32 *ptrPT = NULL ;//ptr_page_table
			struct FrameInfo * ptr_frame_info = get_frame_info(ptr_page_directory, segment_break, &ptrPT);
			if(ptr_frame_info != 0){
				segment_break += PAGE_SIZE;
				continue;
			}
			int ret = allocate_frame(&ptr_frame_info) ;
			if(ret == E_NO_MEM){
				panic("no memory from allocate_frame") ;
			}

			ptr_frame_info->va = segment_break;
			ret = map_frame(ptr_page_directory , ptr_frame_info , segment_break , PERM_WRITEABLE) ;
			if(ret == E_NO_MEM){
				free_frame(ptr_frame_info);
				panic("no memory from map_frame") ;
			}

			segment_break += PAGE_SIZE;
		}

		segment_break = ROUNDUP(newSbrk, PAGE_SIZE);
		return (void *)va ;
	}else {
		uint32 n;
		uint32 tmp = ROUNDDOWN(segment_break, PAGE_SIZE) ;
		segment_break += increment ;
		n = (ROUNDDOWN(tmp, PAGE_SIZE) - ROUNDDOWN(segment_break, PAGE_SIZE)) / PAGE_SIZE;
		if(n < 0)
			n = 0;

		while(n--){
			uint32 * ptr_page_table ;
			struct FrameInfo * ptr_frame_info  = get_frame_info(ptr_page_directory , tmp ,&ptr_page_table ) ;

			unmap_frame(ptr_page_directory , tmp);
			tmp-=PAGE_SIZE ;
	    }
		return (void *)segment_break ;
	}
}

void* kmalloc(unsigned int size)
{
	//TODO: [PROJECT'23.MS2 - #03] [1] KERNEL HEAP - kmalloc()
	//refer to the project presentation and documentation for details
	// Still to do : use "isKHeapPlacementStrategyFIRSTFIT() ..." functions to check the current strategy

	//Case 1: Block allocator
	if(size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		return alloc_block_FF(size);
	}

	//Case 2: Page allocator
	uint32 pageCnt = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	uint32 i, curCnt = 0, startVA = -1, *ptrPT = NULL;
	for(i = hard_limit + PAGE_SIZE; i < KERNEL_HEAP_MAX; i += PAGE_SIZE){
		if(get_frame_info(ptr_page_directory , i , &ptrPT) == 0)
			curCnt++;
		else
			curCnt = 0;
		if(curCnt == 1)
			startVA = i;

		if(curCnt == pageCnt)
			break;
	}

	//Failed to allocate
	if(curCnt != pageCnt)
		return NULL;

	//found pages -> allocate frames
	i = startVA;
	int pageIndex = (i - hard_limit - PAGE_SIZE) / PAGE_SIZE;
	chunkSZ[pageIndex] = 0;
	while(pageCnt--){
		struct FrameInfo *frame = NULL;
		int ret = allocate_frame(&frame);
		if(ret == E_NO_MEM){
			//free previously allocated pages
			kfree((void *)startVA);
			return NULL;
		}

		chunkSZ[pageIndex] += PAGE_SIZE;
		frame->va = i;
		ret = map_frame(ptr_page_directory, frame, i, PERM_WRITEABLE);
		if(ret == E_NO_MEM){
			//free previously allocated pages
			kfree((void *)startVA);
			return NULL;
		}
		i += PAGE_SIZE;
	}

	return (void *) (startVA);
}
void kfree(void* virtual_address)
{
	//TODO: [PROJECT'23.MS2 - #04] [1] KERNEL HEAP - kfree()
	//refer to the project presentation and documentation for details
	// Write your code here, remove the panic and write your code

	//Case 1: Invalid address
	bool invalid = !((uint32) virtual_address >= KERNEL_HEAP_START && (uint32)virtual_address < hard_limit);
	invalid &= !((uint32) virtual_address >= hard_limit + PAGE_SIZE && (uint32)virtual_address < KERNEL_HEAP_MAX);
	if(invalid){
		panic("Invalid Virtual Address");
		return;
	}


	//Case 2: Block allocator
	if((uint32) virtual_address >= KERNEL_HEAP_START && (uint32)virtual_address < hard_limit){
		free_block(virtual_address);
		return;
	}

	//Case 3: Page allocator
	uint32 curVA = (uint32) virtual_address;
	int pageIndex = (curVA - hard_limit - PAGE_SIZE) / PAGE_SIZE;
	int size = chunkSZ[pageIndex];
	chunkSZ[pageIndex] = 0;
	int pageCnt = ROUNDUP(size, PAGE_SIZE) / PAGE_SIZE;
	while(pageCnt--){
		uint32 *ptrPT = NULL;
		struct FrameInfo *frame = get_frame_info(ptr_page_directory, curVA, &ptrPT);
		unmap_frame(ptr_page_directory, curVA);

		curVA += PAGE_SIZE;
	}
}
unsigned int kheap_physical_address(unsigned int virtual_address)
{
    uint32 *ptr_page_table=NULL;
    struct FrameInfo * ptr_frame_info = get_frame_info(ptr_page_directory,virtual_address,&ptr_page_table);
    if(ptr_frame_info == NULL)
    	return 0;

    return to_physical_address(ptr_frame_info) + ((virtual_address<< 20) >> 20);
}

unsigned int kheap_virtual_address(unsigned int physical_address)
{
	struct FrameInfo * ptr_frame_info =NULL;
	ptr_frame_info = to_frame_info(physical_address);
	if(ptr_frame_info == NULL || ptr_frame_info->va == 0)
		return 0;

	return ptr_frame_info->va + ((physical_address<<20) >> 20);

}


void kfreeall()
{
	panic("Not implemented!");

}

void kshrink(uint32 newSize)
{
	panic("Not implemented!");
}

void kexpand(uint32 newSize)
{
	panic("Not implemented!");
}




//=================================================================================//
//============================== BONUS FUNCTION ===================================//
//=================================================================================//
// krealloc():

//	Attempts to resize the allocated space at "virtual_address" to "new_size" bytes,
//	possibly moving it in the heap.
//	If successful, returns the new virtual_address, in which case the old virtual_address must no longer be accessed.
//	On failure, returns a null pointer, and the old virtual_address remains valid.

//	A call with virtual_address = null is equivalent to kmalloc().
//	A call with new_size = zero is equivalent to kfree().
void *krealloc(void *virtual_address, uint32 new_size)
{
	//TODO: [PROJECT'23.MS2 - BONUS#1] [1] KERNEL HEAP - krealloc()
	// Write your code here, remove the panic and write your code
	return NULL;

	if(virtual_address == NULL)
		return kmalloc(new_size);

	if(new_size == 0){
		kfree(virtual_address);
		return NULL;
	}

	//reallocation in block allocator region
	bool blockAllocated = (uint32) virtual_address >= KERNEL_HEAP_START && (uint32)virtual_address < hard_limit;
	if(blockAllocated && new_size <= DYN_ALLOC_MAX_BLOCK_SIZE){
		return realloc_block_FF(virtual_address, new_size);
	}

	//reallocation from block allocator to page allocator and vice versa
	if(blockAllocated != (new_size <= DYN_ALLOC_MAX_BLOCK_SIZE)){
		kfree(virtual_address);
		return kmalloc(new_size);
	}

	//reallocation in page allocator region

	int pageIndex = ((int) virtual_address - hard_limit - PAGE_SIZE) / PAGE_SIZE;
	new_size = ROUNDUP(new_size, PAGE_SIZE);
	if(chunkSZ[pageIndex] == new_size){
		return virtual_address;
	}

	if(chunkSZ[pageIndex] > new_size){
		//need to unmap extra pages
		uint32 curVA = (uint32) virtual_address + chunkSZ[pageIndex] - PAGE_SIZE;
		int extraPages = (chunkSZ[pageIndex] - new_size) / PAGE_SIZE;
		while(extraPages--){
			uint32 *ptrPT = NULL;
			struct FrameInfo *frame = get_frame_info(ptr_page_directory, curVA, &ptrPT);
			unmap_frame(ptr_page_directory, curVA);

			curVA -= PAGE_SIZE;
		}

		chunkSZ[pageIndex] = new_size;
		return virtual_address;
	}

	//new size > prev size
	//count free pages next
	int nxtFree = 0;
	uint32 curVA = (uint32) virtual_address + chunkSZ[pageIndex], *ptrPT = NULL;
	while(curVA < KERNEL_HEAP_MAX){
		if(get_frame_info(ptr_page_directory, curVA , &ptrPT) != 0)
			break;

		nxtFree++;
		curVA += PAGE_SIZE;
	}

	int extraPages = (new_size - chunkSZ[pageIndex]) / PAGE_SIZE;
	//if enough next free pages
	if(nxtFree >= extraPages){
		curVA = (uint32) virtual_address + chunkSZ[pageIndex];
		int startVA = curVA;
		int startIndex = (startVA - hard_limit - PAGE_SIZE) / PAGE_SIZE;
		chunkSZ[startIndex] = 0;
		while(extraPages--){
			struct FrameInfo *frame = NULL;
			int ret = allocate_frame(&frame);
			if(ret == E_NO_MEM){
				//free previously allocated pages
				kfree((void *)startVA);
				return NULL;
			}

			chunkSZ[startIndex] += PAGE_SIZE;
			frame->va = curVA;
			ret = map_frame(ptr_page_directory, frame, curVA, PERM_WRITEABLE);
			if(ret == E_NO_MEM){
				//free previously allocated pages
				kfree((void *)startVA);
				return NULL;
			}
			curVA += PAGE_SIZE;
		}

		//success
		chunkSZ[pageIndex] += chunkSZ[startIndex];
		chunkSZ[startIndex] = 0;
		return virtual_address;
	}

	//need to reallocate elsewhere
	void *ret = kmalloc(new_size);
	if(ret != NULL)
		kfree(virtual_address);

	return ret;
}

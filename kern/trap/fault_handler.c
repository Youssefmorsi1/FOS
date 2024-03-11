/*
 * fault_handler.c
 *
 *  Created on: Oct 12, 2022
 *      Author: HP
 */

#include "trap.h"
#include <kern/proc/user_environment.h>
#include "../cpu/sched.h"
#include "../disk/pagefile_manager.h"
#include "../mem/memory_manager.h"

//2014 Test Free(): Set it to bypass the PAGE FAULT on an instruction with this length and continue executing the next one
// 0 means don't bypass the PAGE FAULT
uint8 bypassInstrLength = 0;

//===============================
// REPLACEMENT STRATEGIES
//===============================
//2020
void setPageReplacmentAlgorithmLRU(int LRU_TYPE)
{
	assert(LRU_TYPE == PG_REP_LRU_TIME_APPROX || LRU_TYPE == PG_REP_LRU_LISTS_APPROX);
	_PageRepAlgoType = LRU_TYPE ;
}
void setPageReplacmentAlgorithmCLOCK(){_PageRepAlgoType = PG_REP_CLOCK;}
void setPageReplacmentAlgorithmFIFO(){_PageRepAlgoType = PG_REP_FIFO;}
void setPageReplacmentAlgorithmModifiedCLOCK(){_PageRepAlgoType = PG_REP_MODIFIEDCLOCK;}
/*2018*/ void setPageReplacmentAlgorithmDynamicLocal(){_PageRepAlgoType = PG_REP_DYNAMIC_LOCAL;}
/*2021*/ void setPageReplacmentAlgorithmNchanceCLOCK(int PageWSMaxSweeps){_PageRepAlgoType = PG_REP_NchanceCLOCK;  page_WS_max_sweeps = PageWSMaxSweeps;}

//2020
uint32 isPageReplacmentAlgorithmLRU(int LRU_TYPE){return _PageRepAlgoType == LRU_TYPE ? 1 : 0;}
uint32 isPageReplacmentAlgorithmCLOCK(){if(_PageRepAlgoType == PG_REP_CLOCK) return 1; return 0;}
uint32 isPageReplacmentAlgorithmFIFO(){if(_PageRepAlgoType == PG_REP_FIFO) return 1; return 0;}
uint32 isPageReplacmentAlgorithmModifiedCLOCK(){if(_PageRepAlgoType == PG_REP_MODIFIEDCLOCK) return 1; return 0;}
/*2018*/ uint32 isPageReplacmentAlgorithmDynamicLocal(){if(_PageRepAlgoType == PG_REP_DYNAMIC_LOCAL) return 1; return 0;}
/*2021*/ uint32 isPageReplacmentAlgorithmNchanceCLOCK(){if(_PageRepAlgoType == PG_REP_NchanceCLOCK) return 1; return 0;}

//===============================
// PAGE BUFFERING
//===============================
void enableModifiedBuffer(uint32 enableIt){_EnableModifiedBuffer = enableIt;}
uint8 isModifiedBufferEnabled(){  return _EnableModifiedBuffer ; }

void enableBuffering(uint32 enableIt){_EnableBuffering = enableIt;}
uint8 isBufferingEnabled(){  return _EnableBuffering ; }

void setModifiedBufferLength(uint32 length) { _ModifiedBufferLength = length;}
uint32 getModifiedBufferLength() { return _ModifiedBufferLength;}

//===============================
// FAULT HANDLERS
//===============================


//Handle the table fault
void table_fault_handler(struct Env * curenv, uint32 fault_va)
{
	//panic("table_fault_handler() is not implemented yet...!!");
	//Check if it's a stack page
	uint32* ptr_table;
#if USE_KHEAP
	{
		ptr_table = create_page_table(curenv->env_page_directory, (uint32)fault_va);
	}
#else
	{
		__static_cpt(curenv->env_page_directory, (uint32)fault_va, &ptr_table);
	}
#endif
}
//Handle the page fault
void page_fault_handler(struct Env * curenv, uint32 fault_va)
{
#if USE_KHEAP
		struct WorkingSetElement *victimWSElement = NULL;
		uint32 wsSize = LIST_SIZE(&(curenv->page_WS_list));
#else
		int iWS =curenv->page_last_WS_index;
		uint32 wsSize = env_page_ws_get_size(curenv);
#endif
	fault_va = ROUNDDOWN(fault_va, PAGE_SIZE);
	if(wsSize < (curenv->page_WS_max_size) && isPageReplacmentAlgorithmFIFO())
	{
		//TODO: [PROJECT'23.MS2 - #15] [3] PAGE FAULT HANDLER - Placement
		uint32 *ptrPT = NULL ;//ptr_page_table
		struct FrameInfo * ptr_frame_info = get_frame_info(curenv->env_page_directory , fault_va , &ptrPT) ;
		int all=allocate_frame(&ptr_frame_info);
		all = map_frame(curenv->env_page_directory , ptr_frame_info , fault_va , PERM_USER | PERM_WRITEABLE|PERM_PRESENT) ;
		pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT | PERM_MARKED | PERM_USER | PERM_WRITEABLE ,0);
		int ret = pf_read_env_page(curenv, (void *)fault_va) ;
		bool mapframe=0;
		if(ret== E_PAGE_NOT_EXIST_IN_PF)
		{
			if(!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va < USTACKTOP && fault_va >= USTACKBOTTOM)))
			{

				sched_kill_env(curenv->env_id);
			}
			else
			{
				mapframe=1;
			}

		}
		else
		{

			mapframe=1;
		}



		struct WorkingSetElement *WSE =NULL;
		WSE= env_page_ws_list_create_element(curenv , fault_va) ;
        int L_size=curenv->page_WS_list.size + 1;
        int L_Max_Size=curenv->page_WS_max_size;
		if((L_size )== (L_Max_Size))
		{
			LIST_INSERT_TAIL(&(curenv->page_WS_list),WSE);
			curenv->page_last_WS_element= LIST_FIRST(&(curenv->page_WS_list));
		}
		else
		{
			LIST_INSERT_TAIL(&(curenv->page_WS_list),WSE);
			curenv->page_last_WS_element=NULL;
		}
		//refer to the project presentation and documentation for details
	}
	else
	{
		//cprintf("REPLACEMENT=========================WS Size = %d\n", wsSize );
		//refer to the project presentation and documentation for details
		if(isPageReplacmentAlgorithmFIFO())
		{
			//TODO: [PROJECT'23.MS3 - #1] [1] PAGE FAULT HANDLER - FIFO Replacement

			struct WorkingSetElement*ptr_to_remove=curenv->page_last_WS_element;
			uint32 *ptrPT = NULL ;//ptr_page_table
			struct FrameInfo * ptr_all_frame_info = get_frame_info(curenv->env_page_directory , fault_va , &ptrPT) ;
			int all=allocate_frame(&ptr_all_frame_info);
			all = map_frame(curenv->env_page_directory , ptr_all_frame_info , fault_va , PERM_USER | PERM_WRITEABLE|PERM_PRESENT | PERM_MARKED) ;

			pt_set_page_permissions(curenv->env_page_directory,ptr_to_remove->virtual_address,0,PERM_PRESENT | PERM_MARKED);
			pt_set_page_permissions(curenv->env_page_directory,fault_va,PERM_PRESENT | PERM_MARKED | PERM_USER | PERM_WRITEABLE ,0);

			int ret = pf_read_env_page(curenv, (void *)fault_va) ;
			if(ret== E_PAGE_NOT_EXIST_IN_PF)
			{
				if(!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va < USTACKTOP && fault_va >= USTACKBOTTOM)))
					sched_kill_env(curenv->env_id);
			}

			struct WorkingSetElement *WSE =NULL;
			WSE= env_page_ws_list_create_element(curenv , fault_va) ;

			if(curenv->page_last_WS_element==LIST_LAST(&(curenv->page_WS_list))){
				curenv->page_last_WS_element=curenv->page_WS_list.lh_first;
				LIST_INSERT_TAIL(&(curenv->page_WS_list),WSE);
			}
			else
			{
				curenv->page_last_WS_element=curenv->page_last_WS_element->prev_next_info.le_next;
				LIST_INSERT_BEFORE(&(curenv->page_WS_list),curenv->page_last_WS_element,WSE);
			}
			int perms = pt_get_page_permissions(curenv->env_page_directory,ptr_to_remove->virtual_address);
			uint32 *ptr_table = NULL;
			struct FrameInfo * ptr_frame_info = get_frame_info(curenv->env_page_directory, ptr_to_remove->virtual_address, &ptr_table);
			if((perms & PERM_MODIFIED) == PERM_MODIFIED)
				pf_update_env_page(curenv,ptr_to_remove->virtual_address,ptr_frame_info);

			unmap_frame(curenv->env_page_directory,ptr_to_remove->virtual_address);
			env_page_ws_invalidate(curenv,ptr_to_remove->virtual_address);

		}

		if(isPageReplacmentAlgorithmLRU(PG_REP_LRU_LISTS_APPROX))
		{
			//TODO: [PROJECT'23.MS3 - #2] [1] PAGE FAULT HANDLER - LRU Replacement
			//TODO: [PROJECT'23.MS3 - BONUS] [1] PAGE FAULT HANDLER - O(1) implementation of LRU replacement
			int activeSZ = LIST_SIZE(&curenv->ActiveList);
			int secondSZ = LIST_SIZE(&curenv->SecondList);
			//check if fault va is in second list
			uint32 *ptr_table = NULL;
			struct FrameInfo * ptr_frame_info = get_frame_info(curenv->env_page_directory, fault_va, &ptr_table);

			if((int)ptr_frame_info != 0){
				//check if active is full
				if(activeSZ >= curenv->ActiveListSize){
					struct WorkingSetElement *tail = LIST_LAST(&curenv->ActiveList);
					LIST_REMOVE(&curenv->ActiveList, tail);
					LIST_INSERT_HEAD(&curenv->SecondList, tail);
					pt_set_page_permissions(curenv->env_page_directory, tail->virtual_address, 0, PERM_PRESENT);
				}
				struct WorkingSetElement *faulted = (struct WorkingSetElement *) env_page_ws_get_address(fault_va);
				LIST_REMOVE(&curenv->SecondList, faulted);
				LIST_INSERT_HEAD(&curenv->ActiveList, faulted);
				pt_set_page_permissions(curenv->env_page_directory, fault_va, PERM_PRESENT, 0);

				return;
			}

			//Page not in working set
			if(activeSZ + secondSZ < curenv->page_WS_max_size){
				//Placement
				//check if active list is full
				if(activeSZ >= curenv->ActiveListSize){
					struct WorkingSetElement *tail = LIST_LAST(&curenv->ActiveList);
					LIST_REMOVE(&curenv->ActiveList, tail);
					LIST_INSERT_HEAD(&curenv->SecondList, tail);
					pt_set_page_permissions(curenv->env_page_directory, tail->virtual_address, 0, PERM_PRESENT);
				}

				//allocate, map and add requested page
				uint32 *ptrPT = NULL ;//ptr_page_table
				struct FrameInfo * ptr_frame_info = get_frame_info(curenv->env_page_directory , fault_va , &ptrPT) ;
				int all = allocate_frame(&ptr_frame_info);
				if(all == E_NO_MEM)
				{
					panic("no memory from allocate_frame") ;
				}

				all = map_frame(curenv->env_page_directory , ptr_frame_info , fault_va , PERM_USER | PERM_WRITEABLE|PERM_PRESENT) ;
				if(all == E_NO_MEM)
				{
					free_frame(ptr_frame_info) ;
					panic("no memory from map_frame") ;
				}
				int ret = pf_read_env_page(curenv, (void *)fault_va);
				if(ret == E_PAGE_NOT_EXIST_IN_PF){
					if(!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va < USTACKTOP && fault_va >= USTACKBOTTOM)))
						sched_kill_env(curenv->env_id);
				}

				struct WorkingSetElement *newPage = env_page_ws_list_create_element(curenv, fault_va);
				LIST_INSERT_HEAD(&curenv->ActiveList, newPage);
			}else{
				//replacement
				//allocate, map and add requested page
				uint32 *ptrPT = NULL ;//ptr_page_table
				struct FrameInfo * ptr_frame_info = get_frame_info(curenv->env_page_directory , fault_va , &ptrPT) ;
				int all = allocate_frame(&ptr_frame_info);
				if(all == E_NO_MEM)
				{
					panic("no memory from allocate_frame") ;
				}
				all = map_frame(curenv->env_page_directory , ptr_frame_info , fault_va , PERM_USER | PERM_WRITEABLE|PERM_PRESENT) ;
				if(all == E_NO_MEM)
				{
					free_frame(ptr_frame_info) ;
					panic("no memory from map_frame") ;
				}

				int ret = pf_read_env_page(curenv, (void *)fault_va);
				if(ret == E_PAGE_NOT_EXIST_IN_PF){
					if(!((fault_va >= USER_HEAP_START && fault_va < USER_HEAP_MAX) || (fault_va < USTACKTOP && fault_va >= USTACKBOTTOM)))
					{
						sched_kill_env(curenv->env_id);
					}
				}

				//remove from active
				struct WorkingSetElement *tail = LIST_LAST(&curenv->ActiveList);
				LIST_REMOVE(&curenv->ActiveList, tail);
				LIST_INSERT_HEAD(&curenv->SecondList, tail);
				pt_set_page_permissions(curenv->env_page_directory, tail->virtual_address, 0, PERM_PRESENT);

				//remove from second
				tail = LIST_LAST(&curenv->SecondList);
				pt_set_page_permissions(curenv->env_page_directory, tail->virtual_address, 0, PERM_MARKED);
				env_page_ws_O1_invalidate(curenv, (uint32)tail);

				struct WorkingSetElement *newPage = env_page_ws_list_create_element(curenv, fault_va);
				LIST_INSERT_HEAD(&curenv->ActiveList, newPage);
			}
		}


	}
}

void __page_fault_handler_with_buffering(struct Env * curenv, uint32 fault_va)
{
	panic("this function is not required...!!");
}




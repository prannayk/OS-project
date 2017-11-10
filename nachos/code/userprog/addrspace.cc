// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

void ProcessAddressSpace::updateUse(int ppn){
	InverseEntry * node = &(PageTable[ppn]);
	if (repl == LRU){
		if (lrulist == NULL)	lrulist = new LRUList (node);
		else {
			if (!lrulist->exists(node))
				lrulist->Insert(node);
			else{
				lrulist->MoveToFront(node);
			}
		}
	} else if (repl == LRU_CLOCK){
		if(!node->ref)	node->ref++;
	}
}

void ProcessAddressSpace::removePage(int vpn){
	if(!KernelPageTable[vpn].valid) ASSERT(FALSE);
	KernelPageTable[vpn].valid = FALSE;
	if (lrulist != NULL)	if (lrulist->exists(&(PageTable[KernelPageTable[vpn].physicalPage])))	 lrulist->Delete(&(PageTable[KernelPageTable[vpn].physicalPage]));
	if ((!KernelPageTable[vpn].shared) && KernelPageTable[vpn].dirty){
		KernelPageTable[vpn].swapped = TRUE;
		for(int copy = 0;copy<PageSize;copy++){
			swapspace[vpn*PageSize + copy] = machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize + copy];
		}
		bzero(&(machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize]), PageSize);
	}
}

void 
ProcessAddressSpace::loadPage(int vpn){
	static int count = 0;
	KernelPageTable[vpn].physicalPage = GetPhysicalPage(currentThread->GetPID());
	KernelPageTable[vpn].valid = TRUE;
	PageTable[KernelPageTable[vpn].physicalPage].entry = &(KernelPageTable[vpn]);
	if (KernelPageTable[vpn].swapped){
		for(int copy = 0; copy < PageSize; copy++)
			machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize + copy] = swapspace[vpn*PageSize + copy];
		return;
	}
    bzero(&machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize], PageSize);
	int virtualStart = vpn*PageSize;
	int virtualEnd = virtualStart + PageSize;
	if (((virtualStart <= codeStart) && (codeStart <= virtualEnd)) || ((virtualStart <= codeEnd) && (virtualEnd >= codeEnd)) || ((codeStart <= virtualStart) && (codeEnd >= virtualEnd))){
		unsigned cstart = max(codeStart, virtualStart);
		unsigned cend = min(virtualEnd -1, codeEnd);
		unsigned offset = cstart % PageSize;
		unsigned csize = cend - cstart + 1;
		NoffHeader noffH; OpenFile* exec;
		exec = fileSystem->Open(currentThread->GetFilename());
		exec->ReadAt((char *)&noffH, sizeof(noffH), 0);
		unsigned filepos = noffH.code.inFileAddr + (cstart - codeStart);
		exec->ReadAt(&(machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize + offset]),PageSize, filepos );
	}
	if (((virtualStart <= dataStart) && (dataStart <= virtualEnd)) || ((virtualStart <= dataEnd) && (virtualEnd >= dataEnd)) || ((dataStart <= virtualStart) && (dataEnd >= virtualEnd))){
		unsigned cstart = max(dataStart, virtualStart);
		unsigned cend = min(virtualEnd -1, dataEnd);
		unsigned offset = cstart % PageSize;
		unsigned csize = cend - cstart + 1;
		NoffHeader noffH; OpenFile* exec;
		exec = fileSystem->Open(currentThread->GetFilename());
		exec->ReadAt((char *)&noffH, sizeof(noffH), 0);
		unsigned filepos = noffH.initData.inFileAddr + (cstart - dataStart);
		exec->ReadAt(&(machine->mainMemory[KernelPageTable[vpn].physicalPage*PageSize + offset]),csize, filepos );
	}
}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;
    unsigned vpn, offset;
    TranslationEntry *entry;
    unsigned int pageFrame;
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numVirtualPages = divRoundUp(size, PageSize);
	swapspace = new char [PageSize*numVirtualPages];
    size = numVirtualPages * PageSize;


    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numVirtualPages, size);
    KernelPageTable = new TranslationEntry[numVirtualPages];
    for (i = 0; i < numVirtualPages; i++) {
	KernelPageTable[i].virtualPage = i;
	KernelPageTable[i].physicalPage = -1;
	KernelPageTable[i].valid = FALSE;
	KernelPageTable[i].use = FALSE;
	KernelPageTable[i].dirty = FALSE;
	KernelPageTable[i].readOnly = FALSE;  
	KernelPageTable[i].shared = FALSE;
	KernelPageTable[i].swapped = FALSE;
	}

	codeStart = noffH.code.virtualAddr;
	codeEnd = noffH.code.size + codeStart;
	dataStart = noffH.initData.virtualAddr;
	dataEnd = noffH.initData.size + dataStart;

}

//----------------------------------------------------------------------
// ProcessAddressSpace::ProcessAddressSpace (ProcessAddressSpace*) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------

ProcessAddressSpace::ProcessAddressSpace(ProcessAddressSpace *parentSpace, int pid)
{
    numVirtualPages = parentSpace->GetNumPages();
	swapspace = new char [PageSize*numVirtualPages];
    unsigned i, size = numVirtualPages * PageSize;

    DEBUG('a', "Initializing address space, num pages %d, size %d\n",
                                        numVirtualPages, size);
    TranslationEntry* parentPageTable = parentSpace->GetPageTable();
    KernelPageTable = new TranslationEntry[numVirtualPages];
	char* parentSwap = parentSpace->GetSwap();
    for (i = 0; i < numVirtualPages; i++) {
		KernelPageTable[i].virtualPage = i;
		if((!parentPageTable[i].shared) && parentPageTable[i].valid){
			PageTable[parentPageTable[i].physicalPage].blocked = TRUE;
			KernelPageTable[i].physicalPage = GetPhysicalPage(pid);
			unsigned startAddrParent = parentPageTable[i].physicalPage*PageSize;
		    unsigned startAddrChild = KernelPageTable[i].physicalPage*PageSize;
		    for (int j=0; j<PageSize; j++) {
		       machine->mainMemory[startAddrChild+j] = machine->mainMemory[startAddrParent+j];
				swapspace[i*PageSize + j] = parentSwap[i*PageSize + j];
			}
			PageTable[parentPageTable[i].physicalPage].blocked = FALSE;
			currentThread->SortedInsertInWaitQueue(stats->totalTicks + 1000);
			stats->pageFaultCount++;
			PageTable[KernelPageTable[i].physicalPage].entry = &(KernelPageTable[i]);
		} else if (parentPageTable[i].shared)
			KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;
        KernelPageTable[i].valid = parentPageTable[i].valid;
        KernelPageTable[i].use = parentPageTable[i].use;
        KernelPageTable[i].dirty = parentPageTable[i].dirty;
        KernelPageTable[i].readOnly = parentPageTable[i].readOnly; 
		KernelPageTable[i].shared = parentPageTable[i].shared;
		KernelPageTable[i].swapped = parentPageTable[i].swapped;
	}
	codeStart = parentSpace->codeStart;
	dataStart = parentSpace->dataStart;
	codeEnd = parentSpace->codeEnd;
	dataEnd = parentSpace->dataEnd;

}

//----------------------------------------------------------------------
// ProcessAddressSpace::ExpandSpace (int size, int &start) is called by a forked thread.
//      We need to duplicate the address space of the parent.
//----------------------------------------------------------------------
void
ProcessAddressSpace::ExpandSpace (int size, int &start)
{
	int sharedPages = divRoundUp(size, PageSize);
	int oldVpages = this->GetNumPages();
    numVirtualPages = oldVpages + sharedPages;
    unsigned i; size = numVirtualPages * PageSize;

    DEBUG('a', "Initializing shared address space, num pages %d, size %d\n",
                                        numVirtualPages, size);
    // first, set up the translation
    TranslationEntry* parentPageTable = this->GetPageTable();
    KernelPageTable = new TranslationEntry[numVirtualPages];
	int shared = 0; 
    for (i = 0; i < numVirtualPages; i++) {
	    KernelPageTable[i].virtualPage = i;
		if (i < oldVpages){ 
		    KernelPageTable[i].physicalPage = parentPageTable[i].physicalPage;
			KernelPageTable[i].valid = parentPageTable[i].valid;
			KernelPageTable[i].use = parentPageTable[i].use;
	        KernelPageTable[i].dirty = parentPageTable[i].dirty;
		    KernelPageTable[i].readOnly = parentPageTable[i].readOnly;  
			KernelPageTable[i].shared = parentPageTable[i].shared;
		} else {
			KernelPageTable[i].physicalPage = GetPhysicalPage(currentThread->GetPID());
			if (!shared) {shared++ ; start = KernelPageTable[i].virtualPage*PageSize; }
			bzero(&machine->mainMemory[KernelPageTable[i].physicalPage*PageSize], PageSize);
			KernelPageTable[i].valid = TRUE;
	        KernelPageTable[i].use = FALSE;
		    KernelPageTable[i].dirty = FALSE;
			KernelPageTable[i].readOnly = FALSE;  
			KernelPageTable[i].shared = TRUE;
			stats->pageFaultCount++;
			PageTable[KernelPageTable[i].physicalPage].entry = &(KernelPageTable[i]);
		}
    }
	RestoreContextOnSwitch();
}
//----------------------------------------------------------------------
// ProcessAddressSpace::~ProcessAddressSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

ProcessAddressSpace::~ProcessAddressSpace()
{
	for(int i=0; i< numVirtualPages ; i++){
		if (!KernelPageTable[i].shared){
			if(lrulist != NULL)	lrulist->Delete(&(PageTable[KernelPageTable[i].physicalPage]));
			PageTable[KernelPageTable[i].physicalPage].entry = NULL;
			numPagesAllocated--;
			ASSERT(numPagesAllocated >= 0);
		}
	}
   delete KernelPageTable;
}

//----------------------------------------------------------------------
	// ProcessAddressSpace::InitUserModeCPURegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
ProcessAddressSpace::InitUserModeCPURegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numVirtualPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numVirtualPages * PageSize - 16);
}

//----------------------------------------------------------------------
// ProcessAddressSpace::SaveContextOnSwitch
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void ProcessAddressSpace::SaveContextOnSwitch() 
{}

//----------------------------------------------------------------------
// ProcessAddressSpace::RestoreContextOnSwitch
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void ProcessAddressSpace::RestoreContextOnSwitch() 
{
    machine->KernelPageTable = KernelPageTable;
    machine->KernelPageTableSize = numVirtualPages;
}

unsigned
ProcessAddressSpace::GetNumPages()
{
   return numVirtualPages;
}

TranslationEntry*
ProcessAddressSpace::GetPageTable()
{
   return KernelPageTable;
}

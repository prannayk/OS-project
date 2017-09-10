# Group Number 17 

## 150511 -  Prannay Khosla
## 150499 -  Prakhar Agarwal
## 150728 -  Soumye Singhal
## 150298 -  Jaismin Kaur

# GetReg
The argument would have been passed in the Register Number 4. So, to get the Register number which we want to read, we call
machine->ReadRegister(4). Then we call machine->ReadRegister() on that to get the value of that Register. After that we write  the obtained value into the 2nd Register using machine->WriteRegister(2,..).
# GetPA
To get the Physical address for the passed Virtual address we do the following. First we read the Virtual address using ReadRegister(4). We first see if there is a physical page lookup tage or kernel page table and if it exists then we uses that to translate the virtual address to physical address and then return it. This is the same as done in translate.cc and hence we could have alternately just called translate on the read virtual address to get the required physical address.
# GetPID and Get PPID [TODO]
To get the pid we call pid method of the thread object. Each thread has it's pid and ppid's set appropriately. We set the appropriate pid and ppid for any thread object whenever the constructor of the object is being called. When the construtor is called, the pid for the new thread to be created is assigned the value of a global counter. After that the global counter is incremented. Thus every new thread gets a new and unique pid when it is created.
To set the appropriate PPID for any thread, whenever any new thread object's construtor is called we call the pid for the current process which will be the parend for the new thread to be created and then assign this pid to the ppid for the new process to be created. Just one exception is that if the (currentThread == NULL), ie this is the first process to be created and hence wouldn't have any parent, we assign it a ppid = 1.
# GetNumInstr[TODO]
To get the Number of Instructions, we call stats->userTicks() method for the current thread object.
# Time
To get the time, we directly call stats->TotalTicks() and return it by writing it into register number 2.
# Yield
To yield the current running thread to the CPU so that some new thread can be schedules we simply need to call the YieldCPU() method for the current thread object and we are done.
# Sleep
Sleep is executed by maintaining a min heap of all threads in the order of their wake up time. We check for threads that have to removed and added as required at the end of every interrupt that is generated after a single tick. 
# Exec
We create a new address space, push the new executable into it, set up the user registers and then execute the program from the first instruction. 
# Exit
We wake up all waiting processes, which are maintained in a list of sleeping processes. After that is done, we just kill the process by resetting all it's children. 
# Join
We check if the process has exited and otherwise just wait for the process to exit. if it has then we return it's exit status. This is done by maintaining a wait list. Whenever a child process dies it wakes up the required proces and puts it back into execution. It also puts the child exit code into the return register before doing so using a function defined for the NachOSThreadObject. 
# Fork
We create a new process, and set up it's address space by using alternate constructor. We allocate it's space by maintaining the a global memory start pointer. Following that we set up the kernel page table and copy all the physical pages, byte by byte. on completing that we set up the return registers and maintain the context space. We then create a fork function which performs the required tasks to ensure the process can be switched into the processor. On comleting this, the process is automatically added to ready queue and the parent is returned the pid of the forked process. 

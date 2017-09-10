# NachOS
If you find this error during `make`:
```
/usr/bin/ld.bfd.real: cannot find -lstdc++
collect2: error: ld returned 1 exit status
make: *** [nachos] Error 1
```
use the following command:

`sudo ln -s /usr/lib32/libstdc++.so.6 /usr/lib32/libstdc++.so`

## Implemenations of various syscalls

# GetReg
  The argument would have been passed in the Register Number 4. So, to get the Register number which we want to read, we call
  machine->ReadRegister(4). Then we call machine->ReadRegister() on that to get the value of that Register. After that we write   the obtained value into the 2nd Register using machine->WriteRegister(2,..).
# GetPA
  To get the Physical address for the passed Virtual address we do the following. First we read the Virtual address using ReadRegister(4). We first see if there is a physical page lookup tage or kernel page table and if it exists then we uses that to translate the virtual address to physical address and then return it. This is the same as done in translate.cc and hence we could have alternately just called translate on the read virtual address to get the required physical address.
# GetPID

# GetPPID

# GetNumInstr

# Time

# Yield

# Sleep

# Exec

# Exit

# Join

# Fork

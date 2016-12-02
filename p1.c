#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "usloss.h"
#include "vm.h"
#include "phase2.h"
#include "phase5.h"
#define DEBUG        0
#define MAXPROC      50

extern int debugflag;
extern int VM_INIT;
extern int NUM_PAGES;
extern Process processes[MAXPROC];



//>>> Looks ok, same as I was thinking, add inits of PTE attributes as needed
void p1_fork(int pid){

    // check VM_INIT to see if you need to assign a page table to this process
    if (VM_INIT) {

        // initialize slot for process the process table
        processes[pid].numPages = NUM_PAGES;
        processes[pid].procBox = MboxCreate(0, MAX_MESSAGE); 

        // create as many PTEs as there are numpages
        processes[pid].pageTable = (PTE*) malloc(NUM_PAGES * sizeof(PTE));

        // initialize the pages
        for (int i = 0; i < NUM_PAGES; i++) {
            processes[pid].pageTable[i].state = UNUSED;
            processes[pid].pageTable[i].frame = -1;
            processes[pid].pageTable[i].diskBlock = -1;
        }
    }
} // Ends Function p1_fork

// Will need additional steps once DiskTable and FrameTable have been created
void p1_quit(int pid){
  // Per normal - execute only if VM_INIT has occured
  if(VM_INIT){

    // Inform Disk Table that all disk blocks owned by this process can be used
    // (XTODO-TBD)

    // Inform Frame Table that all frames owned by this process are available
    // (XTODO-TBD)

    // Free the Page Table
    free(processes[pid].pageTable);
  }
} // Ends Function p1_quit

// Needs a little more work...
void p1_switch(int old, int new){
  // Per normal - execute only if VM_INIT has occured
  if(VM_INIT){

    // Perform an unmap of the old Process
    for (int i = 0; i < NUM_PAGES; i++) {

      // If the old process i'th Page is in a frame

      // Do a memcpy of the frame 

      // Do a USLOSS_MmuUnmap call

    } // Ends Unmapping of Old Process

    // Perform a map of the new Process
    for (int i = 0; i < NUM_PAGES; i++) {

      // If new process i'th page is in a frame

      // Do a USLOSS_MmuMap call

      // Will also need to update the access bit
      // via USLOSS_MmuSetAccess, but unsure how
      // as of now...

    } // Ends Unmapping of Old Process    

    // Inform VM Stats that a context switch has occured
    vmStats.switches++;

  } // Ends VM_INIT==TRUE conditional implementation
  USLOSS_Console("p1_switch() has completed\n");
} // Ends Function p1_switch
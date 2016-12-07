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
extern int NUM_FRAMES;
extern Process processes[MAXPROC];
extern FTE *FrameTable;
extern DTE *DiskTable;
extern int START_PID;

// IN QUIT, UNMAP WHATEVER WE OWNED IN THE FRAME
// IN SWITCH UNMAP ONLY AS NECESSARY




// Main role - Clear the page table entry for a new process
void p1_fork(int pid){
  if(DEBUG==1){USLOSS_Console("In Function p1_fork\n");}
  // check VM_INIT - if VM System initialized, (re-)initialize page table
  if (VM_INIT) {
    // Re-initialize the Page Table
    // TODO check if this is necessary
    // for (int i = 0; i < NUM_PAGES; i++) {
    //   processes[pid].pageTable[i].state = UNUSED;
    //   processes[pid].pageTable[i].frame = -1;
    //   processes[pid].pageTable[i].diskBlock = -1;
    // }
  }
} // Ends Function p1_fork





// Will need additional steps once DiskTable and FrameTable have been created
// When a Proces Quits:
//Any of its frame table entries need to indicate that there is now no owner
//Any disk blocks it used will indicate there is no owner
void p1_quit(int pid){
  if(DEBUG==1){USLOSS_Console("In Function p1_quit\n");}
  // Per normal - execute only if VM_INIT has occured
  if(VM_INIT){

    // Inform Disk Table that all disk blocks owned by this process can be used
    for (int i = 0; i < NUM_PAGES; i++){
      if(processes[pid%MAXPROC].pageTable[i].diskBlock != -1){
        DiskTable[processes[pid%MAXPROC].pageTable[i].diskBlock].pid  = -1;
        DiskTable[processes[pid%MAXPROC].pageTable[i].diskBlock].page = -1;
      }
    }

    // Inform Frame Table that all frames owned by this process are available
    for (int i = 0; i < NUM_FRAMES; i++){
      if(FrameTable[i].owner==pid){
        FrameTable[i].owner=-1; // XTODO - MUTEX NEEDED
        FrameTable[i].isUsed = UNUSED;
        USLOSS_MmuUnmap(0, i); // ADDED VIA HOMER 12/5
      }
    }
  }
} // Ends Function p1_quit

// Needs a little more work...
void p1_switch(int old, int new){
  if(DEBUG==1){USLOSS_Console("Switching from %d to %d\n", old, new);}

  // Per normal - execute only if VM_INIT has occured
  if(VM_INIT==1){

    // only unmap old if it has a page table
    if(old > START_PID) {
      // Perform an unmap of the old Process
      for (int i = 0; i < NUM_PAGES; i++) {
        // ADDED THIS IN VIA HOMER 12/5
        // TODO check validity of addition of FROZEN
        if (processes[old%MAXPROC].pageTable[i].state == INCORE || processes[old%MAXPROC].pageTable[i].state == FROZEN) {
          USLOSS_MmuUnmap(0, i);
        }
      } // Ends Unmapping of Old Process
    }

    if (new > START_PID) {
      // Perform a map of the new Process
      for (int i = 0; i < NUM_PAGES; i++) {

        // NEED INCORE? BECAUSE IF ON DISK - WE WONT CARE ANYWAY
        if (processes[new%MAXPROC].pageTable[i].state == INCORE) {

          int frame = processes[new%MAXPROC].pageTable[i].frame;

          // Do a USLOSS_MmuMap call
          USLOSS_MmuMap(TAG, i, frame, USLOSS_MMU_PROT_RW);          
        }

      } // Ends Unmapping of Old Process  
    }  
    vmStats.switches++;
  } // Ends VM_INIT==TRUE conditional implementation
  // Inform VM Stats that a context switch has occured
} // Ends Function p1_switch

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

void
p1_fork(int pid)
{

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

    if (DEBUG && debugflag)
        USLOSS_Console("p1_fork() called: pid = %d\n", pid);
} /* p1_fork */

void
p1_switch(int old, int new)
{

    // TODO actually do the unmapping of old and mapping of new

    if (DEBUG && debugflag)
        USLOSS_Console("p1_switch() called: old = %d, new = %d\n", old, new);
} /* p1_switch */

void
p1_quit(int pid)
{
    if (DEBUG && debugflag)
        USLOSS_Console("p1_quit() called: pid = %d\n", pid);
} /* p1_quit */

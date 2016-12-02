/*======================================================================
|  USLOSS Project - Phase 5 : phase5.h
+-----------------------------------------------------------------------
|  Author:      Steven Eiselen | Breelan Lubers
|  Language:    C utilizing USLOSS
|  Class:       CSC 452 Fall 2016
|  Instructor:  Patrick Homer
|  Purpose:     Contains definitions for the Phase 5 Implementation
+=====================================================================*/

#ifndef _PHASE5_H
#define _PHASE5_H
#define PHASE_3      // To get correct results from usyscall.h

#include <usloss.h>
#include <mmu.h>
#include <stdio.h>
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>

/*----------------------------------------------------------------------
|>>> Phase 5 Constants / External Variables
+---------------------------------------------------------------------*/
#define PAGER_PRIORITY 2 // Pager Priority
#define MAXPAGERS 4      // Maximum number of pagers

extern int VM_INIT;      // Flag indicating if VM System was initialized

/*----------------------------------------------------------------------
|>>> Phase 5 External Function Declarations
+---------------------------------------------------------------------*/
extern int start5(char *);
void       PrintStats(void);

/*----------------------------------------------------------------------
|>>> Paging Statistics Structure
+---------------------------------------------------------------------*/
typedef struct VmStats {
  int pages;          // Size of VM region, in pages
  int frames;         // Size of physical memory, in frames
  int diskBlocks;     // Size of disk, in blocks (pages)
  int freeFrames;     // # of frames that are not in-use
  int freeDiskBlocks; // # of blocks that are not in-use
  int switches;       // # of context switches
  int faults;         // # of page faults
  int new;            // # faults caused by previously unused pages
  int pageIns;        // # faults that required reading page from disk
  int pageOuts;       // # faults that required writing a page to disk
  int replaced;       // # Via Dr. Homer - Will not be implementing
} VmStats;
extern VmStats vmStats;

#endif /* _PHASE5_H */
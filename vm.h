/*======================================================================
|  USLOSS Project - Phase 5 : vm.h
+-----------------------------------------------------------------------
|  Author:      Steven Eiselen | Breelan Lubers
|  Language:    C utilizing USLOSS
|  Class:       CSC 452 Fall 2016
|  Instructor:  Patrick Homer
|  Purpose:     Defines Data Structures and Constants used with/by the
|               Virtual Memory Subsystem for the Phase 5 Implementation
+=====================================================================*/

/*----------------------------------------------------------------------
|>>> Tag - All processes use the same tag
+---------------------------------------------------------------------*/
#define TAG 0

/*----------------------------------------------------------------------
|>>> Different States for a Page
+-----------------------------------------------------------------------
| > USUSED - the page is not used
| > INCORE - the page is in in a frame (i.e. in 'core' memory)

PTE Statuses via Homer:
PAGEOUT - need frame for other purpose, so write it to disk
PAGEIN - bring the page into the frame


+---------------------------------------------------------------------*/
#define UNUSED 500
#define INCORE 501 // RENAME TO PAGEINFRAME
#define ONDISK 502 // RENAME TO PAGEOUTDISK
#define ISUSED 503

/*----------------------------------------------------------------------
|>>> Different States for a Frame
+---------------------------------------------------------------------*/
#define DIRTY  1
#define CLEAN  0
#define ISREF  2
#define UNREF  3


/*----------------------------------------------------------------------
|>>> Structure PTE - Page Table Entry
+-----------------------------------------------------------------------
| Purpose: Models a Virtual Memory Page Table Entry. Attributes include
|          the state of a page, the frame that stores the page (if any),
|          and the disk block that stores the page (if any).
|
| Notes:   A) Additional Attributes - Dr. Homer mentioned in class that
|             there is no need for any additional attributes outside of
|             the three already provided in the skeleton code.
|
|          B) Attribute Purpose/Values:
|              > state =     state of the PTE (see state defs above)
|              > frame =     frame that stores the page, else -1 if none
|              > diskBlock = disk block that stores the page, -1 if none          
+---------------------------------------------------------------------*/
typedef struct PTE {
  int  state;
  int  frame;
  int  diskBlock;
} PTE;


/*----------------------------------------------------------------------
|>>> Structure FTE - Frame Table Entry
+-----------------------------------------------------------------------
| Purpose: Models a Frame Table Entry...
|
| Notes:   A) x
|
|          B) Attribute Purpose/Values:
|              > state =     state of the PTE (see state defs above)
|              > frame =     frame that stores the page, else -1 if none
|              > diskBlock = disk block that stores the page, -1 if none          
+---------------------------------------------------------------------*/
typedef struct FTE{
  int owner;
  int isUsed;
  int page;
  int isDirty;      // Examine this bit to see if PTE is dirty or not
  int isReferenced; // ??? needed? or is this the same as state?
}FTE;


// Disk Table Entry
typedef struct DTE {
    int pid;         // Process that owns this sector
    int page;        // Page # associated with this sector
} DTE;

/*
 * Per-process information.
 */
typedef struct Process {
    int  numPages;   // Size of the page table.
    PTE  *pageTable; // The page table for the process.
    // Removed procBox, as associated FaultMsg takes care of this role
    // Add more stuff here */
} Process;

/*
 * Information about page faults. This message is sent by the faulting
 * process to the pager to request that the fault be handled.
 */
typedef struct FaultMsg {
    int  pid;        // Process with the problem.
    void *addr;      // Address that caused the fault.
    int  replyMbox;  // Mailbox to send reply.
    // Add more stuff here.
} FaultMsg;

#define CheckMode() assert(USLOSS_PsrGet() & USLOSS_PSR_CURRENT_MODE)

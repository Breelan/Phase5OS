/*======================================================================
|  USLOSS Project - Phase 5 : phase5.c
+-----------------------------------------------------------------------
|  Author:      Steven Eiselen | Breelan Lubers
|  Language:    C utilizing USLOSS
|  Class:       CSC 452 Fall 2016
|  Instructor:  Patrick Homer
|  Purpose:     Implements Phase 5
+-----------------------------------------------------------------------
 >>>>>>>>>>>>>>>>>>>> OPERATION TOTALITY UNISPHERE <<<<<<<<<<<<<<<<<<<<

Notes:

 > XTODO: Relocate function definitions to Phase5.h before turnin


+=====================================================================*/
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <vm.h>
#include <string.h>

/*----------------------------------------------------------------------
|>>> These are needed to work with libpatrickphase4.a
+---------------------------------------------------------------------*/
extern void mbox_create(systemArgs *args_ptr);
extern void mbox_release(systemArgs *args_ptr);
extern void mbox_send(systemArgs *args_ptr);
extern void mbox_receive(systemArgs *args_ptr);
extern void mbox_condsend(systemArgs *args_ptr);
extern void mbox_condreceive(systemArgs *args_ptr);

/*----------------------------------------------------------------------
|>>> Data Structure / Variable / ID Definitions
+-----------------------------------------------------------------------
| Implementation Notes:
|
| > FaultMsg faults: Via Dr. Homer - "Processes can only have one page
|                    fault at a time, so we can allocate the messages 
|                    statically and index them by PID." Furthermore, we
|                    will exploit this data structure to use as storage
|                    for the private mailboxes that processes will wait
|                    on whenever they execute a MMU Interrupt and block
|                    pending completion of a Pager action via a message
|
| > void *vmRegion:  Dr. Homer recommended that we define the vmRegion
|                    in this file, so that's what we'll do.
|
| > int pagerID:     The maximum number of Pager Daemons is defined by
|                    USLOSS, ergo we can safely declare an array of such
|                    size with the guarantee thereof. NUM_PAGERS defines
|                    the number of Pager Daemons as passed in by call of
|                    VmInit, and will be used to indicate how many Pager
|                    Daemons the system uses, which can be <MAXPAGERS
|
----------------------------------------------------------------------*/

// Data Structures _____________________________________________________
static Process processes[MAXPROC]; // Phase 5 Process Table
FaultMsg       faults[MAXPROC];    // Fault Info Table, defined in vm.h
VmStats        vmStats;            // VM system stats, defined in vm.h 
void           *vmRegion;          // Start address of Virtual Memory

// Pager PIDs / Pager Mailbox IDs ______________________________________
int            pagerID[MAXPAGERS]; // Pager Daemon PIDS, max is defined
int            pagerBox;           // Mailbox ID that Pagers wait on

// VM System Settings (Assigned Via VmInit) ____________________________
int            NUM_PAGES;          // Number of Pages
int            NUM_MAPPINGS;       // Number of Mappings
int            NUM_FRAMES;         // Number of Frames
int            NUM_PAGERS;         // Number of Pager Daemons

// Misc. / 'In a Rush' Added ___________________________________________
int *          DiskTable;          // Current state of Disk 1
int            VM_INIT;            // Informs if VmInit has completed


/*----------------------------------------------------------------------
|>>> Interrupt and Syscall Declarations
+-----------------------------------------------------------------------
| Implementation Notes:
|
| > FaultHandler is installed into the USLOSS_IntVec via VmInitReal
| > vmInit is installed into the Syscall Vector via start4
| > vmDestroy is installed into the Syscall Vector via start4
+---------------------------------------------------------------------*/
static void FaultHandler(int  type, void *arg); // MMU Interrupt Handler
static void vmInit(systemArgs *sysargsPtr);     // Syscall Handler
void        *vmInitReal(int mappings, int pages, int frames, int pagers);
static void vmDestroy(systemArgs *sysargsPtr);  // Syscall Handler


/*----------------------------------------------------------------------
|>>> Util Function and Process Declarations
+-----------------------------------------------------------------------
| Implementation Notes:
|
| > x
+---------------------------------------------------------------------*/
void        PrintStats(void); // Prints the VmStats Struct Attributes
static int  Pager(char *buf); // Implements the Pager Process
void        clockAlgo();      // Placeholder for the Clock Algorithm








/*----------------------------------------------------------------------
|>>> Function start4
+-----------------------------------------------------------------------
|
| Purpose: > Initializes the VM system call handlers
|          > Initializes the Phase 5 Process Table
|          > Spawns the start5 'Entry Point' Process
|
+---------------------------------------------------------------------*/
int start4(char *arg) {

  // Used for Spawn and Wait calls per usual
  int pid; int result; int status;

  // Indicate VmInit has not completed (might not need / but keeping!)
  VM_INIT = 0;

  // XTODO - Still not sure why these are needed, but here they are!
  systemCallVec[SYS_MBOXCREATE]      = mbox_create;
  systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
  systemCallVec[SYS_MBOXSEND]        = mbox_send;
  systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
  systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
  systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

  // Install the Syscall Vector members for vmInit/vmDestroy
  systemCallVec[SYS_VMINIT]    = vmInit;
  systemCallVec[SYS_VMDESTROY] = vmDestroy;

  //####################################################################
  //>>> CONSTRUCTION ZONE, ALL CODE ABOVE AND BELOW LOOKS OKAY

    int temp;

    for (int i = 0; i < MAXPROC; i++){
      Mbox_Create(0, MAX_MESSAGE, &temp);
      processes[i].procBox = temp;
    }

    /* initialize the Disk Table */

    // get information about disk1
    int numSectors;
    int numTracks;
    int diskSize;

    DiskSize(1, &numSectors, &numTracks, &diskSize);

    // allocate memory for DiskTable
    DiskTable = (int *)malloc(numTracks * numSectors * sizeof(int));

  //####################################################################

  // Spawn start5, then wait for it to terminate (compressing code to save lines)
  result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, PAGER_PRIORITY, &pid);
  if(result != 0){USLOSS_Console("start4(): Error spawning start5\n");Terminate(1);}
  result = Wait(&pid, &status);
  if(result != 0){USLOSS_Console("start4(): Error waiting for start5\n");Terminate(1);}
  Terminate(0);
  return 0; // Should never reach here, but makes GCC happy per usual
} // Ends Process start4






/*
 *----------------------------------------------------------------------
 *
 * VmInit --
 *
 * Unpacks and checks the systemArgs passed to it before calling vmInitReal 
 * to handle the real logic.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is initialized.
 *
 *----------------------------------------------------------------------
 */
static void
vmInit(systemArgs *sysargsPtr)
{
    //USLOSS_Console("made it into vmInit\n");
    CheckMode(); // should be in kernel mode now

    // unpack args
    int mappings = (int) ((long) sysargsPtr->arg1);
    int pages = (int) ((long) sysargsPtr->arg2);
    int frames = (int) ((long) sysargsPtr->arg3);
    int pagers = (int) ((long) sysargsPtr->arg4);

    // check args

    // cannot create more than max pagers
    if(pagers > MAXPAGERS) {
        USLOSS_Console("too many pagers!\n");
      sysargsPtr->arg4 = (void *)((long) -2);
      return;
    }

    // check if VM region has already been initialized
    if(VM_INIT == 1) {
      USLOSS_Console("vm region already initialized!\n");
      sysargsPtr->arg4 = (void *)((long) -2);
      return;
    }

    // mappings should equal pages
    if(mappings != pages) {
      USLOSS_Console("mappings do not equal pages!\n");
      sysargsPtr->arg4 = (void *)((long) -1);
      return;
    }

    // call vmInitReal
    int result = (int) (long)vmInitReal(mappings, pages, frames, pagers);

    // result should hold address of the first byte in the VM region
    sysargsPtr->arg1 = (void *)((long) result);
    sysargsPtr->arg4 = 0;
    return;

} /* vmInit */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroy --
 *
 * Stub for the VmDestroy system call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      VM system is cleaned up.
 *
 *----------------------------------------------------------------------
 */

static void
vmDestroy(systemArgs *sysargsPtr)
{
   CheckMode();
} /* vmDestroy */


/*
 *----------------------------------------------------------------------
 *
 * vmInitReal --
 *
 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
 *
 *----------------------------------------------------------------------
 */
void *
vmInitReal(int mappings, int pages, int frames, int pagers)
{
   int status;
   int dummy;
   int pid;

   CheckMode();
   // THE FOLLOWING CAME WITH SKELETON
   // status = USLOSS_MmuInit(mappings, pages, frames);
   // if (status != USLOSS_MMU_OK) {
   //    USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
   //    abort();
   // }

   // install fault handler
   USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;


   /*
    * Initialize page tables.
    */



   /* 
    * Create the fault mailbox.
    */
    pagerBox = MboxCreate(pagers, MAX_MESSAGE);


   /*
    * Fork the pagers. Store pager pids in global variables.
    */
    // for(int i = 0; i < pagers; i++) {
    //   if(i == 0) {
    //     pager0ID = fork1("pager0", Pager, NULL, USLOSS_MIN_STACK, PAGER_PRIORITY);
    //   }
    //   if(i == 1) {
    //     pager1ID = fork1("pager1", Pager, NULL, USLOSS_MIN_STACK, PAGER_PRIORITY);
    //   }
    //   if(i == 2) {
    //     pager2ID = fork1("pager2", Pager, NULL, USLOSS_MIN_STACK, PAGER_PRIORITY);   
    //   }
    //   if(i == 3) {
    //     pager3ID = fork1("pager3", Pager, NULL, USLOSS_MIN_STACK, PAGER_PRIORITY);
    //   }      
    // }

   /*
    * Zero out, then initialize, the vmStats structure
    */
   memset((char *) &vmStats, 0, sizeof(VmStats));
   
   // vmStats.pages = pages;
   // vmStats.frames = frames;

   // for now, ignore inputs and create an equal number of pages & frames
   vmStats.pages = pages;
   vmStats.frames = frames;
   /*
    * Initialize other vmStats fields.
    */
   vmStats.diskBlocks = 32; // pages are 8KB in size
   vmStats.freeFrames = frames;    // all frames are free at this point
   vmStats.freeDiskBlocks = 32; // all diskBlocks are free at this point
   vmStats.switches = 10;           // there have been no switches yet
   vmStats.faults = 0;             // no faults have occured yet
   vmStats.new = 0;                
   vmStats.pageIns = 0;
   vmStats.pageOuts = 0;
   vmStats.replaced = 0;

   // initialize VM region with MmuInit
   status = USLOSS_MmuInit(mappings, pages, frames);

   // check if there is an error
   if (status != USLOSS_MMU_OK) {
      USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
      abort();
   }

   // set up a mapping in the MMU
   // QUESTION is protection (3rd arg) same as page state?
   status = USLOSS_MmuMap(TAG, 0, 0, USLOSS_MMU_PROT_RW);
   //USLOSS_Console("status from MmuMap is %d\n", status);

   // print vmStats
   //PrintStats();

   vmStats.faults = 1;
   vmStats.new = 1;

   VM_INIT = 1;

   return USLOSS_MmuRegion(&dummy);
} /* vmInitReal */


/*
 *----------------------------------------------------------------------
 *
 * PrintStats --
 *
 *      Print out VM statistics.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Stuff is printed to the USLOSS_Console.
 *
 *----------------------------------------------------------------------
 */
void
PrintStats(void)
{
     USLOSS_Console("VmStats\n");
     USLOSS_Console("pages:          %d\n", vmStats.pages);
     USLOSS_Console("frames:         %d\n", vmStats.frames);
     USLOSS_Console("diskBlocks:     %d\n", vmStats.diskBlocks);
     USLOSS_Console("freeFrames:     %d\n", vmStats.freeFrames);
     USLOSS_Console("freeDiskBlocks: %d\n", vmStats.freeDiskBlocks);
     USLOSS_Console("switches:       %d\n", vmStats.switches);
     USLOSS_Console("faults:         %d\n", vmStats.faults);
     USLOSS_Console("new:            %d\n", vmStats.new);
     USLOSS_Console("pageIns:        %d\n", vmStats.pageIns);
     USLOSS_Console("pageOuts:       %d\n", vmStats.pageOuts);
     USLOSS_Console("replaced:       %d\n", vmStats.replaced);
} /* PrintStats */


/*
 *----------------------------------------------------------------------
 *
 * vmDestroyReal --
 *
 * Called by vmDestroy.
 * Frees all of the global data structures
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The MMU is turned off.
 *
 *----------------------------------------------------------------------
 */
void
vmDestroyReal(void)
{

   CheckMode();
   USLOSS_MmuDone();
   /*
    * Kill the pagers here.
    */
   /* 
    * Print vm statistics.
    */
   USLOSS_Console("vmStats:\n");
   USLOSS_Console("pages: %d\n", vmStats.pages);
   USLOSS_Console("frames: %d\n", vmStats.frames);
   USLOSS_Console("blocks: %d\n", vmStats.diskBlocks);
   /* and so on... */

} /* vmDestroyReal */

/*
 *----------------------------------------------------------------------
 *
 * FaultHandler
 *
 * Handles an MMU interrupt. Simply stores information about the
 * fault in a queue, wakes a waiting pager, and blocks until
 * the fault has been handled.
 *
 * Results:
 * None.
 *

  > int  type == USLOSS_MMU_INT
  > void *arg == Offset within VM region


 * Side effects:
 * The current process is blocked until the fault is handled.
 *
 *----------------------------------------------------------------------
 */

int tester = 0;
static void FaultHandler(int type ,void *arg){
  //USLOSS_Console("FaultHandler was called\n");
  int temp = processes[getpid()%MAXPROC].procBox;


  //MboxReceive(temp, NULL, MAX_MESSAGE);

  int ag = (int) ((long) arg);



  // int temp2 = USLOSS_MmuGetCause(); // <<< Cause Code 1 == USLOSS_MMU_FAULT == Address was in unmapped page
  

  int temp2 = USLOSS_MmuMap(1, 0, 1, USLOSS_MMU_PROT_RW);
  USLOSS_Console("return val of MmuMap == %d\n", temp2 );


  USLOSS_MmuSetAccess(1, USLOSS_MMU_REF);
  USLOSS_Console("cause == %d\n", USLOSS_MmuGetCause() );
  
  tester++; if(tester == 10){USLOSS_Halt(0);}

  //int cause;

  //int offset = (int) (long) arg;

  //assert(type == USLOSS_MMU_INT);
  //cause = USLOSS_MmuGetCause();
  //assert(cause == USLOSS_MMU_FAULT);
  //vmStats.faults++;

   /*
    * Fill in faults[pid % MAXPROC], send it to the pagers, and wait for the
    * reply.
    */

} /* FaultHandler */

/*
 *----------------------------------------------------------------------
 *
 * Pager 
 *
 * Kernel process that handles page faults and does page replacement.
 *
 * Results:
 * None.
 *
 * Side effects:
 * None.
 *
 *----------------------------------------------------------------------
 */
static int
Pager(char *buf)
{
    while(1) {
        /* Wait for fault to occur (receive from mailbox) */
        /* Look for free frame */
        /* If there isn't one then use clock algorithm to
         * replace a page (perhaps write to disk) */
        /* Load page into frame from disk, if necessary */
        /* Unblock waiting (faulting) process */

       // block on mailbox
       char buf[MAX_MESSAGE];
       MboxReceive(pagerBox, buf, MAX_MESSAGE);
    }
    return 0;
} /* Pager */

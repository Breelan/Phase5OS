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
 > XTODO: Utilization of Assert and Abort functions???


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
// Will probably need these two at some point...
#include <stdlib.h>
#include <stdio.h>

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
Process processes[MAXPROC]; // Phase 5 Process Table
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
void           *VM_REGION;          // Where vmRegion starts

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
| Purpose: > Initializes the VM system call handlers
|          > Initializes the Phase 5 Process Table
|          > Spawns the start5 'Entry Point' Process
|
| Parms:   char *arg - Process Argument per normal
|
| Effects: > Results    - MMU Return Status
|          > Systemwide - The MMU is initialized
|
| Returns: Technically nothing, as it calls Terminate
+---------------------------------------------------------------------*/
int start4(char *arg) {
  // Used for Spawn and Wait calls per usual
  int pid; int result; int status;

  // Indicate VmInit has not completed (might not need, but keeping!)
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

  // Spawn start5, then wait for it to terminate (compressing code to save lines)
  result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, PAGER_PRIORITY, &pid);
  if(result != 0){USLOSS_Console("start4(): Error spawning start5\n");Terminate(1);}
  result = Wait(&pid, &status);
  if(result != 0){USLOSS_Console("start4(): Error waiting for start5\n");Terminate(1);}
  Terminate(0);
  return 0; // Should never reach here, but makes GCC happy per usual
} // Ends Process start4


/*----------------------------------------------------------------------
|>>> Syscall Handler vmInit
+-----------------------------------------------------------------------
| Purpose: > Unpacks and checks the systemArgs passed to it
|          > Checks that these parameters are valid input
|          > Calls vmInitReal to actually implement the operation
|
| Parms:   systemArgs *args - struct containing input from VmInit call
|
| Effects: > Results    - None
|          > Systemwide - The VM System is initialized
|
| Returns: Nothing
+---------------------------------------------------------------------*/
static void vmInit(systemArgs *args){

  CheckMode(); // Kernel Mode Check

  //>>> Phase 1 - Unpack systemArgs arguments
  int mappings = (int) ((long) args->arg1);
  int pages =    (int) ((long) args->arg2);
  int frames =   (int) ((long) args->arg3);
  int pagers =   (int) ((long) args->arg4);

  //>>> Phase 2 - Check arguments for validity

  // Check 1: Cannot create more than max pagers
  if(pagers > MAXPAGERS) {
    USLOSS_Console("too many pagers!\n");
    args->arg4 = (void *)((long) -1);
    return;
  }

  // Check 2: VM Region can only be initialized once
  if(VM_INIT == 1) {
    USLOSS_Console("vm region already initialized!\n");
    args->arg4 = (void *)((long) -2);
    return;
  }

  // Check 3: Mappings should equal pages
  if(mappings != pages) {
    USLOSS_Console("mappings do not equal pages!\n");
    args->arg4 = (void *)((long) -1);
    return;
  }

  //>>> Phase 3 - Call vmInitReal to initialize VM Subsystem
  // XTODO - Couldn't we directly assign return of vmInitReal to arg1 ???
  // XTODO - I am also suspecting that maybe we have to do a local assignment 
  //         of vmRegion = result as well. Asked for clarification on Piazza
  int result = (int) (long)vmInitReal(mappings, pages, frames, pagers);

  // Result should hold address of first byte in VM region
  args->arg1 = (void *)((long) result);
  args->arg4 = 0;
  return;
} // Ends Syscall Handler vmInit


/*----------------------------------------------------------------------
|>>> Function vmInitReal
+-----------------------------------------------------------------------

 * Called by vmInit.
 * Initializes the VM system by configuring the MMU and setting
 * up the page tables.
 *
 * Results:
 *      Address of the VM region.
 *
 * Side effects:
 *      The MMU is initialized.
+-----------------------------------------------------------------------
| Implementation Notes: 
| 
| > Page Table attribute initialization values (Via Homer):
|    o aProcess.state     = UNUSED;
|    o aProcess.frame     = -1 (becaue memory can have a 0th frame)
|    o aProcess.diskBlock = -1 (because disk can have a 0th block)
 *

+---------------------------------------------------------------------*/
void * vmInitReal(int mappings, int pages, int frames, int pagers){

  CheckMode(); // Kernel Mode Check

  int status;
  int dummy;
  int pid;
  int i = 0; // Used for loops (C-99 habit I've developed!)

  //>>> Phase 1 - Assign MMU Settings to globals, each is valid at this point
  NUM_MAPPINGS = mappings;
  NUM_PAGES    = pages;
  NUM_FRAMES   = frames;
  NUM_PAGERS   = pagers;

  //>>> Phase 2 - Call USLOSS_MmuInit, check its return value to detect error
  status = USLOSS_MmuInit(mappings, pages, frames);
  if(status != USLOSS_MMU_OK){
    USLOSS_Console("vmInitReal: couldn't init MMU, status %d\n", status);
    abort(); // XTODO: Here's a new C function!
  }

  //>>> Phase 3 - Install Fault Handler
  USLOSS_IntVec[USLOSS_MMU_INT] = FaultHandler;

  //>>> Phase 4 - Initialize Page Tables (see documentation for more info)
  // for (int i = 0; i < MAXPROC; i++){
  //   processes[i].PTE.state     = UNUSED;
  //   processes[i].PTE.frame     = -1;
  //   processes[i].PTE.diskBlock = -1;
  // }

  //>>> Phase 5 - Initialize Frame Table (TBD)


  //>>> Phase 6 - Create Fault Mailboxes
  // XTODO - I THINK THIS MEANS THE BOXES IN faults[MAXPROC]

  // Note: Can assign permanent mailbox on guarantee that Pager will always do a send on a box
  for (int i = 0; i < MAXPROC; i++){
    faults[i].pid = -1;
    faults[i].addr  = NULL;
    faults[i].replyMbox = MboxCreate(0, 0);
  }

  //>>> Phase 7 - Create the Pager Daemon 'Requests Mailbox / Queue'
  pagerBox = MboxCreate(pagers, MAX_MESSAGE);

  //>>> Phase 6 - Fork the Pager Daemons, store pager pids in global variables.
  char buf[10];
  for (i = 0; i < NUM_PAGERS; i++) {
    sprintf(buf, "%d", i); // Why not tell the pager which one it is?
    pagerID[i] = fork1("PagerDaemon", Pager, buf, USLOSS_MIN_STACK, PAGER_PRIORITY);
  } // Borrowed from Dr. Homer's Phase 4 skeleton code


      /* initialize the Disk Table */

    // get information about disk1
    int numSectors;
    int numTracks;
    int diskSize;

    diskSizeReal(1, &numSectors, &numTracks, &diskSize);

    // allocate memory for DiskTable
    DiskTable = (int *)malloc(numTracks * numSectors * sizeof(DTE));





  //>>> Phase 7 - Zero out, then initialize vmStats structure
  memset((char *) &vmStats, 0, sizeof(VmStats));
  vmStats.pages      = pages;
  vmStats.frames     = frames;
  vmStats.freeFrames = frames;
  vmStats.switches   = 10;     // Placeholder - should be 0 (zero)
  vmStats.faults     = 0;      // Placeholder - should be 0 (zero)
  vmStats.new        = 1;      // Placeholder - should be 0 (zero)  
  vmStats.pageIns    = 0;
  vmStats.pageOuts   = 0;
  vmStats.replaced   = 0;

  /* XTODO: I think we need to do a diskSizeReal call here. From the Phase 4 Spec - 

     > int diskSizeReal(int unit, int *sector, int *track, int *disk)

     > Returns information about the size of the disk indicated by unit. 

     > Sector pointer filled in with number of bytes in a sector
     > Track  pointer filled in with number of sectors in a track
     > Disk   pointer filled in with number of tracks in the disk.
  */
  vmStats.diskBlocks = diskSize; // DO A DISK SIZE REAL CALL HERE??? 
  vmStats.freeDiskBlocks = diskSize; // 


  //>>> TEMP - Set up sample mapping in MMU
  // status = USLOSS_MmuMap(TAG, 0, 0, USLOSS_MMU_PROT_RW);
  //####################################################################

  //>>> Phase 8 - Assign VM_INIT to 1, Return address of VM Region
  VM_INIT = 1; 
  VM_REGION = USLOSS_MmuRegion(&dummy);
  return VM_REGION;
} // Ends Function vmInitReal



/*----------------------------------------------------------------------
|>>> Interrupt Handler FaultHandler
+-----------------------------------------------------------------------
| Purpose: Handles a MMU Interrupt event via the following algorithm...
|           
| Algo:    1) Does validity assertion checks (inherited from skeleton)
|          2) Stores information about the fault in the FaultMsg struct
|             associated with the PID of currently executing process. In
|             particular, informs the FaultMsg:
|               A) The PID of the requesting process
|               B) The Memory Offset from vmRegion of the request
|          3) Sends a message to the pagerBox mailbox. A waiting Pager
|             Daemon will at some point attempt to block (or has been
|             blocked waiting) on pagerBox and will service the request.
|             This could block the process until an available pager can
|             service the request - but the internal mailbox system will
|             then manifest a 'waitingSenders' queue.
|          4) The pager servicing the request will awaken the process
|             via process' mailbox ID when it completes the request.
|
| Parms:   > int  type == USLOSS_MMU_INT
|          > void *arg == Offset within VM region
|
| Effects: > Populates a FaultMsg struct, adds request to pagerBox
|          > The current process is blocked until the fault is handled.
|
| Returns: Nothing, and this helped affirm my intuition that the way in
|          which the 'synchronization' between the Fault Handler and the
|          Pager Daemons was explained in the spec and documentation was 
|          unclear, and worse - potentially misleading.
+---------------------------------------------------------------------*/
static void FaultHandler(int  type , void *arg){
  
  int cause;
  int offset = (int) (long)arg;
  int pid = getpid();

  //>>> Phase 1 - Perform validity assertion (Homer's Code via skeleton)
  assert(type == USLOSS_MMU_INT);
  cause = USLOSS_MmuGetCause();
  assert(cause == USLOSS_MMU_FAULT);
  vmStats.faults++;
  
  //>>> Phase 2  - Create a new message to send to Pager Daemon
  faults[pid%MAXPROC].pid = pid;
  faults[pid%MAXPROC].addr = vmRegion + offset;
   
  //>>> Phase 3 - Send the Message, PID as the key
  char buf[MAX_MESSAGE];
  sprintf(buf, "%d", pid); // convert int to string
  MboxSend(pagerBox, buf, sizeof(int));

  //>>> Phase 4 - Block on the procss' private mailbox
  MboxReceive(faults[pid%MAXPROC].replyMbox, NULL, 0);

} // Ends Interrupt Handler FaultHandler

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
    int status;

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

       // convert string to int and get the pid of the requesting process
       int frameId = atoi(buf);

       //USLOSS_Console("yolo = %s\n", buf);

       //>>> If it gets -1, is VmDestroyReal killing it - do thusly...
      if(frameId == -1){quit(1);}

       // get the faultmsg associated with the pid
       FaultMsg msg = faults[frameId];

       // get the frame number
       int frame = ( ( (int) ((long)msg.addr)) - ((int) ((long)VM_REGION)))/ (NUM_FRAMES * 8);

       // need to check the process' pages to see if we can use one
       int page = 0;
       for (int i = 0; i < processes[frameId].numPages; i++) {
          if (processes[frameId].pageTable[i].state == UNUSED) {
            page = i;
          }
       }

       // do the mapping TODO move this to p1_switch
       status = USLOSS_MmuMap(TAG, page, frame, USLOSS_MMU_PROT_RW);

       // wake up the faulting process
       MboxSend(faults[frameId%MAXPROC].replyMbox, NULL, 0);

    }
    return 0;
} /* Pager */




/*----------------------------------------------------------------------
|>>> Syscall Handler vmDestroy
+-----------------------------------------------------------------------
| Purpose: Wrapper function for vmDestroyReal, which actually implements
|          the termination of the VM Subsystem. If VM_INIT==1, i.e. the
|          VM Subsystem was initialized: vmDestroyReal is called. 
|
|          Otherwise, the handler returns having done nothing.
|
| Parms:   systemArgs *args - struct containing input from VmInit call
| Effects: If the VM System was initialized, the VM System is terminated
|          (i.e. 'cleaned up' via Dr. Homer) via the vmDestroyReal call.
| Returns: Nothing
+---------------------------------------------------------------------*/
static void vmDestroy(systemArgs *args){
  CheckMode(); // Are we in Kernel Mode?
  if(VM_INIT == 1){vmDestroyReal();}
} // Ends Syscall Handler vmDestroy


/*----------------------------------------------------------------------
|>>> Function vmDestroyReal
+-----------------------------------------------------------------------
| Purpose: Called by vmDestroy iff the VM Subsystem has been initialized
|          this function implments the termination of the VM Subsystem.
|
| Algo:    1) Calls USLOSS_MmuDone, which via USLOSS Manual Sec 5.2 will
|             "release all the resources associated with the MMU"
|
|          2) Kills all Pager processes by sending them special messages
|             which trigger them to terminate. At present, this message
|             will be a PID of -1 (negative one), though we may change
|             this in future development WLOG to the main intention.
|
|          3) Sends the aforementioned messages to the pager processes
|             one by one in a loop. Following each message send will be
|             a call of join - this will block vmDestroyReal until the
|             corresponding Pager process terminates and quits, which
|             guarantees that all Pager processes will have quit when
|             vmDestroyReal returns.
|
|          4) Frees malloced Data Structures. At the present time, this
|             may include each Process' Page Table, th Frame Table, the
|             Disk Table, and/or other structures as they come up.
|
| Parms:   None
| Effects: Systemwide - The MMU is turned off
+---------------------------------------------------------------------*/
void vmDestroyReal(void){
  CheckMode(); // Are we in Kernel Mode?

  //>>> Phase 1 - Call USLOSS_MmuDone
  USLOSS_MmuDone();

  char yolo[1];

  sprintf(yolo, "%d", -1);


  //>>> Phase 2 - Kill the Pagers
  int status;
  for (int i = 0; i < NUM_PAGERS; i++){
    MboxSend(pagerBox, yolo, sizeof(int));
    join(&status);
  }

  //>>> Phase 3 - Free malloced structures

  // X-TODO Actually freaking do this

  //>>> Phase 4 - Print VM Stats (Not mentioned in spec - but its in your skeleton code so...)

  //PrintStats(); // Might not be what Homer did

  // Keeping these in case Homer wants to print these 4 and not call PrintStats
  // USLOSS_Console("vmStats:\n");
  // USLOSS_Console("pages: %d\n", vmStats.pages);
  // USLOSS_Console("frames: %d\n", vmStats.frames);
  // USLOSS_Console("blocks: %d\n", vmStats.diskBlocks);

  // We no longer have an initialized MMU / VM Subsystem! 
  VM_INIT = 0;

} // Ends Function vmDestroyReal

/*----------------------------------------------------------------------
|>>> Function PrintStats
+-----------------------------------------------------------------------
| Purpose: Prints out attributes of the vmStats VM statistics struct.
| Parms:   systemArgs *args - struct containing input from VmInit call
| Effects: Stuff is printed to the USLOSS_Console.
+---------------------------------------------------------------------*/
void PrintStats(void){
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
} // Ends Function PrintStats

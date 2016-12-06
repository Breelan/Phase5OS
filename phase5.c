/*======================================================================
|>>> USLOSS Project - Phase 5 : phase5.c
+-----------------------------------------------------------------------
|  Author:      Steven Eiselen | Breelan Lubers
|  Language:    C utilizing USLOSS
|  Class:       CSC 452 Fall 2016
|  Instructor:  Patrick Homer
|  Purpose:     Implements Phase 5
+-----------------------------------------------------------------------
|>>> Implementation Notes for Phase 5
|      
|
| > Preface:
|
|   I put lots of time and effort into this, so I want to show some of
|   the notes taken along the way, just as I did with Phases 2 and 3.
|
|   This is also my penance for a poor delivery on Phase 4.
|
|
| > Bitwise Math Proofs for Clock/Pager MMU Access Set stuff:

 > C+U == 00 == 0
 > C+R == 01 == 1 <- is USLOSS_MMU_REF as it encompasses Reference Bit's ID
 > D+U == 10 == 2 <- is USLOSS_MMU_DIRTY as it encompasses Dirty Bit's ID
 > D+R == 11 == 3

 Ergo...

 > QUERY: IS REFERENCED? => (FULL BITS)&(USLOSS_MMU_REF)
     o (C+R)&(C+R) == (01)&(01) == (01) == USLOSS_MMU_REF
     o (D+R)&(C+R) == (11)&(01) == (01) == USLOSS_MMU_REF

 > QUERY: IS DIRTY? => (FULL BITS)&(USLOSS_MMU_DIRTY)
     o (D+U)&(D+U) == (10)&(10) == (10) == USLOSS_MMU_DIRTY
     o (D+R)&(D+U) == (11)&(10) == (10) == USLOSS_MMU_DIRTY

 > ACTION: SET UNREFERENCED => (FULL BITS)&(USLOSS_MMU_DIRTY)
     o (C+R)&(D+U) == (01)&(10) == (00) == (C+U)
     o (D+R)&(D+U) == (11)&(10) == (10) == (D+U)

 > ACTION: SET CLEAN => (FULL BITS)&(USLOSS_MMU_REF)
     o (D+U)&(C+R) == (10)&(01) == (00) == (C+U)
     o (D+R)&(C+R) == (11)&(01) == (01) == (C+R)





+-----------------------------------------------------------------------

TTD Before turnin when everything works:
 > XTODO: Might not need definition of vmRegion anymore
 > XTODO: Skip simpleTest7 - does not work
 > XTODO: Do valgrind test to make sure malloc works!
 > XTODO: Look for the other TODO items

+=====================================================================*/
#include <assert.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <phase5.h>
#include <usyscall.h>
#include <libuser.h>
#include <libuser.c>
#include <vm.h>
#include <string.h>
// Will probably need these two at some point...
#include <stdlib.h>
#include <stdio.h>
#include <providedPrototypes.h>

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
| > void *vmRegion:  Defined local to resolve the extern vmRegion issue 
|                    via the test cases. Dr. Homer advised us to define
|                    vmRegion in this file, and redefined it as such in 
|                    the new skeleton.c file, so that's what we'll do.
|
| > int pagerID:     The maximum number of Pager Daemons is defined by
|                    USLOSS, ergo we can safely declare an array of such
|                    size with the guarantee thereof. NUM_PAGERS defines
|                    the number of Pager Daemons as passed in by call of
|                    VmInit, and will be used to indicate how many Pager
|                    Daemons the system uses, which can be <MAXPAGERS
|


+---------------------------------------------------------------------*/

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
int            SECTOR_SIZE;        // Used for vmInitReal
int            TRACK_SIZE;         // Used for vmInitReal
int            DISK_SIZE;          // Used for vmInitReal
void           *VM_REGION;         // Where vmRegion starts

// Misc. / 'In a Rush' Added ___________________________________________
DTE *          DiskTable;          // Current state of Disk 1
FTE *          FrameTable;
int            VM_INIT;            // Informs if VmInit has completed
int            clockHand = 0;
int            PAGER_QUIT = -2;    // signals a pager to quit (XTODO: Should put in phase5.h)


// Homemade Boolean ____________________________________________________
#define T 1
#define F 0

// TEMP MUTEX
int FT_ACCESS;
int DT_ACCESS;




/*----------------------------------------------------------------------
|>>> Function Declarations
+-----------------------------------------------------------------------
| Implementation Notes:
|
| > FaultHandler is installed into the USLOSS_IntVec via VmInitReal
| > vmInit is installed into the Syscall Vector via start4
| > vmDestroy is installed into the Syscall Vector via start4
+---------------------------------------------------------------------*/
static void FaultHandler(int  type, void *arg);
static void vmInit(systemArgs *sysargsPtr);
static void vmDestroy(systemArgs *sysargsPtr);
static int  Pager(char *buf);
void        *vmInitReal(int m, int p, int f, int pd);
void        vmDestroyReal(void); 
int         getFrame();


/*----------------------------------------------------------------------
|>>> Function start4
+-----------------------------------------------------------------------
| Purpose: User Process that initializes VM system call handlers, the 
|          Phase 5 Process Table, gets information about the Disk, and
|          spawns start5 'Entry Point' process (as used for testing)
| Parms:   char *arg - Process Argument per normal
| Effects: Systemwide - See 'Purpose' above
| Returns: Technically nothing, as it calls Terminate
+---------------------------------------------------------------------*/
int start4(char *arg) {
  // Used for Spawn and Wait calls per usual
  int pid; int result; int status;

  // Initialize/indicate that VmInit has not completed
  VM_INIT = 0;

  // (Via Homer) To get user-process access to mailbox functions
  systemCallVec[SYS_MBOXCREATE]      = mbox_create;
  systemCallVec[SYS_MBOXRELEASE]     = mbox_release;
  systemCallVec[SYS_MBOXSEND]        = mbox_send;
  systemCallVec[SYS_MBOXRECEIVE]     = mbox_receive;
  systemCallVec[SYS_MBOXCONDSEND]    = mbox_condsend;
  systemCallVec[SYS_MBOXCONDRECEIVE] = mbox_condreceive;

  // Install Syscall Vector Handlers for vmInit/vmDestroy
  systemCallVec[SYS_VMINIT]          = vmInit;
  systemCallVec[SYS_VMDESTROY]       = vmDestroy;

  // Initialize Process Table per normal (with values as suggested by Dr. Homer)
  for(int i = 0; i<MAXPROC; i++){
    processes[i].numPages  = -1;
    processes[i].pageTable = NULL;
  }

  // Get information about the Disk
  DiskSize(1, &SECTOR_SIZE, &TRACK_SIZE, &DISK_SIZE);

  // Spawn start5, then wait for it to terminate (compressing code to save lines)
  result = Spawn("Start5", start5, NULL, 8*USLOSS_MIN_STACK, PAGER_PRIORITY, &pid);
  if(result != 0){USLOSS_Console("start4(): Error spawning start5\n");Terminate(1);}
  result = Wait(&pid, &status);
  if(result != 0){USLOSS_Console("start4(): Error waiting for start5\n");Terminate(1);}
  Terminate(0); // The non-erroneous call of Terminate
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
  int result = (int) (long)vmInitReal(mappings, pages, frames, pagers);

  // Result should hold address of first byte in VM region
  args->arg1 = (void *)((long) result);
  args->arg4 = 0;
  return;
} // Ends Syscall Handler vmInit


/*----------------------------------------------------------------------
|>>> Function vmInitReal
+-----------------------------------------------------------------------
| Purpose: Called by vmInit, this function performs several operations
|          encompassing the initialization of the VM Subsystem:
|
|          1) Configure and initialize the MMU via a USLOSS_MmuInit call
|          2) Install the FaultHandler which handles MMU Interrupts
|          3) Pre-Initialize the Page Tables with values (Via Dr. Homer)
|          4) Initialize the Frame Table (XTODO-TBD)
|          5) Initialize the faults FaultMsg Data Structure Array
|          6) Define pagerBox - the Pager Wait/Service Mailbox
|          7) Initialize (fork) the Pager Daemons
|          8) Initialize the vmStats VM Subsystem Statistics
|          9) Get and return vmRegion address via USLOSS_MmuRegion call
|         
| Parms:   > int mappings = How many mappings for this VM System?
|          > int pages    = How many pages for this VM System?
|          > int frames   = How many frames for this VM System?
|          > int pagers   = How many Pager Daemons for this VM System?
|
| Effects: All aforementioned data structures and variables are either
|          assigned and/or initialized. MMU Subsystem is considered
|          initialized on the return of this function, ergo VM_INIT is
|          assigned to '1' before this function returns.
|
| Returns: The address of the VM Region i.e. vmRegion
+-----------------------------------------------------------------------
| Implementation Notes: 
| 
| > Page Table attribute initialization values (Via Homer):
|    o aProcess.state     = UNUSED;
|    o aProcess.frame     = -1 (becaue memory can have a 0th frame)
|    o aProcess.diskBlock = -1 (because disk can have a 0th block)
|
| > Initializing FaultMsg Private Mailboxes: We can assign a 'permanent' 
|   mailbox versus 'swapping mailboxes' with each P1_Fork call on the 
|   guarantee that the Pager will always do a send on a fault member's
|   replyBox, thus resetting its state before said process can ever 
|   unblock and quit, else an error has occured outside Phase 5's scope.
|
|
+---------------------------------------------------------------------*/
void * vmInitReal(int mappings, int pages, int frames, int pagers){
  CheckMode(); // Kernel Mode Check

  int status; // Used to get USLOSS_Mmu... return values
  int dummy;  // Used to pass addresses into function calls
  int i = 0;  // Used for loops (C-99 habit I've developed!)

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
  for(int i = 0; i < MAXPROC; i++){

    // We just thought this might actually refer to how many pages the process is using
    processes[i].numPages = 0;

    // Create as many PTEs as there are numpages
    processes[i].pageTable = (PTE*) malloc(NUM_PAGES * sizeof(PTE));
    // Initialize the Page Table
    for (int j = 0; j < NUM_PAGES; j++) {
      processes[i].pageTable[j].state = UNUSED;
      processes[i].pageTable[j].frame = -1;
      processes[i].pageTable[j].diskBlock = -1;
    } // Ends malloc and initialization of Page Table
  } // Ends initialization of Process Table

  //>>> Phase 5 - Initialize Frame Table (TBD)
  FrameTable = (FTE*) malloc(NUM_FRAMES * sizeof(FTE));
  for (int i = 0; i < NUM_FRAMES; i++) {
    FrameTable[i].owner        = -1;
    FrameTable[i].isUsed       = UNUSED; 
    FrameTable[i].page         = -1;
    FrameTable[i].isDirty      = CLEAN;
    FrameTable[i].isReferenced = UNREF;
  } // Ends malloc and initialization of Frame Table

  //>>> Phase 6 - Initialize FaultMsg structs, create FaultMsg Mailboxes
  for (int i = 0; i < MAXPROC; i++){
    faults[i].pid = -1;
    faults[i].addr  = NULL;
    faults[i].replyMbox = MboxCreate(0, 0);
  }

  //>>> Phase 7 - Create the Pager Daemon 'Requests Mailbox / Queue'
  pagerBox = MboxCreate(pagers, MAX_MESSAGE);

  //>>> Phase 8 - Fork the Pager Daemons, store pager pids in global variables.
  char buf[10];
  for (i = 0; i < NUM_PAGERS; i++) {
    sprintf(buf, "%d", i); // Why not tell the pager which one it is?
    pagerID[i] = fork1("PagerDaemon", Pager, buf, USLOSS_MIN_STACK, PAGER_PRIORITY);
  } // Borrowed from Dr. Homer's Phase 4 skeleton code


  //>>> Phase 9 - Allocate and initialize DiskTable
  DiskTable = (DTE*) malloc(DISK_SIZE * sizeof(DTE));
  for(int i = 0; i < DISK_SIZE; i++){
    DiskTable[i].pid  = -1; 
    DiskTable[i].page = -1;    
  }
 
  //>>> Phase 10 - Zero out, then initialize vmStats structure
  memset((char *) &vmStats, 0, sizeof(VmStats));
  vmStats.pages          = pages;
  vmStats.frames         = frames;
  vmStats.freeFrames     = frames;
  vmStats.switches       = 0;
  vmStats.faults         = 0;
  vmStats.new            = 0;
  vmStats.pageIns        = 0;
  vmStats.pageOuts       = 0;
  vmStats.replaced       = 0;
  vmStats.diskBlocks     = 64;
  vmStats.freeDiskBlocks = 64;

  //>>> Phase 11 - Assign VM_INIT to 1, Return address of VM Region
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
|
| Notes:   Page Faults will occur whenever the vmRegion is 'touched'.
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
  Homer's Algo:
   > Wait for fault to occur (receive from mailbox) 
   > Look for free frame 
   > If there isn't one 
       o then use clock algorithm to replace a page (perhaps write to disk) 
   > Load page into frame from disk, if necessary 
   > Unblock waiting (faulting) process 


 * Results:
 * None.
 *
 * Side effects:
 * None.




 *----------------------------------------------------------------------
 */
static int Pager(char *buf){
  int getter;
  int targetFrame;
  int targetPage;
  int blockToUse;
  int CLIENT;
  int OLDGUY;
  int OLDGUYpage;
  int gotOne; // indicates Pager found a frame
  int WL_SEC; // AKA *W*hile *L*oop *Sec*urity - covers my ass in case of buggy while loop

  char rec[1];

  int opIter = 1;

  //enable interrupts
  USLOSS_PsrSet(USLOSS_PsrGet() | USLOSS_PSR_CURRENT_INT);  

  while(1) {




    //##################################################################
    //>>> PHASE 1 - SET UP THE REQUEST, GET THE MESSAGE, QUIT IF KILLED
    //##################################################################    

    // Stuff we should reassign each new request operation
    gotOne = F;
    getter = 0;
    WL_SEC = 0;

    // Do receive on pagerBox - when we unblock, we get to work...
    MboxReceive(pagerBox, rec, MAX_MESSAGE);
    // Get the int value of the message
    CLIENT = atoi(rec);


    // If CLIENT==PAGER_QUIT: it's from VmDestroyReal, so do it!
    if(CLIENT == PAGER_QUIT){quit(1);}

    //##################################################################
    //>>> PHASE 2 - GET TARGET FRAME A.K.A. CLOCK ALGO + 'HOMER'S TWIST'
    //##################################################################

    // Case 1 - Homer's Twist: FrameTable reports frame unused: use it!
    for(int i = 0; i<NUM_FRAMES ; i++){
      if (FrameTable[i].isUsed == UNUSED){
        targetFrame = i;
        gotOne = T;
        break;
      }
    } // Ends Scenario 1 (Homer's Twist)

    // Case 2 and 3 - Clock Algorithm: Get an unreferenced frame
    while(gotOne==F && WL_SEC < 1000){

      // Get MMU Ref Bits for clockHand frame
      USLOSS_MmuGetAccess(clockHand, &getter);

      // If the bit is referenced
      if( (getter&USLOSS_MMU_REF) == USLOSS_MMU_REF){
        
        // Mask Dirty Bit ID over curent Bit to Unreference
        getter = getter&USLOSS_MMU_DIRTY;

        // And send the modified bits back to the MMU
        USLOSS_MmuSetAccess(clockHand,getter);
      } // Ends modifying a referenced frame to become unreferenced

      // We've discovered an unreferenced frame - we're done!
      else{
        // But don't forget to tell gotOne this!
        gotOne = T;
        targetFrame = clockHand;
      }

      if(clockHand == NUM_FRAMES-1){clockHand = 0;}
      else{clockHand++;}

      WL_SEC++; // For Security Purposes
    }if(WL_SEC==100){USLOSS_Console("\n\n>>>Alert! While Loop Security Fuse!\n\n");}

    //##################################################################
    //>>> GET CURRENT OWNER OF FRAME AND PAGE HE MAPPED HIS FRAME TO
    //##################################################################
    OLDGUY = FrameTable[targetFrame].owner;
    OLDGUYpage = FrameTable[targetFrame].page;

    if(OLDGUY>0){
      processes[OLDGUY].pageTable[OLDGUYpage].state = FROZEN;
    }




    //##################################################################
    //>>> PHASE 3 - GET TARGET PAGE
    //##################################################################
    targetPage = (int) (long)((faults[CLIENT].addr)-(vmRegion)) / USLOSS_MmuPageSize();




    // increment new if this PTE has never been used before
    if(processes[CLIENT].pageTable[targetPage].state == UNUSED) {
      vmStats.new++;
    }

    //##################################################################
    //>>> PHASE 4 - HANDLE THE DIRTY FRAME SCENARIO
    //##################################################################

    // Get the dirty bit, will determine if we need to write frame to disk
    USLOSS_MmuGetAccess(targetFrame, &getter);

    if( (getter&USLOSS_MMU_DIRTY) == USLOSS_MMU_DIRTY){

      if(processes[OLDGUY].pageTable[OLDGUYpage].diskBlock == -1){USLOSS_Console("Hi!\n");}



      USLOSS_MmuUnmap(TAG, OLDGUYpage);
      // FIRST check if you have to write to disk - or is frame full of 0's?

      // Map Pager to the frame, else MMU Interrupt will happen
      USLOSS_MmuMap(TAG, OLDGUYpage, targetFrame, USLOSS_MMU_PROT_RW);


      // only write to disk if first char isn't 0
        // Create the disk buffer
        char buf [USLOSS_MmuPageSize()];

        // Copy the page to the buffer (AND LOL Was going crazy until I saw Piazza Post # 428)
        ///memcpy(buf, vmRegion, USLOSS_MmuPageSize());




        memcpy(buf, vmRegion+(OLDGUYpage*USLOSS_MmuPageSize()), USLOSS_MmuPageSize());


        // Find an available disk block to use
        for (blockToUse = 0; blockToUse < DISK_SIZE; blockToUse++){
          if(DiskTable[blockToUse].pid == -1){
            break;
          }
        }

        DiskTable[blockToUse].pid = OLDGUY;
        DiskTable[blockToUse].page = OLDGUYpage;
        // Write to the disk. We'll use 8 sectors i.e 1 track blocks for now
        diskWriteReal(1, blockToUse, 0,8, buf);
        
        // Update the state for the OLDGUY whose frame is now on a disk
        processes[OLDGUY].pageTable[OLDGUYpage].diskBlock = blockToUse;
        processes[OLDGUY].pageTable[OLDGUYpage].state     = ONDISK;
        processes[OLDGUY].pageTable[OLDGUYpage].frame     = -1;
        // Update vmStats
        vmStats.pageOuts++;
        vmStats.freeDiskBlocks--;

      // We're done with the frame - unmap ourselves
      USLOSS_MmuUnmap(TAG, OLDGUYpage);

    } // Ends Frame-To-Disk Operation

    //USLOSS_Console("client = %d and oldguy = %d\n\n", CLIENT, OLDGUY);

    //##################################################################
    //>>> PHASE 4 - HANDLE THE PAGE SCENARIO
    //##################################################################


    USLOSS_MmuMap(TAG, targetPage, targetFrame, USLOSS_MMU_PROT_RW);

    if( processes[CLIENT].pageTable[targetPage].state == ONDISK){

      // Get the block to use from the CLIENT
      blockToUse = processes[CLIENT].pageTable[targetPage].diskBlock;

      char buf [USLOSS_MmuPageSize()];
      
      // Read the block from the disk
      diskReadReal(1, blockToUse, 0, 8, buf);




      // And copy into the frame
      memcpy(vmRegion+(targetPage*USLOSS_MmuPageSize()), buf, USLOSS_MmuPageSize());



  
      DiskTable[blockToUse].pid = -1;


      vmStats.freeDiskBlocks++;
      vmStats.pageIns++;
    }

    else{
    
      // vmStats.new++;
      // Zero out the memory
      memset(vmRegion+(targetPage*USLOSS_MmuPageSize()), 0, USLOSS_MmuPageSize());
    }




    // RESET MMU ACCESS BITS TO ZERO
    USLOSS_MmuSetAccess(targetFrame,0);


    // PAGER MUST UNMAP SO PROCESS CAN IN SWITCH
    USLOSS_MmuUnmap(TAG, targetPage);

    //##################################################################
    //>>> PHASE 5 - UPDATE THE CLIENT PAGE TABLE AND FRAME TABLE
    //##################################################################

    // Update Client process page table
    processes[CLIENT%MAXPROC].pageTable[targetPage].state = INCORE;
    processes[CLIENT%MAXPROC].pageTable[targetPage].frame = targetFrame;

    //XTODO - NEEDS MUTEX

    USLOSS_MmuGetAccess(targetFrame, &getter);

    if( (getter&USLOSS_MMU_DIRTY) == (USLOSS_MMU_DIRTY)) {FrameTable[targetFrame].isDirty=DIRTY;}
    else                                                 {FrameTable[targetFrame].isDirty=CLEAN;}

    if( (getter&USLOSS_MMU_REF) == (USLOSS_MMU_REF))     {FrameTable[targetFrame].isReferenced=ISREF;}
    else                                                 {FrameTable[targetFrame].isReferenced=UNREF;}

    FrameTable[targetFrame].page         = targetPage;
    FrameTable[targetFrame].owner        = CLIENT;
    FrameTable[targetFrame].isUsed       = ISUSED;

    //##################################################################
    //>>> DONE - WAKE UP CLIENT PROCESS
    //##################################################################
    MboxSend(faults[CLIENT%MAXPROC].replyMbox, NULL, 0);       
  } // Ends Pager Loop
  return 0; // So GCC won't complain
} // Ends Process Pager


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
|             will be a PID of -2 (negative one), though we may change
|             this in future development WLOG to the main intention.
|
|          3) Sends the aforementioned messages to the pager processes
|             one by one in a loop. Following each message send will be
|             a call of join - this will block vmDestroyReal until the
|             corresponding Pager process terminates and quits, which
|             guarantees via the  Phase 1 layer that all Pager processes
|             will have quit when vmDestroyReal returns.
|
|          4) Frees malloced Data Structures. At the present time, this
|             may include each Process' Page Table, th Frame Table, the
|             Disk Table, and/or other structures as they come up.
|

>> INCLUDE NOTE OF FACT THAT WERE FOLLOWING HOMER'S SKELETON CODE ALGO

| Parms:   None
| Effects: Systemwide - The MMU is turned off
+---------------------------------------------------------------------*/
void vmDestroyReal(void){
  CheckMode(); // Are we in Kernel Mode?

  VM_INIT = 0;      // We no longer have a MMU/VM Subsystem

  int status;   // Used to make join call happy because it needs int pointer
  char mess[1]; // Used with sprintf to send integer value passed into message

  // Encode int value -1 into special message
  sprintf(mess, "%d", PAGER_QUIT);

  // Kill the Pagers
  for (int i = 0; i < NUM_PAGERS; i++){
    MboxSend(pagerBox, mess, sizeof(int));
    join(&status);
  }

  PrintStats(); // Might not be what Homer did

  // Free Stuff...
  free(FrameTable);

  //free(DiskTable);


  



  for(int i = 0; i < MAXPROC; i++){
    free(processes[i].pageTable); 
  } // Ends initialization of Process Table

  USLOSS_MmuDone(); // Make it official...call USLOSS_MmuDone
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

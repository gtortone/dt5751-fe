/*****************************************************************************/
/**
\file feoV1725.cxx

\mainpage

\section contents Contents

Standard Midas Frontend for Optical access to the CAEN v1725 using the A3818 CONET2 driver

\subsection organization File organization

- Compile time parameters setting
- MIDAS global variable defintion
- MIDAS function declaration
- Equipment variables
- functions

\subsection routines Callback routines for system transitions

These routines are called whenever a system transition like start/
stop of a run occurs. The routines are called on the following
occations:

- frontend_init:  When the frontend program is started. This routine
                should initialize the hardware.

- frontend_exit:  When the frontend program is shut down. Can be used
                to releas any locked resources like memory, commu-
                nications ports etc.

- begin_of_run:   When a new run is started. Clear scalers, open
                rungates, etc.

- end_of_run:     Called on a request to stop a run. Can send
                end-of-run event and close run gates.

- pause_run:      When a run is paused. Should disable trigger events.

- resume_run:     When a run is resumed. Should enable trigger events.

\subsection notes Notes about this frontend

This frontend has been designed so that it should compile and work
by default without actual actual access to v1725 hardware. We have turned
off portions of code which make use of the driver to the actual hardware.
Where data acquisition should be performed, we generate random data instead
(see v1725CONET2::ReadEvent()). See usage below to use real hardware.



The code to use real hardware assumes this setup:
- 1 A3818 PCI-e board per PC to receive optical connections
- NBLINKSPERA3818 links per A3818 board
- NBLINKSPERFE optical links controlled by each frontend
- NB1725PERLINK v1725 modules per optical link (daisy chained)
- NBV1725TOTAL v1725 modules in total
- The event builder mechanism is used

\subsection usage Usage


\subsubsection real Real hardware
Adjust NBLINKSPERA3818, NBLINKSPERFE, NB1725PERLINK and NBV1725TOTAL below according
to your setup.  NBV1725TOTAL / (NBLINKSPERFE * NB1725PERLINK) frontends
must be started in total. When a frontend is started, it must be assigned an index
number:

    ./frontend -i 0

If no index number is supplied, it is assumed that only 1 frontend is used to control
all boards on all links on that computer.

For example, consider the following setup:

    NBLINKSPERA3818    4     // Number of optical links used per A3818
    NBLINKSPERFE       1     // Number of optical links controlled by each frontend
    NB1725PERLINK      2     // Number of daisy-chained v1725s per optical link
    NBV1725TOTAL       32    // Number of v1725 boards in total

We will need 32/(2*2) = 8 frontends (8 indexes; from 0 to 7).  Each frontend
controls 2*2 = 4 v1725 boards.  Compile and run:

    ./feoV1725.exe



 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sched.h>
#include <sys/resource.h>

#include <fstream>
#include <string>
#include <vector>
#include <memory>

#include "midas.h"
#include "v1725CONET2.hxx"

#include <zmq.h>
//#include <zmq.hpp>

// __________________________________________________________________
// --- General feov1725 parameters


#ifndef NBLINKSPERA3818
#define NBLINKSPERA3818   4   //!< Number of optical links used per A3818
#define NBLINKSPERFE      1   //!< Number of optical links controlled by each frontend
#define NB1725PERLINK     1   //!< Number of daisy-chained v1725s per optical link
#define NBV1725TOTAL      4  //!< Number of v1725 boards in total
#define NBCORES           8   //!< Number of cpu cores, for process/thread locking
#endif


#define SLEEP_TIME_BETWEEN_CONNECTS 50 // in milliseconds

#define  EQ_EVID   1                //!< Event ID
#define  EQ_TRGMSK 0                //!< Trigger mask (overwritten in code)
                                    //!< based on feIndex (see _init) 
#define  FE_NAME   "feov1725MTI"       //!< Frontend name

#define UNUSED(x) ((void)(x)) //!< Suppress compiler warnings
const bool SYNCEVENTS_DEBUG = true;

// __________________________________________________________________
// --- MIDAS global variables
extern HNDLE hDB;   //!< main ODB handle
//extern BOOL debug;  //!< debug printouts

/* make frontend functions callable from the C framework */
#ifdef __cplusplus
extern "C" {
#endif

/*-- Globals -------------------------------------------------------*/

//! The frontend name (client name) as seen by other MIDAS clients
char *frontend_name = (char*)FE_NAME;
//! The frontend file name, don't change it
char *frontend_file_name = (char*)__FILE__;
//! frontend_loop is called periodically if this variable is TRUE
BOOL frontend_call_loop = FALSE;
//! a frontend status page is displayed with this frequency in ms
INT display_period = 000;
//! maximum event size produced by this frontend
INT max_event_size = 32 * 222800;
//! maximum event size for fragmented events (EQ_FRAGMENTED)
INT max_event_size_frag = 5 * 1024 * 1024;
//! buffer size to hold events
INT event_buffer_size = 30 * max_event_size + 10000;

bool runInProgress = false; //!< run is in progress
uint32_t timestamp_offset[NBLINKSPERFE*NB1725PERLINK]; //!< trigger time stamp offsets

// __________________________________________________________________
/*-- MIDAS Function declarations -----------------------------------------*/
INT frontend_init();
INT frontend_exit();
INT begin_of_run(INT run_number, char *error);
INT end_of_run(INT run_number, char *error);
INT pause_run(INT run_number, char *error);
INT resume_run(INT run_number, char *error);
INT frontend_loop();
extern void interrupt_routine(void);  //!< Interrupt Service Routine

INT read_event_from_ring_bufs(char *pevent, INT off);
INT read_buffer_level(char *pevent, INT off);
INT read_temperature(char *pevent, INT off);
void * link_thread(void *);
void *subscriber;

// __________________________________________________________________
/*-- Equipment list ------------------------------------------------*/
#undef USE_INT
//! Main structure for midas equipment
EQUIPMENT equipment[] =
{
    {
        "V1725_Data%02d",           /* equipment name */
        {
            EQ_EVID, EQ_TRGMSK,     /* event ID, trigger mask */
#if USE_SYSTEM_BUFFER
            "SYSTEM",               /* write events to system buffer */
#else
            "BUF%02d",              /* make different frontends (indexes) write to different buffers */
#endif //USE_SYSTEM_BUFFER
#ifdef USE_INT
            EQ_INTERRUPT,           /* equipment type */
#else
            EQ_POLLED | EQ_EB,      /* equipment type */
#endif //USE_INT

            LAM_SOURCE(0, 0x0),     /* event source crate 0, all stations */
            "MIDAS",                /* format */
            TRUE,                   /* enabled */
            RO_RUNNING,             /* read only when running */
            500,                    /* poll for 500ms */
            0,                      /* stop run after this event limit */
            0,                      /* number of sub events */
            0,                      /* don't log history */
            "", "", ""
        },
        read_event_from_ring_bufs,  /* readout routine */
    },

    {
        "V1725_BufLvl%02d",             /* equipment name */
        {
            100, 0x1000,            /* event ID, corrected with feIndex, trigger mask */
            "SYSTEM",               /* event buffer */
            EQ_PERIODIC,            /* equipment type */
            0,                      /* event source */
            "MIDAS",                /* format */
            TRUE,                   /* enabled */
            RO_RUNNING | RO_TRANSITIONS |   /* read when running and on transitions */
            RO_ODB,                 /* and update ODB */
            1000,                   /* read every 1 sec */
            0,                      /* stop run after this event limit */
            0,                      /* number of sub events */
            1,                      /* log history */
            "", "", ""
        },
        read_buffer_level,       /* readout routine */
    },
    {
      "V1725_Temp%02d",             /* equipment name */
      {
	100, 0x1000,            /* event ID, corrected with feIndex, trigger mask */
	"SYSTEM",               /* event buffer */
	EQ_PERIODIC,            /* equipment type */
	0,                      /* event source */
	"MIDAS",                /* format */
	TRUE,                   /* enabled */
	RO_ALWAYS |    /* read when running and on transitions */
	RO_ODB,                 /* and update ODB */
	1000,                   /* read every 1 sec */
	0,                      /* stop run after this event limit */
	0,                      /* number of sub events */
	1,                      /* log history */
	"", "", ""
      },
      read_temperature,       /* readout routine */
    },
    {""}
};

#ifdef __cplusplus
}
#endif

std::vector<v1725CONET2> ov1725; //!< objects for the v1725 modules controlled by this frontend
std::vector<v1725CONET2>::iterator itv1725;  //!< Main thread iterator
std::vector<v1725CONET2>::iterator itv1725_thread[NBLINKSPERFE];  //!< Link threads iterators

pthread_t tid[NBLINKSPERFE];                            //!< Thread ID
int thread_retval[NBLINKSPERFE] = {0};                  //!< Thread return value
int thread_link[NBLINKSPERFE];                          //!< Link number associated with each thread

/********************************************************************/
/********************************************************************/
/********************************************************************/
/**
 * \brief   Sequencer callback info
 *
 * Function which gets called when record is updated
 *
 * \param   [in]  h main ODB handle
 * \param   [in]  hseq Handle for record that was updated
 * \param   [in]  info Record descriptor additional info
 */
void seq_callback(INT h, INT hseq, void *info){
  KEY key;

  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (hseq == itv1725->GetSettingsHandle()){
      db_get_key(h, hseq, &key);
      itv1725->SetSettingsTouched(true);
      cm_msg(MINFO, "seq_callback", "Settings %s touched. Changes will take effect at start of next run.", key.name);
    }
  }
}

//
//-------------------------------------------------------------------
/**
 * \brief   Frontend initialization
 *
 * Runs once at application startup.  We initialize the hardware and optical
 * interfaces and set the equipment status in ODB.  We also lock the frontend
 *  to once physical cpu core.
 *
 * \return  Midas status code
 */
INT frontend_init(){

  int feIndex = get_frontend_index();
  // If feIndex == -1, then just treat it as the first frontend; ie, set to 0.
  if(feIndex < 0){
		cm_msg(MERROR,"Init", "Must specify the frontend index (ie use -i X command line option)");
		return FE_ERR_HW;
	}

  
  set_equipment_status(equipment[0].name, "Initializing...", "#FFFF00");
  printf("<<< Begin of Init\n");

  {
    // Reset the PLL lock loss flag in ODB
    char Path[255];
    sprintf(Path,"/DS Alarm/PLL Loss FE0%d",get_frontend_index());
    INT dummy;
    int size=sizeof(INT);
    db_get_value(hDB, 0, Path, &(dummy), &size, TID_INT, true);
    dummy=-1;
    db_set_value(hDB, 0, Path, &(dummy), sizeof(INT), 1, TID_INT);
  }
  
  {
    // Correct the Trigger mask based on the frontend index, update ODB
    // Used for sorting the threads, and for logger filtering
    char sEpath[64];
    equipment[0].info.trigger_mask = (2<<feIndex);
    snprintf(sEpath, sizeof(sEpath), "Equipment/%s/Common/Trigger mask", equipment[0].name);
    db_set_value(hDB, 0, sEpath, &(equipment[0].info.trigger_mask), sizeof(WORD), 1, TID_WORD);
    
    // Correct the Buffer level equipment Event ID based on the frontend index, update ODB
    equipment[1].info.event_id += feIndex;
    snprintf(sEpath, sizeof(sEpath), "Equipment/%s/Common/Event ID", equipment[1].name);
    db_set_value(hDB, 0, sEpath, &(equipment[1].info.event_id), sizeof(WORD), 1, TID_WORD);
  }

  // --- Suppress watchdog for PICe for now  ; what is this???
  cm_set_watchdog_params(FALSE, 0);

  int nExpected = 0; //Number of v1725 boards we expect to activate
  int nActive = 0;   //Number of v1725 boards activated at the end of frontend_init
  std::vector<std::pair<int,int> > errBoards;  //v1725 boards which we couldn't connect to
  
  nExpected = NB1725PERLINK*NBLINKSPERFE;
  
  if((NBV1725TOTAL % (NB1725PERLINK*NBLINKSPERFE)) != 0){
    printf("Incorrect setup: the number of boards controlled by each frontend"
           " is not a fraction of the total number of boards. %i %i %i\n",NBV1725TOTAL,NB1725PERLINK,NBLINKSPERFE);
  }
  
  int maxIndex = (NBV1725TOTAL/NB1725PERLINK)/NBLINKSPERFE - 1;
  if(feIndex < 0 || feIndex > maxIndex){
    printf("Front end index (%i) must be between 0 and %d\n", feIndex, maxIndex);
    exit(FE_ERR_HW);
  }
  
  int firstLink = (feIndex % (NBLINKSPERA3818 / NBLINKSPERFE)) * NBLINKSPERFE;
  int lastLink = firstLink + NBLINKSPERFE - 1;
  for (int iLink=firstLink; iLink <= lastLink; iLink++) {
    for (int iBoard=0; iBoard < NB1725PERLINK; iBoard++) {
      printf("==== feIndex:%d, Link:%d, Board:%d ====\n", feIndex, iLink, iBoard);
      
      // Compose unique module ID
      int moduleID = feIndex*NBLINKSPERFE*NB1725PERLINK + (iLink-firstLink)*NB1725PERLINK + iBoard;
      
      // Create module objects
      ov1725.emplace_back(feIndex, iLink, iBoard, moduleID, hDB);
      ov1725.back().SetVerbosity(0);
      
      // Open Optical interface
      switch(ov1725.back().Connect()){
      case v1725CONET2::ConnectSuccess:
        nActive++;
        break;
      case v1725CONET2::ConnectErrorCaenComm:
      case v1725CONET2::ConnectErrorTimeout:
        errBoards.push_back(std::pair<int,int>(iLink,iBoard));
        break;
      case v1725CONET2::ConnectErrorAlreadyConnected:
        //do nothing
        break;
      default:
        //Can't happen
        break;
      }
      
      if(!((iLink == lastLink) && (iBoard == (NB1725PERLINK-1)))){
        printf("Sleeping for %d milliseconds before next board\n", SLEEP_TIME_BETWEEN_CONNECTS);
        ss_sleep(SLEEP_TIME_BETWEEN_CONNECTS);
      }
    }
  }

  /* This must be done _after_ filling the vector because we pass a pointer to config
   * to db_open_record.  The location of the object in memory must not change after
   * doing that. */
  int nInitOk = 0;
  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (! itv1725->IsConnected()) continue;   // Skip unconnected board
    
    // Setup ODB record (create if necessary)
    itv1725->SetBoardRecord(hDB,seq_callback);
    // Set history ODB record (create if necessary)
    itv1725->SetHistoryRecord(hDB,seq_callback);

    int status = itv1725->InitializeForAcq();
    nInitOk += status;
  }
  
  // Abort if board status not Ok.
  if (nInitOk != 0) return FE_ERR_HW;
  
  printf(">>> End of Init. %d active v1725. Expected %d\n\n", nActive, nExpected);

  if(nActive == nExpected){
    set_equipment_status(equipment[0].name, "Initialized", "#00ff00");
  }
  else{
    return FE_ERR_HW;
  }

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(0, &mask);  //Main thread to core 0
  if( sched_setaffinity(0, sizeof(mask), &mask) < 0 ){
    printf("ERROR setting cpu affinity for main thread: %s\n", strerror(errno));
  }

  //-begin - ZMQ----------------------------------------------------------
#if (0)
  zmq::context_t context(1);
  zmq::socket_t socket (context, ZMQ_SUB);
  std::cout << "This Subscriber connecting to ChronoBox Publisher... ";
  socket.bind ("tcp://*:5555");
  std::cout << " Done" << std::endl;
#endif
#if (1)
  //  Socket to talk to clients
  void *context = zmq_ctx_new ();
  subscriber = zmq_socket (context, ZMQ_SUB);
  int rc = zmq_bind (subscriber, "tcp://*:5555");
  // Without message
  zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
  printf (" This subscriber is connecting to the ChronoBox Publisher context: %p *subscriber: %p rc:%d \n", context, subscriber, rc);
#endif
  //-end - ZMQ----------------------------------------------------------

  return SUCCESS;
}

//
//----------------------------------------------------------------------------
/**
 * \brief   Frontend exit
 *
 * Runs at frontend shutdown.  Disconnect hardware and set equipment status in ODB
 *
 * \return  Midas status code
 */
INT frontend_exit(){

  set_equipment_status(equipment[0].name, "Exiting...", "#FFFF00");

  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (itv1725->IsConnected()){
      itv1725->Disconnect();
    }
  }
  set_equipment_status(equipment[0].name, "Exited", "#00ff00");
  return SUCCESS;
}

//
//----------------------------------------------------------------------------
/**
 * \brief   Begin of Run
 *
 * Called every run start transition.  Set equipment status in ODB,
 * start acquisition on the modules.
 *
 * \param   [in]  run_number Number of the run being started
 * \param   [out] error Can be used to write a message string to midas.log
 */
INT begin_of_run(INT run_number, char *error)
{
  set_equipment_status(equipment[0].name, "Starting run...", "#FFFF00");
  cm_msg(MINFO,"BOR", "Start of begin_of_run");
  printf("<<< Start of begin_of_run\n");
  
  int rb_handle;
  int status;
  
  runInProgress = true;
  {
    // Reset the PLL lock loss flag in ODB
    char Path[255];
    sprintf(Path,"/DS Alarm/PLL Loss FE0%d",get_frontend_index());
    INT dummy = -1;
    db_set_value(hDB, 0, Path, &(dummy), sizeof(INT), 1, TID_INT);
  }

  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (! itv1725->IsConnected()) continue;   // Skip unconnected board
    DWORD vmeAcq, vmeStat;
    itv1725->ReadReg(V1725_ACQUISITION_STATUS, &vmeAcq);//Test the PLL lock once (it may have happened earlier)
    if ((vmeAcq & 0x80) == 0) {
      cm_msg(MERROR,"BeginOfRun","V1725 PLL loss lock Board (sometime in the past):%d (vmeAcq=0x%x)"
             ,itv1725->GetModuleID(), vmeAcq);
      // PLL loss lock reset by the VME_STATUS read!
      itv1725->ReadReg(V1725_VME_STATUS, &vmeStat);
      usleep(100);
      itv1725->ReadReg(V1725_ACQUISITION_STATUS, &vmeAcq); // Test the PLL again 
      if ((vmeAcq & 0x80) == 0) {
        cm_msg(MERROR,"BeginOfRun","V1725 PLL lock still lost Board: %d (vmeAcq=0x%x)"
               ,itv1725->GetModuleID(), vmeAcq);
        return FE_ERR_HW;
      }
    }
    
    bool go = itv1725->StartRun();
    if (go == false) return FE_ERR_HW;
    
    //Create ring buffer for board
    status = rb_create(event_buffer_size, max_event_size, &rb_handle);
    if(status == DB_SUCCESS){
      itv1725->SetRingBufferHandle(rb_handle);
    }
    else{
      cm_msg(MERROR, "feov1725:BOR", "Failed to create rb for board %d", itv1725->GetModuleID());
    }
  }
  
  // Create one thread per optical link
  for(int i=0; i<NBLINKSPERFE; ++i){
    thread_link[i] = i;
    status = pthread_create(&tid[i], NULL, &link_thread, (void*)&thread_link[i]);
    if(status){
      cm_msg(MERROR,"feov1725:BOR", "Couldn't create thread for link %d. Return code: %d", i, status);
    }
  }
  
  set_equipment_status(equipment[0].name, "Started run", "#00ff00");
  printf(">>> End of begin_of_run\n\n");
  
  return SUCCESS;
}

//
//----------------------------------------------------------------------------
void * link_thread(void * arg)
{
  int link = *(int*)arg;
  std::cout << "Started thread for link " << link << " out of " << NBCORES << " cores" << std::endl;

  //Lock each thread to a different cpu core
  cpu_set_t mask;
  CPU_ZERO(&mask);
  switch(NBCORES){
  case 1:
    //Don't do anything
    break;
  case 2:
    CPU_SET(link % 2, &mask); //TRIUMF test PC. Even boards on core 0, odd boards on core 1
    break;
  default:
    /* This will spread the threads on all cores except core 0 when the main thread resides.
     * ex 1 (SNOLAB): NBCORES=8, 4 threads:
     * threads (links) 0,1,2,3 will go on cores 1,2,3,4
     * ex 2: NBCORES 4, 4 threads:
     * threads (links) 0,1,2,3 will go on cores 1,2,3,1     */
    CPU_SET((link % (NBCORES-1)) + 1, &mask);
    printf("core setting: NBCORES:%d link:%d core %d\n", NBCORES, link,(link % (NBCORES-1)) + 1);
    break;
  }
  if( sched_setaffinity(0, sizeof(mask), &mask) < 0 ){
    printf("ERROR setting cpu affinity for thread %d: %s\n", link, strerror(errno));
  }

  void *wp;
  int status;
  int rb_handle;
  int moduleID;
  int rb_level;
  int firstBoard = link*NB1725PERLINK; //First board on this link

  while(1) {  // Indefinite until run stopped (!runInProgress)
    // This loop is running until EOR flag (runInProgress)

    // process the addressed board for that link only
    for (itv1725_thread[link] = ov1725.begin() + firstBoard;
         itv1725_thread[link] != ov1725.begin() + firstBoard + NB1725PERLINK;
         ++itv1725_thread[link]){

      // Shortcut
      rb_handle = itv1725_thread[link]->GetRingBufferHandle();
      moduleID = itv1725_thread[link]->GetModuleID();

      // Check if event in hardware to read
      if (itv1725_thread[link]->CheckEvent()){


        /* If we've reached 75% of the ring buffer space, don't read
         * the next event.  Wait until the ring buffer level goes down.
         * It is better to let the v1725 buffer fill up instead of
         * the ring buffer, as this the v1725 will generate the HW busy to the 
         * DTM.
         */
        rb_get_buffer_level(rb_handle, &rb_level);
        if(rb_level > (int)(event_buffer_size*0.75)) {
          continue;
        }
       
        // Ok to read data
        status = rb_get_wp(rb_handle, &wp, 100);
        if (status == DB_TIMEOUT) {
          cm_msg(MERROR,"link_thread", "Got wp timeout for thread %d (module %d).  Is the ring buffer full?",
                 link, moduleID);
          cm_msg(MERROR,"link_thread", "Exiting thread %d with error", link);
          thread_retval[link] = -1;
          pthread_exit((void*)&thread_retval[link]);
        }
        
        // Read data
        if(itv1725_thread[link]->ReadEvent(wp)) {
        } else {
          cm_msg(MERROR,"link_thread", "Readout routine error on thread %d (module %d)", link, moduleID);
          cm_msg(MERROR,"link_thread", "Exiting thread %d with error", link);
          thread_retval[link] = -1;
          pthread_exit((void*)&thread_retval[link]);
        }
      } // CheckEvent

      // Sleep for 5us to avoid hammering the board too much
      usleep(1); 
    } // Done with all the modules

    // Escape if run is done -> kill thread
    if(!runInProgress)
      break;
  }
  
  cm_msg(MINFO,"link_thread", "Exiting thread %d clean...", link);
  std::cout << "Exiting thread " << link << " clean " << std::endl;
  thread_retval[link] = 0;
  pthread_exit((void*)&thread_retval[link]);
}

//
//----------------------------------------------------------------------------
/**
 * \brief   End of Run
 *
 * Called every stop run transition. Set equipment status in ODB,
 * stop acquisition on the modules.
 *
 * \param   [in]  run_number Number of the run being ended
 * \param   [out] error Can be used to write a message string to midas.log
 */
INT end_of_run(INT run_number, char *error)
{

  set_equipment_status(equipment[0].name, "Ending run...", "#FFFF00");
  cm_msg(MINFO,"EOR", "Start of end_of_run");
  printf("<<< Start of end_of_run \n");

  DWORD eStored;
  bool result;
  int * status;

  if(runInProgress){  //skip actions if we weren't running

    runInProgress = false;  //Signal threads to quit

    // Do not quit parent before children processes, wait for the proper
    // child exit first.
    for(int i=0; i < NBLINKSPERFE; ++i){
      pthread_join(tid[i],(void**)&status);
      printf(">>> Thread %d joined, return code: %d\n", i, *status);
    }

    // Stop run
    for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
      if (itv1725->IsConnected()) {  // Skip unconnected board
        result = itv1725->StopRun();

        if(!result)
          cm_msg(MERROR, "EOR",
                 "Could not stop the run for module %d", itv1725->GetModuleID());

	printf("Number of events in ring buffer for module-%i: %i\n",itv1725->GetModuleID(),itv1725->GetNumEventsInRB());

        rb_delete(itv1725->GetRingBufferHandle());
        itv1725->SetRingBufferHandle(-1);
	itv1725->ResetNumEventsInRB();
      }
    }

    // Info about event in HW buffer
    result = ov1725[0].Poll(&eStored);
    if(eStored != 0x0) {
      cm_msg(MERROR, "EOR", "Events left in the v1725-%i: %d",itv1725->GetModuleID(),eStored);
    }

  }

  printf(">>> End Of end_of_run\n\n");
  set_equipment_status(equipment[0].name, "Ended run", "#00ff00");

  return SUCCESS;
}

//
//----------------------------------------------------------------------------
/**
 * \brief   Pause Run
 *
 * Called every pause run transition.
 *
 * \param   [in]  run_number Number of the run being ended
 * \param   [out] error Can be used to write a message string to midas.log
 *
 * \return  Midas status code
 */
INT pause_run(INT run_number, char *error)
{

  cm_msg(MINFO,"PAUSE", "Beginning of pause_run");
  printf("<<< Beginning of pause_run \n");

  DWORD eStored;
  bool result;
  int * status;

  if(runInProgress){  //skip actions if we weren't running

    runInProgress = false;  //Signal threads to quit

    for(int i=0; i < NBLINKSPERFE; ++i){
      pthread_join(tid[i],(void**)&status);
      printf(">>> Thread %d joined, return code: %d\n", i, *status);
    }

    // Stop run
    for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
      if (itv1725->IsConnected()) {  // Skip unconnected board
        result = itv1725->StopRun();

        if(!result)
          cm_msg(MERROR, "EOR",
                 "Could not stop the run for module %d", itv1725->GetModuleID());

        rb_delete(itv1725->GetRingBufferHandle());
        itv1725->SetRingBufferHandle(-1);
		itv1725->ResetNumEventsInRB();
      }
    }

    result = ov1725[0].Poll(&eStored);
    if(eStored != 0x0) {
      cm_msg(MERROR, "EOR", "Events left in the v1725: %d",eStored);
    }

  }

  cm_msg(MINFO,"PAUSE", "End of pause_run");
  printf("<<< End of pause_run \n");
  return SUCCESS;
}

//
//----------------------------------------------------------------------------
/**
 * \brief   Resume Run
 *
 * Called every resume run transition.
 *
 * \param   [in]  run_number Number of the run being ended
 * \param   [out] error Can be used to write a message string to midas.log
 *
 * \return  Midas status code
 */
INT resume_run(INT run_number, char *error)
{

  printf("<<< Beginning of resume_run \n");

  int rb_handle;
  int status;

  runInProgress = true;

  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (! itv1725->IsConnected()) continue;   // Skip unconnected board

    // Done in frontend_init, or StartRun if settings have changed
    // itv1725->InitializeForAcq(); 

    bool go = itv1725->StartRun();
    if (go == false) return FE_ERR_HW;

    //Create ring buffer for board
    status = rb_create(event_buffer_size, max_event_size, &rb_handle);
    if(status == DB_SUCCESS){
      itv1725->SetRingBufferHandle(rb_handle);
    }
    else{
      cm_msg(MERROR, "feov1725:Resume", "Failed to create rb for board %d", itv1725->GetModuleID());
    }
  }

  //Create one thread per optical link
  for(int i=0; i<NBLINKSPERFE; ++i){
    thread_link[i] = i;
    status = pthread_create(&tid[i], NULL, &link_thread, (void*)&thread_link[i]);
    if(status){
      cm_msg(MERROR,"feov1725:Resume", "Couldn't create thread for link %d. Return code: %d", i, status);
    }
  }

  printf("<<< End of resume_run \n");
  return SUCCESS;
}

//
//----------------------------------------------------------------------------
DWORD prevtime = 0;
INT numloops = 0;
DWORD sn=0;
/**
 * \brief   Frontend loop
 *
 * If frontend_call_loop is true, this routine gets called when
 * the frontend is idle or once between every event.
 *
 * \return  Midas status code
 */
INT frontend_loop()
{

  return SUCCESS;
}

// ___________________________________________________________________
// Event polling; only ready for readout only when data is present in all ring buffers
extern "C" INT poll_event(INT source, INT count, BOOL test)
{

  register int i;

  for (i = 0; i < count; i++) {
    
    //ready for readout only when data is present in all ring buffers
    bool evtReady = true;
    for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
      if(itv1725->IsConnected() && (itv1725->GetNumEventsInRB() == 0)) {
        evtReady = false;
      }
    }

    //If event not ready or we're in test phase, keep looping
    if (evtReady && !test)
      //      return 0;
      return 1;
    
    usleep(20);
  }
  return 0;
}

//
//----------------------------------------------------------------------------
/**
 * \brief   Interrupt configuration (not implemented)
 *
 * Routine for interrupt configuration if equipment is set in EQ_INTERRUPT
 * mode.  Not implemented right now, returns SUCCESS.
 *
 * \param   [in]  cmd Command for interrupt events (see midas.h)
 * \param   [in]  source Equipment index number
 * \param   [in]  adr Interrupt routine (see mfe.c)
 *
 * \return  Midas status code
 */
extern "C" INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
{
  switch (cmd) {
  case CMD_INTERRUPT_ENABLE:
    break;
  case CMD_INTERRUPT_DISABLE:
    break;
  case CMD_INTERRUPT_ATTACH:
    break;
  case CMD_INTERRUPT_DETACH:
    break;
  }
  return SUCCESS;
}

//
//----------------------------------------------------------------------------
/**
 * \brief   Event readout
 *
 * Get data from all ring buffers and compose the MIDAS banks.
 */
INT read_event_from_ring_bufs(char *pevent, INT off) {
  DWORD *pdata;
  
  if (!runInProgress) return 0;
  
  sn = SERIAL_NUMBER(pevent);
  
  bk_init32(pevent);
  
  // Get the ChronoBox bank
  bk_create(pevent, "ZMQ0", TID_DWORD, (void **)&pdata);
  // Use ZMQ_DONTWAIT to prevent blocking.
  int stat = zmq_recv (subscriber, pdata, 1000, ZMQ_DONTWAIT);
  // PAA - As long as you don't close the bank, the bank won't be recorder
  if (stat > 0) {
    printf ("stat: %d  pdata[0]: %d ... ", stat, pdata[0]);
    printf("composing ZMQ bank\n");
    // May want to compare the CB-S/N and TS to the expected values
    // ...
    pdata += stat/sizeof(uint32_t); 
    stat = bk_close(pevent, pdata);
  }
  
  // Get the V1725
  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (! itv1725->IsConnected()) continue;   // Skip unconnected board

    // >>> Fill Event bank
    itv1725->FillEventBank(pevent);

  }

  INT ev_size = bk_size(pevent);
  if(ev_size == 0)
    cm_msg(MINFO,"read_trigger_event", "******** Event size is 0, SN: %d", sn);
  return ev_size;
}

//                                                                                         
//----------------------------------------------------------------------------             
INT read_buffer_level(char *pevent, INT off) {
  
  bk_init32(pevent);
  int PLLLockLossID = -1;
  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    itv1725->FillBufferLevelBank(pevent);

    // Check the PLL lock
    DWORD vmeStat, vmeAcq;
    itv1725->ReadReg(V1725_ACQUISITION_STATUS, &vmeAcq);
    if ((vmeAcq & 0x80) == 0) {
      PLLLockLossID= itv1725->GetModuleID();
      cm_msg(MINFO,"read_buffer_level","V1725 PLL loss lock Board:%d (vmeAcq=0x%x)"
             , itv1725->GetModuleID(), vmeAcq);
      itv1725->ReadReg(V1725_VME_STATUS, &vmeStat);
    }
  }
  
  // Set ODB flag if PLL lock lost
  if (PLLLockLossID > -1){
    char Path[255];
    sprintf(Path,"/DS Alarm/PLL Loss FE0%d",get_frontend_index());
    db_set_value(hDB, 0, Path, &(PLLLockLossID), sizeof(INT), 1, TID_INT);
    // PLL loss lock reset by the VME_STATUS read!
  }
  printf(" | ");  
  return bk_size(pevent);
}


//                                                                                         
//----------------------------------------------------------------------------             
INT read_temperature(char *pevent, INT off) {

  bk_init32(pevent);

  // Read the temperature for each ADC...
  DWORD temp;
  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725){

    DWORD *pdata;
    int addr;
    char bankName[5];
    sprintf(bankName,"TP%02d", itv1725->GetModuleID());
    bk_create(pevent, bankName, TID_DWORD, (void **)&pdata);
    for (int i=0;i<16;i++) {
     addr = V1725_CHANNEL_TEMPERATURE | (i << 8);
     itv1725->ReadReg(addr, &temp);
     *pdata++ =  temp;
     
    }
    bk_close(pevent,pdata);
    
    //- ZMQ test ----------------------------
    // Receiving ZMQ data
    //    zmq::message_t cb_data;
    //    subscriber.recv(&cb_data);
    //    printf("cb_data: %d\n", cb_data
    //    char msg[255] = {'\0'};

    /*
    uint32_t rcvbuf[100];
    int stat = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), ZMQ_DONTWAIT);
    if (stat > 0) {
    printf ("stat: %d - rcvbuf[0]: %d ... ", stat, rcvbuf[0]);
    printf("composing ZMQ bank\n");
    bk_create(pevent, "ZMQ0", TID_DWORD, (void **)&pdata);
    memcpy(pdata, rcvbuf, stat);
    pdata += stat/sizeof(uint32_t);
    bk_close(pevent, pdata);
    */
    
    /*
    bk_create(pevent, "ZMQ0", TID_DWORD, (void **)&pdata);
    int stat = zmq_recv (subscriber, pdata, 1000, ZMQ_DONTWAIT);
    // PAA - As long as you don't close the bank, the bank list & event is unchanged.
    if (stat > 0) {
      printf ("stat: %d  pdata[0]: %d ... ", stat, pdata[0]);
      printf("composing ZMQ bank\n");
      pdata += stat/sizeof(uint32_t); 
      stat = bk_close(pevent, pdata);
      printf("bk_close size:%d\n", stat);
    }
    */
    
    //- ZMQ test ----------------------------
  }
  return bk_size(pevent);
}

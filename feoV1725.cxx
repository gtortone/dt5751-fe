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
#include "mfe.h"
#include "v1725CONET2.hxx"

#include <zmq.h>
//#include <zmq.hpp>

// __________________________________________________________________
// --- General feov1725 parameters


#ifndef NBLINKSPERA3818
#define NBLINKSPERA3818   1   //!< Number of optical links used per A3818
#define NBLINKSPERFE      1   //!< Number of optical links controlled by each frontend
#define NB1725PERLINK     1   //!< Number of daisy-chained v1725s per optical link
#define NBV1725TOTAL      1  //!< Number of v1725 boards in total
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

/*-- Globals -------------------------------------------------------*/

//! The frontend name (client name) as seen by other MIDAS clients
char const *frontend_name = (char*)FE_NAME;
//! The frontend file name, don't change it
char const *frontend_file_name = (char*)__FILE__;
//! frontend_loop is called periodically if this variable is TRUE
BOOL frontend_call_loop = FALSE;
//! a frontend status page is displayed with this frequency in ms
INT display_period = 000;
//! maximum event size produced by this frontend (from #define in v1725CONET2.hxx)
INT max_event_size = V1725_MAX_EVENT_SIZE;
//! maximum event size for fragmented events (EQ_FRAGMENTED)
INT max_event_size_frag = 5 * 1024 * 1024;

//! buffer size to hold events
//! Very large events - don't consume too much memory
#if V1725_MAX_EVENT_SIZE > 30000000
INT event_buffer_size = 10 * max_event_size + 10000;
#elif V1725_MAX_EVENT_SIZE > 10000000
INT event_buffer_size = 20 * max_event_size + 10000;
#else
INT event_buffer_size = 30 * max_event_size + 10000;
#endif

bool runInProgress = false; //!< run is in progress
bool stopRunInProgress = false; //!<
bool eor_transition_called = false; // already called EOR
uint32_t timestamp_offset[NBLINKSPERFE*NB1725PERLINK]; //!< trigger time stamp offsets

BOOL enableChronobox = true;
BOOL enableMerging = true;
int unmergedModuleToRead = -1;
BOOL writePartiallyMergedEvents = false;
BOOL flushBuffersAtEndOfRun = false;
INT timestampMatchingThreshold = 50;

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

BOOL wait_buffer_empty(int transition, BOOL first);
INT read_event_from_ring_bufs(char *pevent, INT off);
INT read_buffer_level(char *pevent, INT off);
INT read_temperature(char *pevent, INT off);
void * link_thread(void *);
void *subscriber;

BOOL equipment_common_overwrite = false;

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

std::vector<v1725CONET2> ov1725; //!< objects for the v1725 modules controlled by this frontend
std::vector<v1725CONET2>::iterator itv1725;  //!< Main thread iterator
std::vector<v1725CONET2>::iterator itv1725_thread[NBLINKSPERFE];  //!< Link threads iterators

pthread_t tid[NBLINKSPERFE];                            //!< Thread ID
int thread_retval[NBLINKSPERFE] = {0};                  //!< Thread return value
int thread_link[NBLINKSPERFE];                          //!< Link number associated with each thread
bool is_first_event = true;

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

// Start the chronobox run going.  TRUE=start run, FALSE=stop run)
INT chronobox_start_stop(bool start){

  int status;
  if(start){
    status = system("esper-tool write -d true 192.168.1.3 mod_tdm run");
    printf("Started chronobox run; status = %i\n",status);
  }else{
    printf("Stopping chronobox run\n");
    status = system("esper-tool write -d false 192.168.1.3 mod_tdm run");
    printf("Stopped chronobox run; status = %i\n",status);
  }

  return status;
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

  {
    // Create flags for whether to enable Chronobox, and whether to merge data from all boards in same event.
    char cb_path[255], merge_path[255], partial_path[255], flush_path[255], thresh_path[255];
    int size_bool = sizeof(BOOL);
    int size_dword = sizeof(DWORD);
    sprintf(cb_path, "/Equipment/%s/Settings/Enable chronobox", equipment[0].name);
    sprintf(merge_path, "/Equipment/%s/Settings/Merge data from boards", equipment[0].name);
    sprintf(partial_path, "/Equipment/%s/Settings/Write partially merged events", equipment[0].name);
    sprintf(flush_path, "/Equipment/%s/Settings/Flush buffers at end of run", equipment[0].name);
    sprintf(thresh_path, "/Equipment/%s/Settings/TS match thresh (clock ticks)", equipment[0].name);
    db_get_value(hDB, 0, cb_path, &enableChronobox, &size_bool, TID_BOOL, TRUE);
    db_get_value(hDB, 0, merge_path, &enableMerging, &size_bool, TID_BOOL, TRUE);
    db_get_value(hDB, 0, partial_path, &writePartiallyMergedEvents, &size_bool, TID_BOOL, TRUE);
    db_get_value(hDB, 0, flush_path, &flushBuffersAtEndOfRun, &size_bool, TID_BOOL, TRUE);
    db_get_value(hDB, 0, thresh_path, &timestampMatchingThreshold, &size_dword, TID_DWORD, TRUE);
  }

  // --- Suppress watchdog for PICe for now  ; what is this???
  cm_set_watchdog_params(FALSE, 0);

  int nExpected = 0; //Number of v1725 boards we expect to activate
  int nActive = 0;   //Number of v1725 boards activated at the end of frontend_init
  std::vector<std::pair<int,int> > errBoards;  //v1725 boards which we couldn't connect to

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
    // Setup ODB record (create if necessary)
    itv1725->SetBoardRecord(hDB,seq_callback);
    // Set history ODB record (create if necessary)
    itv1725->SetHistoryRecord(hDB,seq_callback);

    if (itv1725->IsEnabled()) {
      nExpected++;
    }

    if (! itv1725->IsConnected()) continue;   // Skip unconnected board

    int status = itv1725->InitializeForAcq();
    nInitOk += status;
  }

  // Abort if board status not Ok.
  if (nInitOk != 0) return FE_ERR_HW;

  printf(">>> End of Init. %d active v1725. Expected %d\n\n", nActive, nExpected);

  if (nActive < nExpected){
    cm_msg(MERROR, __FUNCTION__, "Unexpected number of active boards (%d vs %d)", nActive, nExpected);
    return FE_ERR_HW;
  } 
  
  set_equipment_status(equipment[0].name, "Initialized", "#00ff00");

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(5, &mask);  //Main thread to core 5
  if( sched_setaffinity(0, sizeof(mask), &mask) < 0 ){
    printf("ERROR setting cpu affinity for main thread: %s\n", strerror(errno));
  }

  // Setup a deferred transition to wait till the V1725 buffer is empty.
  cm_register_deferred_transition(TR_STOP, wait_buffer_empty);

  //-begin - ZMQ----------------------------------------------------------

  //  Socket to talk to clients
  void *context = zmq_ctx_new ();
  subscriber = zmq_socket (context, ZMQ_SUB);
  int rc = zmq_connect (subscriber, "tcp://chronobox:5555");
  // Without message
  zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "", 0);
  printf (" This subscriber is connecting to the ChronoBox Publisher context: %p *subscriber: %p rc:%d \n"
	  , context, subscriber, rc);

  // Need to discard the first ZMQ bank.
  is_first_event = true;

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

  stopRunInProgress = false;
  eor_transition_called = false;

  runInProgress = true;
  {
    // Reset the PLL lock loss flag in ODB
    char Path[255];
    sprintf(Path,"/DS Alarm/PLL Loss FE0%d",get_frontend_index());
    INT dummy = -1;
    db_set_value(hDB, 0, Path, &(dummy), sizeof(INT), 1, TID_INT);
  }

  {
    // Create/read flags for whether to enable Chronobox, and whether to merge data from all boards in same event.
    char cb_path[255], merge_path[255], partial_path[255], flush_path[255], thresh_path[255];
    sprintf(cb_path, "/Equipment/%s/Settings/Enable chronobox", equipment[0].name);
    sprintf(merge_path, "/Equipment/%s/Settings/Merge data from boards", equipment[0].name);
    sprintf(partial_path, "/Equipment/%s/Settings/Write partially merged events", equipment[0].name);
    sprintf(flush_path, "/Equipment/%s/Settings/Flush buffers at end of run", equipment[0].name);
    sprintf(thresh_path, "/Equipment/%s/Settings/TS match thresh (clock ticks)", equipment[0].name);
    INT size = sizeof(BOOL);
    db_get_value(hDB, 0, cb_path, &enableChronobox, &size, TID_BOOL, TRUE);
    db_get_value(hDB, 0, merge_path, &enableMerging, &size, TID_BOOL, TRUE);
    db_get_value(hDB, 0, partial_path, &writePartiallyMergedEvents, &size, TID_BOOL, TRUE);
    db_get_value(hDB, 0, flush_path, &flushBuffersAtEndOfRun, &size, TID_BOOL, TRUE);
    size = sizeof(DWORD);
    db_get_value(hDB, 0, thresh_path, &timestampMatchingThreshold, &size, TID_DWORD, TRUE);
  }

  if (enableChronobox && !enableMerging) {
    cm_msg(MERROR, __FUNCTION__, "Invalid setup - you must merge data from all boards if running with the chronobox.");
    sprintf(error, "Invalid setup - you must merge data from all boards if running with the chronobox.");
    return FE_ERR_ODB;
  }

  if (enableChronobox) {
    /// Make sure the chronobox is stopped
    chronobox_start_stop(false);
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

  // Need to discard the first ZMQ bank.
  is_first_event = true;

  if (enableChronobox) {
    /// Sleep 1 second and start chronobox
    sleep(1);
    chronobox_start_stop(true);
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
    CPU_SET((link + 1), &mask);
    printf("core setting: NBCORES:%d link:%d core %d\n", NBCORES, link,(link + 1));
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
      if (!stopRunInProgress && itv1725_thread[link]->CheckEvent()){


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

  std::cout << "Exiting thread " << link << " clean " << std::endl;
  thread_retval[link] = 0;
  pthread_exit((void*)&thread_retval[link]);
}

timeval wait_start;

// Check how many events we have in the ring buffer
BOOL wait_buffer_empty(int transition, BOOL first)
 {

   if(first){
     printf("\nDeferred transition.  First call of wait_buffer_empty. Stopping run\n");
     // Some funny business here... need to pause the readout on the threads before
     // making the chronobox stop call... some sort of contention for the system resources.
     if (enableChronobox) {
       stopRunInProgress = true;
       usleep(500);
       chronobox_start_stop(false);
       stopRunInProgress = false;
       sleep(1);
       // We'll stop the V1725s later
    } else {
      for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
        if (itv1725->IsConnected()) {  // Skip unconnected board
          itv1725->StopRun();
        }
      }
    }

     gettimeofday(&wait_start, NULL);

     if (flushBuffersAtEndOfRun) {
       cm_msg(MINFO, __FUNCTION__, "Deferring transition to flush more data from boards");
       return FALSE;
     } else {
       return TRUE;
     }
   }

   bool haveEventsInBuffer = true;
   for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
     if(itv1725->IsConnected() && (itv1725->GetNumEventsInRB() == 0)) {
       haveEventsInBuffer = false;
     }
   }
   
   // Stay in deferred transition till all events are cleared
   if(haveEventsInBuffer){

     double wait_timeout_secs = 10;
     timeval now;
     gettimeofday(&now, NULL);
 
     if (now.tv_sec - wait_start.tv_sec + 1e-6 * (now.tv_usec - wait_start.tv_usec) > wait_timeout_secs) {
        cm_msg(MINFO, __FUNCTION__, "Still have data on boards, but taking too long to flush it all. Completing transition now.");
        return TRUE;
     } else {
        printf("Deferred transition: still have events\n");
        return FALSE;
     }
   }

  cm_msg(MINFO, __FUNCTION__, "Finished flushing data from ring buffers");
   printf("Deferred transition: cleared all events\n");
   return TRUE;
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
        if (enableChronobox) {
          // If no chronobox, we've already stopped the boards.
          result = itv1725->StopRun();
          
          if(!result) {
            cm_msg(MERROR, "EOR", "Could not stop the run for module %d", itv1725->GetModuleID());
          }
        }

	        printf("Number of events in ring buffer for module-%i: %i\n",itv1725->GetModuleID(),itv1725->GetNumEventsInRB());

        rb_delete(itv1725->GetRingBufferHandle());
        itv1725->SetRingBufferHandle(-1);
	      itv1725->ResetNumEventsInRB();
      }
    }

    // Info about event in HW buffer
    result = ov1725[0].Poll(&eStored);
    if(eStored != 0x0) {
      cm_msg(MERROR, "EOR", "Events left in the v1725-%d: %d",ov1725[0].GetModuleID(), eStored);
    }

  }

  if (enableChronobox) {
    // Clear out all the events in the ZMQ buffer:
    uint32_t rcvbuf [100];
    int total_extra = 0;
    int stat;
    stat = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), ZMQ_DONTWAIT);
    while(stat > 0){
      total_extra++;
      stat = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), ZMQ_DONTWAIT);
    }
    if(total_extra >0) cm_msg(MINFO, "EOR", "Events left in the chronobox: %d",total_extra);
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
INT poll_event(INT source, INT count, BOOL test)
{

  register int i;

  for (i = 0; i < count; i++) {
    for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
      if(itv1725->IsConnected()) {
        itv1725->IssueSwTrigIfNeeded();
      }
    }

    bool evtReady = true;
    unmergedModuleToRead = -1;

    if (enableMerging) {
      //ready for readout only when data is present in all ring buffers
      for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
        if(itv1725->IsConnected() && (itv1725->GetNumEventsInRB() == 0)) {
          evtReady = false;
        }
      }
    } else {
      //ready for readout when data is present on any ring buffer
      evtReady = false;
      int maxNumEvents = -1;

      for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
        if (!itv1725->IsConnected()) {
          continue;
        }

        int numEvents = itv1725->GetNumEventsInRB();

        if (numEvents > 0) {
          evtReady = true;

          if (numEvents > maxNumEvents) {
            // Tell the readout routine to read data from the module with the
            // most events in the buffer; this helps to more fairly read data
            // from multiple boards when we're not merging the data.
            unmergedModuleToRead = itv1725->GetModuleID();
            maxNumEvents = numEvents;
          }
        }
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
INT interrupt_configure(INT cmd, INT source, POINTER_T adr)
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

  // Keep track of timestamps
  std::vector<uint32_t> timestamps;

  if (enableChronobox) {
    // Get the ChronoBox bank
    // If this is the first event, then read ZMQ buffer an extra time; want to discard first event.
    if(is_first_event){
      uint32_t rcvbuf [100];
      int stat0 = zmq_recv (subscriber, rcvbuf, sizeof(rcvbuf), ZMQ_DONTWAIT);
      if(!stat0){
        cm_msg(MERROR,"read_trigger_event", "ZMQ read error on first event. %i\n",stat0);
      }
      is_first_event = false;
      printf("Flushed first event from chronobox\n");
    }

    int stat = -1;

    bk_create(pevent, "ZMQ0", TID_DWORD, (void **)&pdata);

    // Try to receive ZMQ data from chronobox.
    // Use ZMQ_DONTWAIT to prevent blocking.
    // But retry several times in case message is delayed.
    float zmq_timeout_ms = 100;
    float zmq_retry_wait_ms = 1;
    float zmq_time = 0;

    while (zmq_time < zmq_timeout_ms) {
      stat = zmq_recv (subscriber, pdata, 1000, ZMQ_DONTWAIT);

      if (stat > 0) {
        break;
      }

      zmq_time += zmq_retry_wait_ms;
      usleep(1000 * zmq_retry_wait_ms);
    }

    if (stat > 0) {

      //printf("ZMQ: %x %x %x %x %x",pdata[0],pdata[1],pdata[2],pdata[3],pdata[4]);
      // Save the timestamp for ZMQ bank
      timestamps.push_back((pdata[3]& 0x7fffffff)); // Save the ZMQ timestamp
      pdata += stat/sizeof(uint32_t);
      stat = bk_close(pevent, pdata);
    }else{
      // There should be ZMQ data for each bank.  If not, stop the run.
      if(!eor_transition_called){
        cm_msg(MERROR,"read_trigger_event", "Error: did not receive a ZMQ bank after %f ms.  Stopping run.", zmq_timeout_ms);
        cm_transition(TR_STOP, 0, NULL, 0, TR_DETACH, 0);
        eor_transition_called = true;
        return 0;
      }
    }
  } // End of chronobox

  // Get the V1725
  if (!enableMerging && unmergedModuleToRead < 0) {
    cm_msg(MERROR,"read_trigger_event", "Error: module to read is set to invalid value %d! Stopping run.", unmergedModuleToRead);
    cm_transition(TR_STOP, 0, NULL, 0, TR_DETACH, 0);
    eor_transition_called = true;
    return 0;
  }

  int64_t minTimestamp = 0xFFFFFFFF;
  int64_t rolloverTime = 0x80000000;
  DWORD numConnectedBoards = 0;

  if (enableMerging && !enableChronobox) {
    // Merge by timestamp
    for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
      if (! itv1725->IsConnected()) continue;   // Skip unconnected board

      numConnectedBoards++;
      int64_t thisTimestamp = itv1725->PeekRBTimestamp();

      //printf("this: %08lx, curr min: %08lx, rollover: %d\n", thisTimestamp, minTimestamp, std::abs(thisTimestamp - minTimestamp) > rolloverTime / 2);

      if (minTimestamp == 0xFFFFFFFF) {
        // First timestamp
        minTimestamp = thisTimestamp;
      } else if (std::abs(thisTimestamp - minTimestamp) > rolloverTime / 2) {
        // Had rollover
        if (thisTimestamp > minTimestamp) {
          minTimestamp = thisTimestamp;
        }
      } else if (thisTimestamp < minTimestamp) {
        // No rollover
        minTimestamp = thisTimestamp;
      }
    }
  }

  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725) {
    if (! itv1725->IsConnected()) continue;   // Skip unconnected board

    if (enableMerging && itv1725->GetNumEventsInRB() == 0) {
        cm_msg(MERROR,"read_trigger_event", "Error: no events in RB for module %d.  Stopping run.", itv1725->GetModuleID());
        cm_transition(TR_STOP, 0, NULL, 0, TR_DETACH, 0);
        eor_transition_called = true;
        return 0;
    }

    if (!enableMerging && itv1725->GetModuleID() != unmergedModuleToRead) {
      continue;
    }

    DWORD thisTimestamp = itv1725->PeekRBTimestamp();
    DWORD deltaTimestamp = thisTimestamp - minTimestamp;

    if (deltaTimestamp > 0x7FFFFFFF) {
      // Handle rollover
      deltaTimestamp -= 0x7FFFFFFF;
    }

    if (enableMerging && !enableChronobox && deltaTimestamp > timestampMatchingThreshold) {
      continue;
    }

    // >>> Fill Event bank
    uint32_t timestamp;
    itv1725->FillEventBank(pevent,timestamp);

    if (!enableMerging) {
      // Only saving data from 1 board.
      break;
    }

    // Save timestamp for ZLE bank.
    timestamps.push_back((timestamp & 0x7fffffff));
  }


  // Check the timestamps
  if (timestamps.size() > 1) {
    for(unsigned int i = 1; i < timestamps.size(); i++){
      uint32_t diff1 = timestamps[0]-timestamps[i];
      uint32_t diff2 = timestamps[i]-timestamps[0];
      uint32_t diff = (diff1 < diff2) ? diff1 : diff2;
      double fdiff = diff*0.000000008;

      printf("idx:%i sze:%zu [0]:0x%08x [%i]:0x%08x diff:%i secs:%f diff_secs:%f \n", i, timestamps.size(), timestamps[0], i,  timestamps[i], diff, timestamps[i]*0.000000008,fdiff);
    }
  } else {
    printf("only 1 timestamp, [0]:0x%08x secs:%f\n", timestamps[0], timestamps[0]*0.000000008);
  }

  if (enableMerging && !enableChronobox && !writePartiallyMergedEvents && timestamps.size() != numConnectedBoards) {
    printf("Skipping event at time 0x%08x as only have data from %d/%d boards.\n", minTimestamp, (DWORD)timestamps.size(), numConnectedBoards);
    return 0;
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
    if (!itv1725->IsConnected()) {
      continue;
    }
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

  DWORD *pdata;
  bk_init32(pevent);

  // Read the temperature for each ADC...
  DWORD temp;
  for (itv1725 = ov1725.begin(); itv1725 != ov1725.end(); ++itv1725){
    if (!itv1725->IsConnected()) {
      continue;
    }

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
  }

  if(0){
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
  }
  return bk_size(pevent);
}


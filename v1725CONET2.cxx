/*****************************************************************************/
/**
\file v1725CONET2.cxx

## Contents

This file contains the class implementation for the v1725 module driver.
 *****************************************************************************/

#include "v1725CONET2.hxx"
#include <execinfo.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <fstream>

#define UNUSED(x) ((void)(x)) //!< Suppress compiler warnings
#define NUM_SQ_WORDS 5
#define POSTPONE_CHARGE_FILTERING 0
#define MAX_QT_WORDS 2000

//! Configuration string for this board. (ODB: /Equipment/[eq_name]/Settings/[board_name]/)
const char * v1725CONET2::config_str_board[] = {\
    "Enable = BOOL : y",\
    "Has ZLE firmware = BOOL : n",\
    "Acq mode = INT : 5",\
    "Board Configuration = DWORD : 16",\
    "Buffer organization = INT : 10",\
    "Custom size = INT : 40",\
    "Channel Mask = DWORD : 0xFFFF",\
    "Trigger Source = DWORD : 1073741824",\
    "Trigger Output = DWORD : 1073741824",\
    "Post Trigger = DWORD : 100",\
    "Pre Trigger = DWORD : 100",\
    "Front panel IO = DWORD : 0x4D013C",\
    "Enable ZLE = BOOL : n",\
    "almost_full = DWORD : 512",\
    "Front panel LVDS IO = DWORD : 0x1100",\
    "SelfTrigger_Threshold = DWORD[16] :",\
    "[0] 3870",\
    "[1] 3870",\
    "[2] 3870",\
    "[3] 3870",\
    "[4] 3870",\
    "[5] 3870",\
    "[6] 3870",\
    "[7] 3870",\
    "[8] 3870",\
    "[9] 3870",\
    "[10] 3870",\
    "[11] 3870",\
    "[12] 3870",\
    "[13] 3870",\
    "[14] 3870",\
    "[15] 3870",\
    "SelfTrigger_Logic = DWORD[8] :",\
    "[0] 3",\
    "[1] 3",\
    "[2] 3",\
    "[3] 3",\
    "[4] 3",\
    "[5] 3",\
    "[6] 3",\
    "[7] 3",\
    "ZLESignedThresh = INT[16] :",\
    "[0] -5",\
    "[1] -5",\
    "[2] -5",\
    "[3] -5",\
    "[4] -5",\
    "[5] -5",\
    "[6] -5",\
    "[7] -5",\
    "[8] -5",\
    "[9] -5",\
    "[10] -5",\
    "[11] -5",\
    "[12] -5",\
    "[13] -5",\
    "[14] -5",\
    "[15] -5",\
    "ZLENBinsBefore = DWORD[16] :",\
    "[0] 0x5",\
    "[1] 0x5",\
    "[2] 0x5",\
    "[3] 0x5",\
    "[4] 0x5",\
    "[5] 0x5",\
    "[6] 0x5",\
    "[7] 0x5",\
    "[8] 0x5",\
    "[9] 0x5",\
    "[10] 0x5",\
    "[11] 0x5",\
    "[12] 0x5",\
    "[13] 0x5",\
    "[14] 0x5",\
    "[15] 0x5",\
    "ZLENBinsAfter = DWORD[16] :",\
    "[0] 0x5",\
    "[1] 0x5",\
    "[2] 0x5",\
    "[3] 0x5",\
    "[4] 0x5",\
    "[5] 0x5",\
    "[6] 0x5",\
    "[7] 0x5",\
    "[8] 0x5",\
    "[9] 0x5",\
    "[10] 0x5",\
    "[11] 0x5",\
    "[12] 0x5",\
    "[13] 0x5",\
    "[14] 0x5",\
    "[15] 0x5",\
    "ZLEBaseline = DWORD[16] :",\
    "[0] 0x30000",\
    "[1] 0x30000",\
    "[2] 0x30000",\
    "[3] 0x30000",\
    "[4] 0x30000",\
    "[5] 0x30000",\
    "[6] 0x30000",\
    "[7] 0x30000",\
    "[8] 0x30000",\
    "[9] 0x30000",\
    "[10] 0x30000",\
    "[11] 0x30000",\
    "[12] 0x30000",\
    "[13] 0x30000",\
    "[14] 0x30000",\
    "[15] 0x30000",\
    "DAC = DWORD[16] :",\
    "[0] 10000",\
    "[1] 10000",\
    "[2] 10000",\
    "[3] 10000",\
    "[4] 10000",\
    "[5] 10000",\
    "[6] 10000",\
    "[7] 10000",\
    "[8] 10000",\
    "[9] 10000",\
    "[10] 10000",\
    "[11] 10000",\
    "[12] 10000",\
    "[13] 10000",\
    "[14] 10000",\
    "[15] 10000",\
	"Dynamic Range 2V (y) 0.5V (n) = BOOL[16] :",\
	"[0] y",\
	"[1] y",\
	"[2] y",\
	"[3] y",\
	"[4] y",\
	"[5] y",\
	"[6] y",\
	"[7] y",\
	"[8] y",\
	"[9] y",\
	"[10] y",\
	"[11] y",\
	"[12] y",\
	"[13] y",\
	"[14] y",\
	"[15] y",\
    NULL
};

const char v1725CONET2::history_settings[][NAME_LENGTH] = { "eStored", "busy", "rb_level" };

/**
 * \brief   Constructor for the module object
 *
 * Set the basic hardware parameters
 *
 * \param   [in]  feindex   Frontend index number
 * \param   [in]  link      Optical link number
 * \param   [in]  board     Board number on the optical link
 * \param   [in]  moduleID  Unique ID assigned to module
 */
v1725CONET2::v1725CONET2(int feindex, int link, int board, int moduleID, HNDLE hDB)
: feIndex_(feindex), link_(link), board_(board), moduleID_(moduleID), odb_handle_(hDB), num_events_in_rb_(0)
{
  device_handle_ = -1;
  settings_handle_ = 0;
  settings_loaded_ = false;
  settings_touched_ = false;
  running_= false;
  data_type_ = RawPack2;
  rb_handle_ = -1;
  verbosity_ = 0;

  // Start by assuming the board is enabled; will be overriden by ODB later.
  config.enable = true;

}
/**
 * Move constructor needed because we're putting v1725CONET2 objects in a vector which requires
 * either the copy or move operation.  The implicit move constructor (or copy constructor)
 * cannot be created by the compiler because our class contains an atomic object with a
 * deleted copy constructor. */
v1725CONET2::v1725CONET2(v1725CONET2&& other) noexcept
: feIndex_(std::move(other.feIndex_)), link_(std::move(other.link_)), board_(std::move(other.board_)),
    moduleID_(std::move(other.moduleID_)), odb_handle_(std::move(other.odb_handle_)),
        num_events_in_rb_(other.num_events_in_rb_.load())
{
  device_handle_ = std::move(other.device_handle_);
  settings_handle_ = std::move(other.settings_handle_);
  settings_loaded_ = std::move(other.settings_loaded_);
  settings_touched_ = std::move(other.settings_touched_);
  running_= std::move(other.running_);
  data_type_ = std::move(other.data_type_);
  rb_handle_ = std::move(other.rb_handle_);
  data_type_ = std::move(other.data_type_);
  verbosity_ = std::move(other.verbosity_);
  config = std::move(other.config);


}
v1725CONET2& v1725CONET2::operator=(v1725CONET2&& other) noexcept
{
  if (this != &other){  //if trying to assign object to itself

    feIndex_ = std::move(other.feIndex_);
    link_ = std::move(other.link_);
    board_ = std::move(other.board_);
    moduleID_ = std::move(other.moduleID_);
    odb_handle_ = std::move(other.odb_handle_);
    num_events_in_rb_ = other.num_events_in_rb_.load();
    device_handle_ = std::move(other.device_handle_);
    settings_handle_ = std::move(other.settings_handle_);
    settings_loaded_ = std::move(other.settings_loaded_);
    settings_touched_ = std::move(other.settings_touched_);
    running_= std::move(other.running_);
    rb_handle_ = std::move(other.rb_handle_);
    data_type_ = std::move(other.data_type_);
    verbosity_ = std::move(other.verbosity_);
    config = std::move(other.config);

  }
  return *this;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Destructor for the module object
 *
 * Nothing to do.
 */
v1725CONET2::~v1725CONET2()
{
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Get short string identifying the module's index, link and board number
 *
 * \return  name string
 */
std::string v1725CONET2::GetName()
{
  std::stringstream txt;
  txt << "F" << std::setfill('0') << std::setw(2) << feIndex_
      << "L" << std::setfill('0') << std::setw(2) << link_
      << "B" << std::setfill('0') << std::setw(2) << board_;


  return txt.str();
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Get connected status
 *
 * \return  true if board is connected
 */
bool v1725CONET2::IsConnected()
{
  return (device_handle_ >= 0) && config.enable;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Get run status
 *
 * \return  true if run is started
 */
bool v1725CONET2::IsRunning()
{
  return running_;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Connect the board through the optical link
 *
 * \return  CAENComm Error Code (see CAENComm.h)
 */
v1725CONET2::ConnectErrorCode v1725CONET2::Connect()
{
  return Connect(2, 5);  //reasonable default values
}

//
//--------------------------------------------------------------------------------
v1725CONET2::ConnectErrorCode v1725CONET2::Connect(int connAttemptsMax, int secondsBeforeTimeout)
{
  if (verbosity_) std::cout << GetName() << "::Connect()\n";

  ConnectErrorCode returnCode;

  if (IsConnected()) {
    cm_msg(MERROR,"Connect","Board %d already connected", this->GetModuleID());
    returnCode = ConnectErrorAlreadyConnected;
  }


  /* The optical connection hangs quite often, do it in a thread and timeout if necessary */
  CAENComm_ErrorCode sCAEN;
  pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

  pthread_t con_thread;
  volatile struct v1725CONET2::thread_args con_thread_args = { this , &sCAEN, &cv };
  timespec wait_time;
  wait_time.tv_nsec = 0;
  int status;

  /* For info purposes */
  std::stringstream * ssErrMsg;
  timespec start_time, end_time;

  start_time.tv_nsec = end_time.tv_nsec = 0;
  start_time.tv_sec = time(NULL);

  for (int i = 0; i < connAttemptsMax; ++i){

    status = pthread_create(&con_thread, NULL, &v1725CONET2::connectThread, (void*)&con_thread_args);
    if(status){
      cm_msg(MERROR,"Connect", "Couldn't create thread for link %d board %d. Return code: %d",
          this->GetLink(), this->GetBoard(), status);
    }
    pthread_mutex_lock(&m);
    ssErrMsg = new std::stringstream;

    wait_time.tv_sec = time(NULL) + secondsBeforeTimeout;
    if(pthread_cond_timedwait(&cv, &m, &wait_time) == ETIMEDOUT){
      end_time.tv_sec = time(NULL);

      pthread_mutex_unlock(&m);

      *ssErrMsg << "CAENComm_OpenDevice attempt #" << i+1 << " timeout (" << secondsBeforeTimeout << "s).";
      *ssErrMsg << " Total elapsed time: " << end_time.tv_sec - start_time.tv_sec << "s";
      *ssErrMsg << " FE Index: " << feIndex_;
      *ssErrMsg << " Link: " << link_;
      *ssErrMsg << " Board: " << board_;
      *ssErrMsg << " Module ID: " << moduleID_;

      if(i < (connAttemptsMax - 1)){
        *ssErrMsg << " Retrying... ";
      }

      cm_msg(MERROR, "Connect", ssErrMsg->str().c_str());
      returnCode = ConnectErrorTimeout;
    }
    else{
      end_time.tv_sec = time(NULL);

      pthread_mutex_unlock(&m);
      pthread_detach(con_thread);

      if (sCAEN == CAENComm_Success) {
        printf("Link#:%d Board#:%d Module_Handle[%d]:%d\n",
                link_, board_, moduleID_, this->GetDeviceHandle());

        returnCode = ConnectSuccess;
      }
      else {
        device_handle_ = -1;

        *ssErrMsg << "CAENComm_OpenDevice error.";
        *ssErrMsg << " FE Index: " << feIndex_;
        *ssErrMsg << " Link: " << link_;
        *ssErrMsg << " Board: " << board_;
        *ssErrMsg << " Module ID: " << moduleID_;
        *ssErrMsg << " CAENComm_ErrorCode: " << sCAEN;

        cm_msg(MERROR, "Connect", ssErrMsg->str().c_str());
        returnCode = ConnectErrorCaenComm;
      }

      break;
    }
  }

  return returnCode;
}

//
//--------------------------------------------------------------------------------
void * v1725CONET2::connectThread(void * arg){

  v1725CONET2::thread_args * t_args = (v1725CONET2::thread_args*)arg;

  std::cout << "Opening device (i,l,b) = ("
      << t_args->v1725->feIndex_ << ","
      << t_args->v1725->link_ << ","
      << t_args->v1725->board_ << ")" << std::endl;

  *(t_args->errcode) = CAENComm_OpenDevice(CAENComm_PCIE_OpticalLink, t_args->v1725->link_, t_args->v1725->board_,
      0, &(t_args->v1725->device_handle_));
  pthread_cond_signal(t_args->cv);

  return NULL;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Disconnect the board through the optical link
 *
 * \return  CAENComm Error Code (see CAENComm.h)
 */
bool v1725CONET2::Disconnect()
{
  if (verbosity_) std::cout << GetName() << "::Disconnect()\n";

  if (device_handle_ < 0) {
    return true;
  }
  if (!IsConnected()) {
    cm_msg(MERROR,"Disconnect","Board %d already disconnected", this->GetModuleID());
    return false;
  }
  if (IsRunning()) {
    cm_msg(MERROR,"Disconnect","Can't disconnect board %d: run in progress", this->GetModuleID());
    return false;
  }

  if (verbosity_) std::cout << "Closing device (i,l,b) = (" << feIndex_ << "," << link_ << "," << board_ << ")" << std::endl;

  CAENComm_ErrorCode sCAEN = CAENComm_CloseDevice(device_handle_);

  if(sCAEN == CAENComm_Success){
    device_handle_ = -1;
  }
  else
    return false;

  return true;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Start data acquisition
 *
 * Write to Acquisition Control reg to put board in RUN mode. If ODB
 * settings have been changed, re-initialize the board with the new settings.
 * Set _running flag true.
 *
 * \return  CAENComm Error Code (see CAENComm.h)
 */
bool v1725CONET2::StartRun()
{
  if (verbosity_) std::cout << GetName() << "::StartRun()\n";

  if (IsRunning()) {
    cm_msg(MERROR,"StartRun","Board %d already started", this->GetModuleID());
    return false;
  }
  if (!IsConnected()) {
    cm_msg(MERROR,"StartRun","Board %d disconnected", this->GetModuleID());
    return false;
  }

	std::cout << "reinitializing" << std::endl;
	
	//Re-read the record from ODB, it may have changed
	int size = sizeof(V1725_CONFIG_SETTINGS);
	db_get_record(odb_handle_, settings_handle_, &config, &size, 0);
	
	int status = InitializeForAcq();
	if (status == -1){std::cout << "Failed to Acq " << std::endl; return false;  }

  CAENComm_ErrorCode e = AcqCtl_(V1725_RUN_START);
  if (e == CAENComm_Success){
    running_=true;
	}else{
		std::cout << "Failed to start run... " << std::endl;
    return false;
	}	
  return true;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Start data acquisition
 *
 * Write to Acquisition Control reg to put board in STOP mode.
 * Set _running flag false.
 *
 * \return  CAENComm Error Code (see CAENComm.h)
 */
bool v1725CONET2::StopRun()
{
  if (verbosity_) std::cout << GetName() << "::StopRun()\n";

  if (!IsRunning()) {
    cm_msg(MERROR,"StopRun","Board %d already stopped", this->GetModuleID());
    return false;
  }
  if (!IsConnected()) {
    cm_msg(MERROR,"StopRun","Board %d disconnected", this->GetModuleID());
    return false;
  }

  CAENComm_ErrorCode e = AcqCtl_(V1725_RUN_STOP);
  if (e == CAENComm_Success)
    running_=false;
  else
    return false;

  return true;
}


//
//--------------------------------------------------------------------------------
/**
 * \brief   Control data acquisition
 *
 * Write to Acquisition Control reg
 *
 * \param   [in]  operation acquisition mode (see v1725.h)
 * \return  CAENComm Error Code (see CAENComm.h)
 */
CAENComm_ErrorCode v1725CONET2::AcqCtl_(uint32_t operation)
{

  uint32_t reg;
  CAENComm_ErrorCode sCAEN;

  sCAEN = CAENComm_Read32(device_handle_, V1725_ACQUISITION_CONTROL, &reg);

  switch (operation) {
  case V1725_RUN_START:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, (reg | 0x4));
    break;
  case V1725_RUN_STOP:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, (reg & ~( 0x4)));

    break;
  case V1725_REGISTER_RUN_MODE:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, 0x100);
    break;
  case V1725_SIN_RUN_MODE:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, 0x101);
    break;
  case V1725_SIN_GATE_RUN_MODE:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, 0x102);
    break;
  case V1725_MULTI_BOARD_SYNC_MODE:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, 0x103);
    break;
  case V1725_COUNT_ACCEPTED_TRIGGER:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, (reg & ~( 0x8)));
    break;
  case V1725_COUNT_ALL_TRIGGER:
    sCAEN = CAENComm_Write32(device_handle_, V1725_ACQUISITION_CONTROL, (reg | 0x8));
    break;
  default:
    printf("operation %d not defined\n", operation);
    break;
  }
  return sCAEN;

}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Control data acquisition
 *
 * Write to Acquisition Control reg
 *
 * \param   [in]  operation acquisition mode (see v1725.h)
 * \return  CAENComm Error Code (see CAENComm.h)
 */
CAENComm_ErrorCode v1725CONET2::WriteChannelConfig_(uint32_t operation)
{
  return ov1725_ChannelConfig(device_handle_, operation);
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Read 32-bit register
 *
 * \param   [in]  address  address of the register to read
 * \param   [out] val      value read from register
 * \return  CAENComm Error Code (see CAENComm.h)
 */
CAENComm_ErrorCode v1725CONET2::ReadReg_(DWORD address, DWORD *val)
{
  if (verbosity_ >= 2) {
    std::cout << GetName() << "::ReadReg(" << std::hex << address << ")" << std::endl;
    printf("Module: %d, verbosity: %d\n", this->GetModuleID(), verbosity_);
  }
  return CAENComm_Read32(device_handle_, address, val);
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Write to 32-bit register
 *
 * \param   [in]  address  address of the register to write to
 * \param   [in]  val      value to write to the register
 * \return  CAENComm Error Code (see CAENComm.h)
 */
CAENComm_ErrorCode v1725CONET2::WriteReg_(DWORD address, DWORD val)
{

#define SIZE 100

  if(address == 0x8108){
    int nptrs, j;
    void *buffer[SIZE];
    char **strings;

    nptrs = backtrace(buffer, SIZE);
    printf("backtrace() returned %d addresses\n", nptrs);

    strings = backtrace_symbols(buffer, nptrs);

    for (j = 0; j < nptrs; j++)
        printf("%s\n", strings[j]);

    free(strings);
  }

  if (verbosity_ >= 2) std::cout << GetName() << "::WriteReg(" << std::hex << address << "," << val << ")" << std::endl;
  return CAENComm_Write32(device_handle_, address, val);
}

//
//--------------------------------------------------------------------------------
bool v1725CONET2::ReadReg(DWORD address, DWORD *val)
{
  return (ReadReg_(address, val) == CAENComm_Success);
}

//
//--------------------------------------------------------------------------------
bool v1725CONET2::WriteReg(DWORD address, DWORD val)
{
  return (WriteReg_(address, val) == CAENComm_Success);
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Poll Event Stored register
 *
 * Check Event Stored register for any event stored
 *
 * \param   [out]  val     Number of events stored
 * \return  CAENComm Error Code (see CAENComm.h)
 */
bool v1725CONET2::Poll(DWORD *val)
{
  if (!IsConnected()) {
    return false;
  }

  CAENComm_ErrorCode sCAEN = CAENComm_Read32(device_handle_, V1725_EVENT_STORED, val);
  return (sCAEN == CAENComm_Success);
}

//! Maximum size of data to read using BLT (32-bit) cycle
#define MAX_BLT_READ_SIZE_BYTES 1200000

//
//--------------------------------------------------------------------------------
bool v1725CONET2::CheckEvent()
{
  DWORD vmeStat, eStored;
  this->ReadReg(V1725_VME_STATUS, &vmeStat);
  return (vmeStat & 0x1);
}

//
//--------------------------------------------------------------------------------
bool v1725CONET2::ReadEvent(void *wp)
{
  CAENComm_ErrorCode sCAEN;
  
  DWORD size_remaining_dwords, to_read_dwords, *pdata = (DWORD *)wp;
  int dwords_read_total = 0, dwords_read = 0;

	// Block read to get all data from board.  
  sCAEN = ReadReg_(V1725_EVENT_SIZE, &size_remaining_dwords);
	while ((size_remaining_dwords > 0) && (sCAEN == CAENComm_Success)) {
    
    //calculate amount of data to be read in this iteration
    to_read_dwords = (size_remaining_dwords > MAX_BLT_READ_SIZE_BYTES/sizeof(DWORD)) ?
      MAX_BLT_READ_SIZE_BYTES/sizeof(DWORD) : size_remaining_dwords;
    sCAEN = CAENComm_BLTRead(device_handle_, V1725_EVENT_READOUT_BUFFER, (DWORD *)pdata, to_read_dwords, &dwords_read);
    
    if (verbosity_>=2) std::cout << sCAEN << " = BLTRead(handle=" << device_handle_
                                 << ", addr=" << V1725_EVENT_READOUT_BUFFER
                                 << ", pdata=" << pdata
                                 << ", to_read_dwords=" << to_read_dwords
                                 << ", dwords_read returned " << dwords_read << ");" << std::endl;
  
    //increment pointers/counters
    dwords_read_total += dwords_read;
    size_remaining_dwords -= dwords_read;
    pdata += dwords_read;
  }
  
  rb_increment_wp(this->GetRingBufferHandle(), dwords_read_total*sizeof(int));
  
  this->IncrementNumEventsInRB(); //atomic
  if (sCAEN != CAENComm_Success) 
    cm_msg(MERROR,"ReadEvent", "Communication error: %d", sCAEN);

  return (sCAEN == CAENComm_Success);
}


//
//--------------------------------------------------------------------------------
bool v1725CONET2::FillEventBank(char * pevent, uint32_t &timestamp)
{
  if (! this->IsConnected()) {
    cm_msg(MERROR,"FillEventBank","Board %d disconnected", this->GetModuleID());
    return false;
  }

  DWORD *src=NULL;
  DWORD *dest=NULL;

  int status = rb_get_rp(this->GetRingBufferHandle(), (void**)&src, 500);
  if (status == DB_TIMEOUT) {
    cm_msg(MERROR,"FillEventBank", "Got rp timeout for module %d", this->GetModuleID());
    printf("### num events: %d\n", this->GetNumEventsInRB());
    return false;
  }


  if ((*src & 0xF0000000) != 0xA0000000){
    cm_msg(MERROR,"FillEventBank","Incorrect hearder for board:%d (0x%x)", this->GetModuleID(), *src);
    return false;
  }

  uint32_t size_words = *src & 0x0FFFFFFF;
  uint32_t size_copied = size_words;
	timestamp = src[3];

  // >>> create data bank
  char bankName[5];
  snprintf(bankName, sizeof(bankName), "W2%02d", this->GetModuleID());
  if(this->IsZLEData()){
    snprintf(bankName, sizeof(bankName), "ZL%02d", this->GetModuleID());
  }
  else{
    snprintf(bankName, sizeof(bankName), "W2%02d", this->GetModuleID());
  }
 // printf("Bank size (before %s): %u, event size: %u\n", bankName, bk_size(pevent), size_words);
  bk_create(pevent, bankName, TID_DWORD, (void **)&dest);


  uint32_t limit_size = (V1725_MAX_EVENT_SIZE-bk_size(pevent))/4; // what space is left in the event (in DWORDS)
  if (size_words > limit_size) {
//    printf("Event with size: %u (Module %02d) bigger than max %u, event truncated\n", size_words, this->GetModuleID(), limit_size);
    cm_msg(MERROR,"FillEventBank","Event with size: %u (Module %02d) bigger than max %u, event truncated", size_words, this->GetModuleID(), limit_size);
    if(this->IsZLEData()){
      uint32_t toBeCopyed = 4; // Starting with the header
			// We need to find out how many channels we can copy before reaching the limit...
			int i;
			for (i=16; i>0 ; --i){ //We have potentially 16 channels to copy
        uint32_t channelSize = 0;
				channelSize = *(src+toBeCopyed); // Get the size of the data for this channel
				if (toBeCopyed + channelSize > limit_size) break; 
				toBeCopyed += channelSize; // We have enough space for this channel
			}
			size_copied = toBeCopyed + i; //This it the size of the headers, the filled channel, and the "empty channels size" padding.
			//    printf("will be copied: %u out of %u (%d channels)\n", size_copied, size_words, (8-i));
      cm_msg(MERROR,"FillEventBank","will be copied: %u out of %u (%d channels)", size_copied, size_words, (8-i));
      *(src + 0) = 0xA0000000 + size_copied; // Adjust the event size
			for ( ; i>0 ; --i){
				*(src + toBeCopyed+(i-1)) = (uint32_t) 0x1; // Pad the empty channel size = 1 DWORDS
			}
		}
		else {
			//      printf("Raw mode with long waveforms, exceeding the limit: event skipped\n");
      cm_msg(MERROR,"FillEventBank","Raw mode with long waveforms, exceeding the limit: event skipped. Size dwords %d from module %d. Free space left %d dwords of %d bytes.", size_words, this->GetModuleID(), limit_size, V1725_MAX_EVENT_SIZE);
      *(src + 0) = 0xA0000004; // Event Size set to 0 data (4 DWORDS for the the header ==> TO be checked !)
			size_copied = 4;
		}
  } 

	// Mess with the bank structure; use bit 26 of word 2 to indicate if it is ZLE...
	if(this->IsZLEData()){
		uint32_t new_value = (src[1] | 0x4000000);
		src[1] = new_value;
	}

	// copy data over.
  memcpy(dest, src, size_copied*sizeof(uint32_t));

  this->DecrementNumEventsInRB(); //atomic
  rb_increment_rp(this->GetRingBufferHandle(), size_words*sizeof(uint32_t));

  //Close data bank
  bk_close(pevent, dest + size_copied);


  return true;

}



//
//--------------------------------------------------------------------------------
bool v1725CONET2::FillBufferLevelBank(char * pevent)
{
  if (! this->IsConnected()) {
    cm_msg(MERROR,"FillBufferLevelBank","Board %d disconnected", this->GetModuleID());
    return false;
  }

  DWORD *pdata, eStored, almostFull;
  int rb_level;
  char statBankName[5];
  CAENComm_ErrorCode sCAEN;

  snprintf(statBankName, sizeof(statBankName), "BL%02d", this->GetModuleID());
  bk_create(pevent, statBankName, TID_DWORD, (void **)&pdata);

  //Get v1725 buffer level
  sCAEN = ReadReg_(V1725_EVENT_STORED, &eStored);

  if (config.has_zle_firmware) {
    almostFull = 0;
  } else {
    sCAEN = ReadReg_(V1725RAW_ALMOST_FULL_LEVEL, &almostFull);
  }
  
  //Get ring buffer level
  rb_get_buffer_level(this->GetRingBufferHandle(), &rb_level);

  *pdata++ = eStored;
  /***
   * Note: There is no register in the v1725 indicating a busy
   * signal being output.  So we have to deduce it from the buffer
   * level and the almost full setting
   */
  int busy = 0;
  if(almostFull == 0){
    /* If the almost full register is set to 0,
     * the busy signal comes out only when all the
     * 1024 buffers are used */
		// PAA - for buffer Org 0xa
		// busy = (eStored == 1024) ? 1 : 0;
		// PAA - for buffer Org 0x7
    busy = (eStored == 128) ? 1 : 0;
  }
  else {
    busy = (eStored >= almostFull) ? 1 : 0;
  }
  *pdata++ = busy*500; //Make it 500 for better histogram display

  *pdata++ = rb_level;

  if(busy)
    printf(" %d(B)/%u ", eStored, rb_level);
  else
    printf(" %d/%u", eStored, rb_level);


  bk_close(pevent, pdata);


  return (sCAEN == CAENComm_Success);

}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Send a software trigger to the board
 *
 * Send a software trigger to the board.  May require
 * software triggers to be enabled in register 0x810C.
 *
 * \return  CAENComm Error Code (see CAENComm.h)
 */
bool v1725CONET2::SendTrigger()
{
  if (verbosity_) std::cout << GetName() << "::SendTrigger()" << std::endl;
  if (!IsConnected()) {
    cm_msg(MERROR,"SendTrigger","Board %d disconnected", this->GetModuleID());
    return false;
  }

  if (verbosity_) std::cout << "Sending Trigger (l,b) = (" << link_ << "," << board_ << ")" << std::endl;

  return (WriteReg(V1725_SW_TRIGGER, 0x1) == CAENComm_Success);
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Set the ODB record for this board
 *
 * Create a record for the board with settings from the configuration
 * string (v1725CONET2::config_str_board) if it doesn't exist or merge with
 * existing record. Create hotlink with callback function for when the
 * record is updated.  Get the handle to the record.
 *
 * Ex: For a frontend with index number 2 and board number 0, this
 * record will be created/merged:
 *
 * /Equipment/FEV1725I2/Settings/Board0
 *
 * \param   [in]  h        main ODB handle
 * \param   [in]  cb_func  Callback function to call when record is updated
 * \return  ODB Error Code (see midas.h)
 */
int v1725CONET2::SetBoardRecord(HNDLE h, void(*cb_func)(INT,INT,void*))
{
  char set_str[200];

  if(feIndex_ == -1)
    snprintf(set_str, sizeof(set_str), "/Equipment/V1725_Data/Settings/Board%d", moduleID_ % 8);
  else
    snprintf(set_str, sizeof(set_str), "/Equipment/V1725_Data%02d/Settings/Board%d", feIndex_, moduleID_ % 8);

  if (verbosity_) std::cout << GetName() << "::SetBoardRecord(" << h << "," << set_str << ",...)" << std::endl;
  int status,size;
  //create record if doesn't exist and find key
  status = db_check_record(h, 0, set_str, strcomb(config_str_board), TRUE);
  status = db_find_key(h, 0, set_str, &settings_handle_);
  if (status != DB_SUCCESS) {
    cm_msg(MINFO,"SetBoardRecord","Key %s not found. Return code: %d", set_str, status);
  }

  //hotlink
  size = sizeof(V1725_CONFIG_SETTINGS);
  status = db_open_record(h, settings_handle_, &config, size, MODE_READ, cb_func, NULL);
  if (status != DB_SUCCESS){
    cm_msg(MERROR,"SetBoardRecord","Couldn't create hotlink for %s. Return code: %d", set_str, status);
    return status;
  }

  //get actual record
  status = db_get_record(h, settings_handle_, &config, &size, 0);
  if (status != DB_SUCCESS){
    cm_msg(MERROR,"SetBoardRecord","Couldn't get record %s. Return code: %d", set_str, status);
    return status;
  }
  settings_loaded_ = true;
  settings_touched_ = true;

  return status; //== DB_SUCCESS for success
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Set the ODB record for history variable names
 *
 * \param   [in]  h        main ODB handle
 * \param   [in]  cb_func  Callback function to call when record is updated
 * \return  ODB Error Code (see midas.h)
 */
int v1725CONET2::SetHistoryRecord(HNDLE h, void(*cb_func)(INT,INT,void*))
{
  char settings_path[200] = "/Equipment/V1725_BufLvl/Settings/";
  char names_path[200];

  if(feIndex_ == -1)
    snprintf(settings_path, sizeof(settings_path), "/Equipment/V1725_BufLvl/Settings/");
  else
    snprintf(settings_path, sizeof(settings_path), "/Equipment/V1725_BufLvl%02d/Settings/", feIndex_);

//  if (verbosity_) std::cout << GetName() << "::SetHistoryRecord(" << h << "," << settings_path << ",...)" << std::endl;
  int status;//,size;

  HNDLE settings_key;
  status = db_find_key(h, 0, settings_path, &settings_key);

  if(status == DB_NO_KEY){
    db_create_key(h, 0, settings_path, TID_KEY);
    db_find_key(h, 0, settings_path, &settings_key);
  }

  char tmp[11];
  snprintf(tmp, sizeof(tmp), "Names BL%02d", this->moduleID_);
  strncpy(names_path, settings_path, sizeof(names_path));
  strncat(names_path, tmp, sizeof(names_path));

  db_create_key(h, 0, names_path, TID_STRING);
  HNDLE path_key;
  status = db_find_key(h, 0, names_path, &path_key);

  db_set_data(h, path_key, history_settings, sizeof(history_settings),
      sizeof(history_settings)/NAME_LENGTH, TID_STRING);

  if (status != DB_SUCCESS) cm_msg(MINFO,"SetHistoryRecord","Key %s not found", names_path);
  return status;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Initialize the hardware for data acquisition
 *
 * ### Initial setup:
 * - Set FP I/O Ctrl (0x811C) to default settings (output trigger).
 * - Do software reset + clear.
 * - Set up busy daisy chaining
 * - Put acquisition on stop.
 *
 * ### Checks
 * - AMC firmware version (and check that each channel has the same AMC firmware)
 * - ROC firmware version
 * - board type
 *
 * ### Set registers
 * Set parameters manually from ODB.
 *
 * \return  0 on success, -1 on error
 */
int v1725CONET2::InitializeForAcq()
{
  if (verbosity_) std::cout << GetName() << "::InitializeForAcq()" << std::endl;

  if (!settings_loaded_) {
    cm_msg(MERROR,"InitializeForAcq","Cannot call InitializeForAcq() without settings loaded properly on board %d", this->GetModuleID());
    return -1;
  }
  if (!IsConnected()){
    cm_msg(MERROR,"InitializeForAcq","Board %d disconnected", this->GetModuleID());
    return -1;
  }
  if (IsRunning()){
    cm_msg(MERROR,"InitializeForAcq","Board %d already started", this->GetModuleID());
    return -1;
  }
        
  CAENComm_ErrorCode sCAEN;
        
  // Do special board reset for lockup prevention                                                 
	DWORD reg;
  // Clear the board                                                         
	sCAEN = WriteReg_(V1725_SW_RESET, 0x1);
	
	// Need time for the PLL to lock
	ss_sleep(500);

	// Leave FP IO stuff disabled for now... will setup later...	
	// Clear done by accessing Buffer Origanization later on
	//  sCAEN = WriteReg_(V1725_SW_CLEAR, 0x1);
  
  // Set register V1725_FP_IO_CONTROL (0x811C) to default settings
  // (output trigger) will set the board that output the clock later
  sCAEN = WriteReg_(V1725_FP_IO_CONTROL, 0x00000000);
	usleep(200000);

	int size = sizeof(V1725_CONFIG_SETTINGS);
	db_get_record(odb_handle_, settings_handle_, &config, &size, 0);

	sCAEN = WriteReg_(V1725_FP_IO_CONTROL,        config.fp_io_ctrl);	                                                 
  sCAEN = WriteReg_(V1725_FP_LVDS_IO_CRTL,      config.fp_lvds_io_ctrl);

  std::stringstream ss_fw_datatype;
  ss_fw_datatype << "Module " << moduleID_ << ", ";
  // Firmware version check
  // read each AMC firmware version
  // [31:16] Revision date Y/M/DD
  // [15:8] Firmware Revision (X)
  // [7:0] Firmware Revision (Y)
  // eg 0x760C0103 is 12th June 07, revision 1.3
  int addr = 0;
  uint32_t version = 0;
  uint32_t prev_chan = 0;
  // Hardcode correct firmware verisons
	const uint32_t amc_fw_ver = 0x19010007;  // 0x14048c02; for ZLE
	const uint32_t roc_fw_ver = 0x17200410;  // 0x1331040c; for ZLE
  for(int iCh=0;iCh<16;iCh++) {
    addr = 0x108c | (iCh << 8);
    sCAEN = ReadReg_(addr, &version);
    if((iCh != 0) && (prev_chan != version)) {
      cm_msg(MERROR, "InitializeForAcq","Error Channels have different AMC Firmware ");
    }
    prev_chan = version;
  }
	//  cm_msg(MINFO,"feoV1725","Format: YMDD:XX.YY");

#ifndef NO_V1725
  if(version != amc_fw_ver)
    cm_msg(MERROR,"InitializeForAcq","Incorrect AMC Firmware Version: 0x%08x, 0x%08x expected", version, amc_fw_ver);
  else
#endif
    ss_fw_datatype << "AMC FW: 0x" << std::hex << version << ", ";

  // read ROC firmware revision
  // Format as above
  sCAEN = ReadReg_(V1725_ROC_FPGA_FW_REV, &version);
  switch (version)
  {
  case roc_fw_ver:
    ss_fw_datatype << "ROC FW: 0x" << std::hex << version << ", ";
    break;
  default:
#ifndef NO_V1725
    cm_msg(MERROR,"InitializeForAcq","Incorrect ROC Firmware Version: 0x%08x, 0x%08x expected", version, roc_fw_ver);
#endif
    break;
  }

  // Verify Board Type
  const uint32_t v1725_board_type = 0x0e;
  sCAEN = ReadReg_(V1725_BOARD_INFO, &version);
  if((version & 0xFF) != v1725_board_type)
    cm_msg(MINFO,"InitializeForAcq","*** WARNING *** Trying to use a v1725 frontend with another"
		" type of board (0x%x).   Results will be unexpected! ",version);

  // Record board type in the ODB.
  char rdb_str[200];

  if(feIndex_ == -1)
    snprintf(rdb_str, sizeof(rdb_str), "/Equipment/V1725_Data/Readback/Board%d/Board type", moduleID_ % 8);
  else
    snprintf(rdb_str, sizeof(rdb_str), "/Equipment/V1725_Data%02d/Readback/Board%d/Board type", feIndex_, moduleID_ % 8);

  db_set_value(odb_handle_, 0, rdb_str, &version, sizeof(version), 1, TID_DWORD);

//  ss_fw_datatype << this->GetChannelConfig();
  switch(this->GetDataType()){
  case RawPack2:
    ss_fw_datatype << "Raw Data";
    break;
  case ZLEPack2:
    ss_fw_datatype << "ZLE Data";
    break;
  case UnrecognizedDataFormat:
    ss_fw_datatype << "Unrecognized data format";
    break;
  default:
    /* Can't happen */
    break;
  }

	//PAA

  WriteReg_(V1725_ACQUISITION_CONTROL,     config.acq_mode);

  if (config.has_zle_firmware) {
	  WriteReg_(V1725_BOARD_CONFIG,               0); // Many fewer options.
    WriteReg_(V1725ZLE_RECORD_LENGTH,           config.custom_size);
    WriteReg_(V1725ZLE_PRE_TRIGGER_SETTING,     config.pre_trigger);
  } else {
	  WriteReg_(V1725_BOARD_CONFIG,               config.board_config);
    WriteReg_(V1725RAW_BUFFER_ORGANIZATION,     config.buffer_organization);
    WriteReg_(V1725RAW_CUSTOM_SIZE,             config.custom_size);
    WriteReg_(V1725RAW_POST_TRIGGER_SETTING,    config.post_trigger);
    WriteReg_(V1725RAW_ALMOST_FULL_LEVEL,       config.almost_full);
  }
	
	/* A bug exists in the firmware where if the channel mask is 0 (all channels
	 * disabled), the board misbehaves (reports bogus number of events in output
	 * buffer, event ready register doesn't work, etc).  Don't allow it     */
	if(!config.channel_mask){
		cm_msg(MERROR,"InitializeForAcq","The board misbehaves if channel mask is 0 (all channels disabled). Exiting...");
		return FE_ERR_HW;
	}
	
	WriteReg_(V1725_CHANNEL_EN_MASK,         config.channel_mask);
	WriteReg_(V1725_TRIG_SRCE_EN_MASK,       config.trigger_source);
	WriteReg_(V1725_FP_TRIGGER_OUT_EN_MASK,  config.trigger_output);
	WriteReg_(V1725_MONITOR_MODE,            0x3); // Buffer Occupancy mode;
	WriteReg_(V1725_BLT_EVENT_NB,            0x1); // TL? max number of events per BLT is 1?
	WriteReg_(V1725_VME_CONTROL,             V1725_ALIGN64);


	printf("..............................Now other settings...\n");
	//set specfic channel values

	usleep(200000);
#ifdef NO_V1725
	for (int iChan=0; iChan<8; iChan++) {
#else
	for (int iChan=0; iChan<16; iChan++) {
#endif

    if (config.has_zle_firmware) {
  		WriteReg_(V1725ZLE_CHANNEL_THRESHOLD + (iChan<<8), config.selftrigger_threshold[iChan]);
      WriteReg_(V1725ZLE_CHANNEL_LOGIC     + (iChan<<8), config.selftrigger_logic[iChan]);
      WriteReg_(V1725ZLE_ZS_NSAMP_BEFORE   + (iChan<<8), config.zle_bins_before[iChan]);
      WriteReg_(V1725ZLE_ZS_NSAMP_AFTER    + (iChan<<8), config.zle_bins_after[iChan]);
      WriteReg_(V1725ZLE_ZS_BASELINE       + (iChan<<8), config.zle_baseline[iChan]);

      DWORD thresh_comp = config.zle_signed_threshold[iChan] > 0 ? config.zle_signed_threshold[iChan] : (0x80000000 | (-1*config.zle_signed_threshold[iChan]));
      WriteReg_(V1725ZLE_ZS_THRESHOLD      + (iChan<<8), thresh_comp);

      // V1725ZLE_INPUT_CONTROL controls whether ZLE is enabled AND whether to trigger when
      // under thresh or over thresh. For RAW firmware, the polarity is defined in the
      // BOARD_CONFIG register, but that bit doesn't apply here. Instead we read the board
      // config parameter and apply the setting to INPUT_CONTROL.
      // Note that: raw; BOARD_CONFIG  bit 6; 0 => positive pulses
      //            zle; INPUT_CONTROL bit 7; 1 => positive pulses
      bool neg_pulses = (((config.board_config >> 6) & 0x1) == 1);
      DWORD input_control = 0;

      if (!neg_pulses) { 
        input_control |= (0x1 << 8);
      }
      if (!config.enable_zle) { 
        input_control |= (0x1 << 7);
      }

      WriteReg_(V1725ZLE_INPUT_CONTROL + (iChan<<8), input_control);
    } else {
  		WriteReg_(V1725RAW_CHANNEL_THRESHOLD + (iChan<<8), config.selftrigger_threshold[iChan]);
      WriteReg_(V1725RAW_CHANNEL_LOGIC     + (iChan<<8), config.selftrigger_logic[iChan]);
    }
		
		WriteReg_(V1725_DYNAMIC_RANGE       + (iChan<<8), config.dynamic_range_2v[iChan] ? 0 : 1);
		WriteReg_(V1725_CHANNEL_DAC         + (iChan<<8), config.dac[iChan]);	
	}		

	// Wait for 200ms after channing DAC offsets, before starting calibration. 
	usleep(200000);

#ifndef NO_V1725
	// Start the ADC calibration
  WriteReg_(V1725_ADC_CALIBRATION , 1);
	// Now we check to see when the calibration has finished.
	// by checking register 0x1n88.
	DWORD temp;
	for (int i=0;i<16;i++) {
		addr = V1725_CHANNEL_STATUS | (i << 8);
		ReadReg_(addr,&temp);
		//		printf("Channel (%i) %x Status: %x\n",i,addr,temp);
		if((temp & 0x4) == 0x4){
			printf("waiting for ADC calibration to finish...\n");
			int j;
			for(j =0; j < 20; i++){
				sleep(1);
				//				printf("temp %x\n",temp);
				ReadReg_(addr,&temp);
				if((temp & 0x4) == 0x0){
					break;
				}
			}
			if(j < 19){
				ReadReg_(addr,&temp);
				
				printf("Took %i seconds to finish calibration. calibration status: %x\n",j+1,(temp & 0x8));
			}else{
				cm_msg(MINFO, "InitializeForAcq", "ADC Calibration did not finish!");
			}					
		}else{
			//			printf("ADC calibration finished already\n");
		}
	}

	printf("Module[...] : ADC calibration finished already\n");

#endif	
	
	// Check finally for Acquisition status
	sCAEN = ReadReg_(0x8178, &reg);
	printf("Board error status 0x%x\n",reg);
	sCAEN = ReadReg_(0x8100, &reg);
	printf("Board acquisition control 0x%x\n",reg);
	
	sCAEN = ReadReg_(V1725_ACQUISITION_STATUS, &reg);  // 0x8104
	ss_fw_datatype << ", Acq Reg: 0x" << std::hex << reg;
	cm_msg(MINFO, "InitializeForAcq", ss_fw_datatype.str().c_str());
	
	if ((reg & 0x80) != 0x80) { // internal or external clock & PLL locked
		cm_msg(MERROR, "InitAcq", "Module %d (Link %d Board %d ) not initilized properly acq status:0x%x",  moduleID_, link_, board_, reg);
		return -1;
	}
	
  settings_touched_ = false;
  UNUSED(sCAEN);
	
  //ready to do start run
  return 0;
}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Get data type and ZLE configuration
 *
 * Finds the data type (raw/zle, pack2/2.5) from the channel
 * configuration (0x8000)
 *
 */
v1725CONET2::DataType v1725CONET2::GetDataType()
{
        
  // Set ZLE or Raw
  int dataType = ((config.board_config >> 11) & 0x1);
	if(config.enable_zle){
		data_type_ = ZLEPack2;
		printf("ZLEPack2 type: %x %x %x\n",dataType, config.board_config, ((config.board_config >> 16) & 0xF));
		return ZLEPack2;		
	}else{
		data_type_ = RawPack2;
		printf("RawPack2 type: %x %x %x\n",dataType, config.board_config, ((config.board_config >> 16) & 0xF));      
		return RawPack2;
	}

	printf("Unrecognized type: %x %x %x\n",dataType, config.board_config, ((config.board_config >> 16) & 0xF));
	return UnrecognizedDataFormat;

}

//
//--------------------------------------------------------------------------------
/**
 * \brief   Get ZLE setting
 *
 * Get the current ZLE setting from the channel configuration.
 *
 * \return  true if data is ZLE
 */
bool v1725CONET2::IsZLEData(){
  return ((data_type_ == ZLEPack2)||(data_type_ == ZLEPack25));
}

/* emacs
 * Local Variables:
 * mode:C
 * mode:font-lock
 * tab-width: 2
 * c-basic-offset: 2
 * End:
 */



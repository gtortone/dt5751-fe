/*****************************************************************************/
/**
\file v1725CONET2.hxx

## Contents

This file contains the class definition for the v1725 module driver.
 *****************************************************************************/

#ifndef V1725_HXX_INCLUDE
#define V1725_HXX_INCLUDE

#include <iostream>
#include <sstream>
#include <iomanip>
#include <assert.h>
#include <stdlib.h>
#include <sys/time.h>
#include <atomic>
#include <vector>

#include <CAENComm.h>
#include <CAENVMElib.h>
#include "ov1725drv.h"

#include "midas.h"
#include "msystem.h"

// Max event size we support (in bytes).
// Defined here as needed in v1725CONET2.cxx and feoV1725.cxx.
// Size required in bytes = wf_len_us * num_boards * chans_per_board * us_to_bytes_factor + space for other info
// E.g. for 1000us when reading 8 channels from 4 boards:
//   1000 * 4 * 8 * 437.5 + 10000 = 14,010,000
// If going very big, may also need to change /Experiment/MAX_EVENT_SIZE in ODB.
// 45MB/event is enough for 3ms with 4 boards * 8 channels
#define V1725_MAX_EVENT_SIZE 45000000

typedef unsigned short UShort_t;    //Unsigned Short integer 2 bytes (unsigned short)
typedef short          Short_t;     //Signed Short integer 2 bytes (unsigned short)
typedef float          Float_t;     //Float 4 bytes (float)
typedef unsigned char  Byte_t;      //Byte (8 bits) (unsigned char)
typedef unsigned int   UInt_t;      //Unsigned integer 4 bytes (unsigned int)
typedef int            Int_t;       //Signed integer 4 bytes (int)
typedef bool           Bool_t;      //Boolean (0=false, 1=true) (bool)

namespace TMath {
  inline Int_t Abs(Int_t d)
    { return std::abs(d); }

  inline Int_t Max(Int_t a, Int_t b)
     { return a >= b ? a : b; }
};

/**
 * Driver class for the v1725 module using the CAEN CONET2 (optical) interface.
 * Contains all the methods necessary to:
 *
 * - Connect/disconnect the board through an optical connection
 * - Initialize the hardware (set the registers) for data acquisition
 * - Read and write to the ODB
 * - Poll the hardware and read the event buffer into a midas bank
 * - Handle ZLE data
 * - Send a software trigger to the board if desired
 */
class v1725CONET2
{

public:

  /* Enums/structs */
  enum ConnectErrorCode {
    ConnectSuccess,
    ConnectErrorCaenComm,
    ConnectErrorTimeout,
    ConnectErrorAlreadyConnected
  };
  enum DataType {
    RawPack2,                //!< 0: Full data, 2 packing
    RawPack25,               //!< 1: Full data, 2.5 packing
    ZLEPack2,                //!< 2: ZLE data, 2 packing
    ZLEPack25,               //!< 3: ZLE data, 2.5 packing
    UnrecognizedDataFormat
  };
  struct V1725_CONFIG_SETTINGS {
    BOOL      enable;
    BOOL      has_zle_firmware;         //!< Some registers only valid for ZLE or non-ZLE FW
    INT       acq_mode;                 //!< 0x8100@[ 1.. 0]
    DWORD     board_config;             //!< 0x8000@[19.. 0]
    INT       buffer_organization;      //!< 0x800C@[ 3.. 0] - NON-ZLE only
    INT       custom_size;              //!< 0x8020@[31.. 0] (known as record length for ZLE)
    DWORD     channel_mask;             //!< 0x8120@[ 7.. 0]
    DWORD     trigger_source;           //!< 0x810C@[31.. 0]
    DWORD     trigger_output;           //!< 0x8110@[31.. 0]
    DWORD     post_trigger;             //!< 0x8114@[31.. 0] - NON-ZLE only
    DWORD     pre_trigger;              //!< 0x8038@[31.. 0] - ZLE only
    DWORD     fp_io_ctrl;               //!< 0x811C@[31.. 0]
    BOOL      enable_zle;               //!< 0x8064@[31.. 0] - ZLE only
    DWORD     almost_full;              //!< 0x816C@[31.. 0] - NON-ZLE only
    DWORD     fp_lvds_io_ctrl;          //!< 0x81A0@[31.. 0]
    DWORD     selftrigger_threshold[16];//!< 0x1n80@[11.. 0] for NON-ZLE / 0x1n60@[11.. 0] for ZLE
    DWORD     selftrigger_logic[8];     //!< 0x1n84@[11.. 0] for NON-ZLE / 0x1n68@[11.. 0] for ZLE
    INT       zle_signed_threshold[16]; //!< 0x1n5C@[31.. 0] - ZLE only
    DWORD     zle_bins_before[16];      //!< 0x1n54@[31.. 16]- ZLE only
    DWORD     zle_bins_after[16];       //!< 0x1n58@[15.. 0] - ZLE only
    DWORD     zle_baseline[16];         //!< 0x1n34@[31.. 0] - ZLE only
    DWORD     dac[16];                  //!< 0x1n98@[15.. 0]
    BOOL      dynamic_range_2v[16];     //!< 0x1n28@[0]
    float     sw_trig_rate_Hz;          //!< Software-only
  } config; //!< instance of config structure

  /* Static */
  static const char *config_str_board[]; //!< Configuration string for this board
  static const char history_settings[][NAME_LENGTH];

  /* Constructor/Destructor */
  v1725CONET2(int feindex, int link, int board, int moduleID, HNDLE hDB);
  /* Use move instead of copy semantics as we only need one copy of each
   * object (C++11).  See notes in implementation. */
  v1725CONET2(v1725CONET2&&) noexcept;
  v1725CONET2& operator=(v1725CONET2&&) noexcept;
  ~v1725CONET2();

  /* Public methods */
  ConnectErrorCode Connect();
  ConnectErrorCode Connect(int, int);
  static void * connectThread(void *);
  struct thread_args {
    v1725CONET2 * v1725;
    CAENComm_ErrorCode * errcode;
    pthread_cond_t * cv;
  };
  std::string connectStatusMsg;
  bool Disconnect();
  bool StartRun();
  bool StopRun();
  bool IsConnected();
  bool IsEnabled() { return config.enable; }
  bool IsRunning();
  bool ReadReg(DWORD, DWORD*);
  bool WriteReg(DWORD, DWORD);
  bool CheckEvent();
  bool ReadEvent(void *);
  bool FillEventBank(char *, uint32_t &timestamp);
  bool FillBufferLevelBank(char *);
  bool IsZLEData();

  void IssueSwTrigIfNeeded();
  bool SendTrigger();
  bool Poll(DWORD*);
  int SetBoardRecord(HNDLE h, void(*cb_func)(INT,INT,void*));
  int SetHistoryRecord(HNDLE h, void(*cb_func)(INT,INT,void*));
  int InitializeForAcq();

  void FindPulses(bool firstBlock, int closePulseLookAhead, int closePulseNearEndOfBlock, bool extendPulseEnd, bool enableSubPeaks, bool enablePulseSplitting);

  /* Getters/Setters */
  int GetModuleID() { return moduleID_; } //!< returns unique module ID
  int GetLink() { return link_; }         //!< returns optical link number
  int GetBoard() { return board_; }       //!< returns board number
  int GetFEIndex() { return feIndex_; }   //!< returns frontend index
  std::string GetName();
  int GetDeviceHandle() {
    return device_handle_;                //! returns physical device handle
  }
  HNDLE GetODBHandle() {                  //! returns main ODB handle
    return odb_handle_;
  }
  HNDLE GetSettingsHandle() {             //! returns settings record handle
    return settings_handle_;
  }
  bool GetSettingsTouched() {             //! returns true if odb settings  touched
    return settings_touched_;
  }
  void SetSettingsTouched(bool t) {       //! set _settings_touched
    settings_touched_ = t;
  }
  void SetRingBufferHandle(int rb_handle) { //! set ring buffer index
    rb_handle_ = rb_handle;
  }
  int GetRingBufferHandle() {             //! returns ring buffer index
    return rb_handle_;
  }
  int GetNumEventsInRB() {                //! returns number of events in ring buffer
    return num_events_in_rb_.load();
  }
  int PeekRBEventID();
  DWORD PeekRBTimestamp();
  DataType GetDataType();
  int GetVerbosity(){
    return verbosity_;
  }
  void SetVerbosity(int verbosity){
    verbosity_ = verbosity;
  }

  /* These are atomic with sequential memory ordering. See below */
  void IncrementNumEventsInRB() {         //! Increment Number of events in ring buffer
    num_events_in_rb_++;
  }
  void DecrementNumEventsInRB() {         //! Decrement Number of events in ring buffer
    num_events_in_rb_--;
  }
  void ResetNumEventsInRB() {             //! Reset Number of events in ring buffer
    num_events_in_rb_=0;
  }

private:

  /* Private fields */

  /* IMPORTANT
   *
   * If adding additional fields, do NOT forget to change the move constructor
   * and move assignment operator accordingly
   */
  int feIndex_,           //!< Frontend index number
  link_,                  //!< Optical link number
  board_,                 //!< Module/Board number
  moduleID_;              //!< Unique module ID

  int device_handle_;     //!< physical device handle
  HNDLE odb_handle_;      //!< main ODB handle
  HNDLE settings_handle_; //!< Handle for the device settings record
  int rb_handle_;         //!< Handle to ring buffer
  bool settings_loaded_;  //!< ODB settings loaded
  bool settings_touched_; //!< ODB settings touched
  bool running_;          //!< Run in progress
  DataType data_type_;    //!< Data type for all channels:
  int verbosity_;         //!< Make the driver verbose
                          //!< 0: off
                          //!< 1: normal
                          //!< 2: very verbose
  /* We use an atomic types here to get lock-free (no pthread mutex lock or spinlock)
   * read-modify-write. operator++(int) and operator++() on an atomic<integral> use
   * atomic::fetch_add() and operator--(int) and operator--() use atomic::fetch_sub().
   * The default memory ordering for these functions is memory_order_seq_cst (sequentially
   * consistent). This saves us from inserting a memory barrier between read/write pointer
   * incrementation and an increment/decrement of this variable.   */
  std::atomic<int> num_events_in_rb_;  //!< Number of events stored in ring buffer

  timeval last_sw_trig_time;

  Bool_t kFALSE = false;

    /* Private methods */
  CAENComm_ErrorCode AcqCtl_(uint32_t);
  CAENComm_ErrorCode WriteChannelConfig_(uint32_t);
  CAENComm_ErrorCode ReadReg_(DWORD, DWORD*);
  CAENComm_ErrorCode WriteReg_(DWORD, DWORD);
};

#endif // V1725_HXX_INCLUDE


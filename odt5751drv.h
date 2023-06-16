/*********************************************************************

  Name:         odt5751drv.h
  Created by:   T. Lindner
                    implementation of the CAENCommLib functions
  Contents:     dt5751 16-channel 250 MHz 12-bit ADC  (standard RAW firmware)

  $Id$
                
*********************************************************************/
#ifndef  ODT5751DRV_INCLUDE_H
#define  ODT5751DRV_INCLUDE_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "CAENComm.h"
#include "dt5751Raw.h"

CAENComm_ErrorCode odt5751_ChannelSet(int handle, uint32_t channel, uint32_t what, uint32_t that);
CAENComm_ErrorCode odt5751_ChannelGet(int handle, uint32_t channel, uint32_t what, uint32_t *data);
CAENComm_ErrorCode odt5751_ChannelGet(int handle, uint32_t channel, uint32_t what, uint32_t *data);
CAENComm_ErrorCode odt5751_ChannelThresholdSet(int handle, uint32_t channel, uint32_t threshold);
CAENComm_ErrorCode odt5751_ChannelDACSet(int handle, uint32_t channel, uint32_t dac);
CAENComm_ErrorCode odt5751_ChannelDACGet(int handle, uint32_t channel, uint32_t *dac);
CAENComm_ErrorCode odt5751_AcqCtl(int handle, uint32_t operation);
CAENComm_ErrorCode odt5751_ChannelConfig(int handle, uint32_t operation);
CAENComm_ErrorCode odt5751_info(int handle, int *nchannels, uint32_t *data);
CAENComm_ErrorCode odt5751_BufferOccupancy(int handle, uint32_t channel, uint32_t *data);
CAENComm_ErrorCode odt5751_Status(int handle);
CAENComm_ErrorCode odt5751_Setup(int handle, int mode);


#endif // ODT5751DRV_INCLUDE_H

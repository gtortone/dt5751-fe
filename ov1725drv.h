/*********************************************************************

  Name:         ov1725drv.h
  Created by:   T. Lindner
                    implementation of the CAENCommLib functions
  Contents:     v1725 16-channel 250 MHz 12-bit ADC

  $Id$
                
*********************************************************************/
#ifndef  OV1725DRV_INCLUDE_H
#define  OV1725DRV_INCLUDE_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "CAENComm.h"
#include "v1725.h"

CAENComm_ErrorCode ov1725_ChannelSet(int handle, uint32_t channel, uint32_t what, uint32_t that);
CAENComm_ErrorCode ov1725_ChannelGet(int handle, uint32_t channel, uint32_t what, uint32_t *data);
CAENComm_ErrorCode ov1725_ChannelGet(int handle, uint32_t channel, uint32_t what, uint32_t *data);
CAENComm_ErrorCode ov1725_ChannelThresholdSet(int handle, uint32_t channel, uint32_t threshold);
CAENComm_ErrorCode ov1725_ChannelDACSet(int handle, uint32_t channel, uint32_t dac);
CAENComm_ErrorCode ov1725_ChannelDACGet(int handle, uint32_t channel, uint32_t *dac);
CAENComm_ErrorCode ov1725_AcqCtl(int handle, uint32_t operation);
CAENComm_ErrorCode ov1725_ChannelConfig(int handle, uint32_t operation);
CAENComm_ErrorCode ov1725_info(int handle, int *nchannels, uint32_t *data);
CAENComm_ErrorCode ov1725_BufferOccupancy(int handle, uint32_t channel, uint32_t *data);
CAENComm_ErrorCode ov1725_Status(int handle);
CAENComm_ErrorCode ov1725_Setup(int handle, int mode);


#endif // OV1725DRV_INCLUDE_H

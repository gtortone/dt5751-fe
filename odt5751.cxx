/*********************************************************************

  Name:         odt5751.cxx
  Created by:   Thomas Lindner; based on V1720
                    implementation of the CAENCommLib functions
  Contents:     DT5751 16 ch. 12bit 250Msps for Optical link
  MAIN_ENABLE Build : odt5751.c, dt5751.h, odt5751drv.h
   > gcc -g -O2 -Wall -DDO_TIMING -DMAIN_ENABLE -o odt5751.exe odt5751.c -lCAENComm -lrt 
  Operation:
  > ./odt5751 -l 100 -l 0 -b 0
  > ./odt5751 -l 10000 -m 100 -l 1 -b 0

  $Id$
*********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "odt5751drv.h"

#define LARGE_NUMBER 10000000
// Buffer organization map for number of samples
uint32_t DT5751_NSAMPLES_MODE[11] = { (1<<20), (1<<19), (1<<18), (1<<17), (1<<16), (1<<15)
              ,(1<<14), (1<<13), (1<<12), (1<<11), (1<<10)};

/*****************************************************************/
CAENComm_ErrorCode odt5751_ChannelSet(int handle, uint32_t channel, uint32_t what, uint32_t that)
{
  uint32_t reg, mask;

  if (what == DT5751RAW_CHANNEL_THRESHOLD)   mask = 0x0FFF;
  if (what == DT5751_CHANNEL_DAC)         mask = 0xFFFF;
  reg = what | (channel << 8);
  return CAENComm_Write32(handle, reg, (that & 0xFFF));
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_ChannelGet(int handle, uint32_t channel, uint32_t what, uint32_t *data)
{
  uint32_t reg, mask;

  if (what == DT5751RAW_CHANNEL_THRESHOLD)   mask = 0x0FFF;
  if (what == DT5751_CHANNEL_DAC)         mask = 0xFFFF;
  reg = what | (channel << 8);
  return CAENComm_Read32(handle, reg, data);
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_ChannelThresholdSet(int handle, uint32_t channel, uint32_t threshold)
{
  uint32_t reg;
  reg = DT5751RAW_CHANNEL_THRESHOLD | (channel << 8);
  printf("reg:0x%x, threshold:%x\n", reg, threshold);
  return CAENComm_Write32(handle, reg,(threshold & 0xFFF));
}


/*****************************************************************/
CAENComm_ErrorCode odt5751_ChannelDACSet(int handle, uint32_t channel, uint32_t dac)
{
  uint32_t reg, status, ncount;
  
  if ((channel >= 0) && (channel < 8)) {
    reg = DT5751_CHANNEL_STATUS | (channel << 8);
    ncount = 10000;
    do {
      CAENComm_Read32(handle, reg, &status);
    } while ((status & 0x04) && (ncount--));
    if (ncount == 0) return (CAENComm_ErrorCode) -1;
    reg = DT5751_CHANNEL_DAC | (channel << 8);
    printf("reg:0x%x, DAC:%x\n", reg, dac);
    return CAENComm_Write32(handle, reg, (dac & 0xFFFF));
  }
  return (CAENComm_ErrorCode) -1;
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_ChannelDACGet(int handle, uint32_t channel, uint32_t *dac)
{
  uint32_t reg;
  CAENComm_ErrorCode sCAEN = (CAENComm_ErrorCode) -1;

  if ((channel >= 0) && (channel < 8)) {
    reg = DT5751_CHANNEL_DAC | (channel << 8);
    sCAEN = CAENComm_Read32(handle, reg, dac);
  }
  return sCAEN;
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_AcqCtl(int handle, uint32_t operation)
{
  uint32_t reg;
  CAENComm_ErrorCode sCAEN;
  
  sCAEN = CAENComm_Read32(handle, DT5751_ACQUISITION_CONTROL, &reg);
  //  printf("sCAEN:%d ACQ Acq Control:0x%x\n", sCAEN, reg);

  switch (operation) {
  case DT5751_RUN_START:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, (reg | 0x4));
    break;
  case DT5751_RUN_STOP:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, (reg & ~( 0x4)));
    break;
  case DT5751_REGISTER_RUN_MODE:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, 0x0);
    break;
  case DT5751_SIN_RUN_MODE:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, 0x1);
    break;
  case DT5751_SIN_GATE_RUN_MODE:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, 0x2);
    break;
  case DT5751_MULTI_BOARD_SYNC_MODE:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, 0x3);
    break;
  case DT5751_COUNT_ACCEPTED_TRIGGER:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, (reg & ~( 0x8)));
    break;
  case DT5751_COUNT_ALL_TRIGGER:
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL, (reg | 0x8));
    break;
  default:
    printf("operation not defined\n");
    break;
  }
  return sCAEN;
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_ChannelConfig(int handle, uint32_t operation)
{
  CAENComm_ErrorCode sCAEN;
  uint32_t reg, cfg;
  
  sCAEN = CAENComm_Read32(handle, DT5751_BOARD_CONFIG, &reg);  
  sCAEN = CAENComm_Read32(handle, DT5751_BOARD_CONFIG, &cfg);  
  //  printf("Channel_config1: 0x%x\n", cfg);  

  switch (operation) {
  case DT5751_TRIGGER_UNDERTH:
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CFG_BIT_SET, 0x40);
    break;
  case DT5751_TRIGGER_OVERTH:
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CFG_BIT_CLR, 0x40);
    break;
  case DT5751_PACK25_ENABLE:
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CONFIG, (reg | 0x800));
    break;
  case DT5751_PACK25_DISABLE:
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CONFIG, (reg & ~(0x800)));
    break;
  case DT5751_NO_ZERO_SUPPRESSION:
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CONFIG, (reg & ~(0xF000)));
    break;
  case DT5751_ZLE:
    reg &= ~(0xF000);
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CONFIG, (reg | 0x2000));
    break;
  case DT5751_ZS_AMP:
    reg &= ~(0xF000);
    sCAEN = CAENComm_Write32(handle, DT5751_BOARD_CONFIG, (reg | 0x3000));
    break;
  default:
    break;
  }
  sCAEN = CAENComm_Read32(handle, DT5751_BOARD_CONFIG, &cfg);  
  //  printf("Channel_config2: 0x%x\n", cfg);
  return sCAEN;
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_info(int handle, int *nchannels, uint32_t *data)
{
  CAENComm_ErrorCode sCAEN;
  int i, chanmask;
  uint32_t reg;

  // Evaluate the event size
  // Number of samples per channels
  sCAEN = CAENComm_Read32(handle, DT5751RAW_BUFFER_ORGANIZATION, &reg);  
  *data = DT5751_NSAMPLES_MODE[reg];

  // times the number of active channels
  sCAEN = CAENComm_Read32(handle, DT5751_CHANNEL_EN_MASK, &reg);  
  chanmask = 0xf & reg; 
  *nchannels = 0;
  for (i=0;i<4;i++) {
    if (chanmask & (1<<i))
      *nchannels += 1;
  }

  *data *= *nchannels;
  *data /= 2;   // 2 samples per 32bit word
  *data += 4;   // Headers
  return sCAEN;
}

/*****************************************************************/
CAENComm_ErrorCode odt5751_Status(int handle)
{
  CAENComm_ErrorCode sCAEN;
  uint32_t reg;

  printf("================================================\n");
  sCAEN = CAENComm_Read32(handle, DT5751_BOARD_ID, &reg);  
  printf("Board ID             : 0x%x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_BOARD_INFO, &reg);  
  printf("Board Info           : 0x%x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_ACQUISITION_CONTROL, &reg);  
  printf("Acquisition control  : 0x%8.8x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_ACQUISITION_STATUS, &reg);  
  printf("Acquisition status         : 0x%8.8x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_CHANNEL_EN_MASK, &reg);  
  printf("Channel mask               : 0x%5.5x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_BOARD_CONFIG, &reg);  
  printf("Board Configuration        : 0x%5.5x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_TRIG_SRCE_EN_MASK, &reg);  
  printf("Trigger Source Enable Mask : 0x%8.8x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_READOUT_STATUS, &reg);  
  printf("VME Status                 : 0x%x\n", reg);
  sCAEN = CAENComm_Read32(handle, DT5751_EVENT_STORED, &reg);  
  printf("Event Stored               : 0x%8.8x\n", reg);
  for(int i=0; i<4; i++) {
    sCAEN = CAENComm_Read32(handle, DT5751_CHANNEL_STATUS + (i * 0x100), &reg);
    printf("Channel %d status (0x%X)        : 0x%8.8x\n", i, DT5751_CHANNEL_STATUS + (i * 0x100), reg);
    sCAEN = CAENComm_Read32(handle, DT5751_CHANNEL_CONFIG + (i * 0x100), &reg);
    printf("Channel %d config (0x%X)        : 0x%8.8x\n", i, DT5751_CHANNEL_CONFIG + (i * 0x100), reg);
  }
  printf("================================================\n");
  return sCAEN;
}

/*****************************************************************/
/**
Sets all the necessary paramters for a given configuration.
The configuration is provided by the mode argument.
Add your own configuration in the case statement. Let me know
your setting if you want to include it in the distribution.
- <b>Mode 1</b> : 

@param *mvme VME structure
@param  base Module base address
@param mode  Configuration mode number
@return 0: OK. -1: Bad
*/
CAENComm_ErrorCode  odt5751_Setup(int handle, int mode)
{
  CAENComm_ErrorCode sCAEN;
  switch (mode) {
  case 0x0:
    printf("--------------------------------------------\n");
    printf("Setup Skip\n");
    printf("--------------------------------------------\n");
  case 0x1:
    printf("--------------------------------------------\n");
    printf("Trigger from FP, 8ch, 1Ks, postTrigger 800\n");
    printf("--------------------------------------------\n");
    sCAEN = CAENComm_Write32(handle, DT5751RAW_BUFFER_ORGANIZATION,   0x0A);    // 1K buffer
    sCAEN = CAENComm_Write32(handle, DT5751_TRIG_SRCE_EN_MASK,     0x4000);  // External Trigger
    sCAEN = CAENComm_Write32(handle, DT5751_CHANNEL_EN_MASK,       0xFF);    // 8ch enable
    sCAEN = CAENComm_Write32(handle, DT5751RAW_POST_TRIGGER_SETTING,  800);     // PreTrigger (1K-800)
    sCAEN = CAENComm_Write32(handle, DT5751_ACQUISITION_CONTROL,   0x00);    // Reset Acq Control
    printf("\n");
    break;
  case 0x2:
    printf("--------------------------------------------\n");
    printf("Trigger from LEMO\n");
    printf("--------------------------------------------\n");
    sCAEN = CAENComm_Write32(handle, DT5751RAW_BUFFER_ORGANIZATION, 1);
    printf("\n");
    break;
  default:
    printf("Unknown setup mode\n");
    return (CAENComm_ErrorCode)-1;
  }
  return odt5751_Status(handle);
}

/*****************************************************************/
/*-PAA- For test purpose only */
#ifdef MAIN_ENABLE
int main (int argc, char* argv[]) {

  /* Lock the process to an arbitrary core (#3)
  int mask = 0x04;
  if( sched_setaffinity(0, sizeof(mask), &mask) < 0 )
  {
    printf("ERROR: affinity not set\n");
  }
  */

  CAENComm_ErrorCode sCAEN;
  int handle[2];
  int nw, l=0, c=0, d=0, h=0, Nh;
  uint32_t i, lcount, data[50000], temp, lam, reg;
  int Nmodulo=10;
  int tcount=0, eloop=0;
  uint32_t   *pdata, eStored, eSize;
  int loop, Nloop=10;
  int bshowData=0;
  int debug = 0;
  uint32_t pct=0, ct;
  struct timeval t1;
  int   dt1, savelcount=0;
  // Added to test optivca communication (Alex 26/02/12)
  int testCom    = 0;
  uint32_t regRd = 0;

   /* get parameters */
   /* parse command line parameters */
  for (i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == 'd')
      debug = 1;
    else if (strncmp(argv[i], "-s", 2) == 0)
      bshowData = 1;
    else if (strncmp(argv[i], "-t", 2) == 0)
      testCom = 1;
    else if (argv[i][0] == '-') {
      if (i + 1 >= argc || argv[i + 1][0] == '-')
	goto usage;
      if (strncmp(argv[i], "-l", 2) == 0)
	Nloop =  (atoi(argv[++i]));
      else if (strncmp(argv[i], "-o", 2) == 0)
	l =  (atoi(argv[++i]));
      else if (strncmp(argv[i], "-b", 2) == 0)
	d =  (atoi(argv[++i]));
      else if (strncmp(argv[i], "-c", 2) == 0)
	c =  (atoi(argv[++i]));
      else if (strncmp(argv[i], "-m", 2) == 0)
	Nmodulo =  (atoi(argv[++i]));
      else if (strncmp(argv[i], "-d", 2) == 0)
	d =  (atoi(argv[++i]));
    } else {
    usage:
      printf("usage: odt5751 -l (loop count) \n");
      printf("              -o link#\n");
      printf("              -b board#\n");
      printf("              -c interface# (PCIe)\n");
      printf("              -d daisy#\n");
      printf("              -m modulo display\n");
      printf("              -s show data\n");
      printf("              -t test communication\n\n");
      return 0;
         }
  }
  
  //  printf("in odt5751, l %d, d %d, c %d\n", l, d, c);
  
#if 1

  // Test board communication (Alex 26/02/12)
  if(testCom == 1) {
    // Open devices
    sCAEN = CAENComm_OpenDevice(CAENComm_PCIE_OpticalLink, l, d, c, &(handle[h])); 
    if (sCAEN != CAENComm_Success) {
      sCAEN = CAENComm_CloseDevice(handle[h]); 
      printf("Com Test Fail Type One\n");
      return -1;
    } else {
      sCAEN = CAENComm_Read32(handle[h], DT5751_BOARD_INFO, &regRd);
      printf("O:%d B:%d Rev:0x%x FPGA-FW",l, d, regRd);
      for (i=0;i<1;i++) {
	reg = DT5751_FPGA_FWREV | (i << 8);
	sCAEN = CAENComm_Read32(handle[h], reg, &regRd);
	printf("/0x%x", regRd);
      }
      printf(" Com Test Success \n");
      /*
      if((regRd & 0xffff) != 0x1003) {
	sCAEN = CAENComm_CloseDevice(handle[h]); 
	printf("Com Test Fail Type Two\n");
	return -1;
      }
      */
    }
    sCAEN = CAENComm_CloseDevice(handle[h]); 
    return 0;
  }

  //
  // Open devices
  sCAEN = CAENComm_OpenDevice(CAENComm_PCIE_OpticalLink, l, d, c, &(handle[h])); 
  if (sCAEN != CAENComm_Success) {
    handle[h] = -1;
    printf("1st CAENComm_OpenDevice [l:%d, d:%d]: Error %d\n", l, d, sCAEN);
  } else {
    printf("1st Device found : Interface:%d Link:%d  Daisy:%d Handle[%d]:%d\n", c, l, d, h, handle[h]);
    sCAEN = odt5751_Status(handle[h]);
    h++;
  }
  Nh = h;
  printf("Handles opened (%d)\n", Nh);

  CAENComm_CloseDevice(handle[0]); 
  printf("Init Handles released\n");

  //
  // Open devices
  h = 0;
  sCAEN = CAENComm_OpenDevice(CAENComm_PCIE_OpticalLink, l, d, c, &(handle[h])); 
  if (sCAEN != CAENComm_Success) {
    handle[h] = -1;
    printf("2nd CAENComm_OpenDevice [l:%d, d:%d]: Error %d\n", l, d, sCAEN);
  } else {
    printf("2nd Device found : Interface:%d Link:%d  Daisy:%d Handle[%d]:%d\n", c, l, d, h, handle[h]);
    sCAEN = odt5751_Status(handle[h]);
    h++;
  }
  Nh = h;
  //sCAEN = odt5751_AcqCtl(handle[0], DT5751_RUN_STOP);
  //sCAEN = CAENComm_Write32(handle[0], DT5751_SW_CLEAR, 0);
    sCAEN = CAENComm_Write32(handle[0], DT5751_SW_RESET, 0);
  printf("Init After stop\n");

#endif
#if 0
  //
  // Open devices
  for (h=0, l=0;l<1;l++) {
    for (d=0;d<2;d++) {
      // Open VME interface   
      sCAEN = CAENComm_OpenDevice(CAENComm_PCIE_OpticalLink, l, d, c, &(handle[h])); 
      if (sCAEN != CAENComm_Success) {
	handle[h] = -1;
	printf("CAENComm_OpenDevice [l:%d, d:%d]: Error %d\n", l, d, sCAEN);
      } else {
	printf("Device found : Link:%d  Daisy:%d Handle[%d]:%d\n", l, d, h, handle[h]);
	sCAEN = odt5751_Status(handle[h]);
	h++;
      }
    }
  }
  Nh = h;
  printf("Handles opened (%d)\n", Nh);
#endif
#if 1
  for (h=0;h<Nh;h++) {
    
    ////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    // sCAEN = CAENComm_Write32(handle[h], DT5751_SW_RESET              , 0);
    ////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////
    
    sCAEN = odt5751_AcqCtl(handle[h], 0x3);
    sCAEN = CAENComm_Write32(handle[h], DT5751_BOARD_CONFIG        , 0x10);
    sCAEN = CAENComm_Write32(handle[h], DT5751_BUFFER_ORGANIZATION   , 0xa);
    sCAEN = CAENComm_Write32(handle[h], DT5751_CHANNEL_EN_MASK       , 0x3);
    sCAEN = CAENComm_Write32(handle[h], DT5751_TRIG_SRCE_EN_MASK     , 0x40000000);
    sCAEN = CAENComm_Write32(handle[h], DT5751_MONITOR_MODE          , 0x3);  // buffer occupancy
    // 
    // Set Channel threshold
    for (i=0;i<8;i++) {
      odt5751_ChannelSet(handle[h], i, DT5751_CHANNEL_THRESHOLD, 0x820);
      sCAEN = odt5751_ChannelGet(handle[h], i, DT5751_CHANNEL_THRESHOLD, &temp);
      printf("Board: %d Threshold[%i] = %d \n", h, i, temp);
    }

    //
    // Set DAC value
    for (i=0;i<8;i++) {
      sCAEN = odt5751_ChannelDACSet(handle[h], i, 0x88b8);
      sCAEN = odt5751_ChannelDACGet(handle[h], i, &temp);
      printf("Board :%d DAC[%i] = %d \n", h, i, temp);
    }
    
  }
  printf("Modules configured\n");


  // Start boards
  for (h=0;h<Nh;h++) {
    if (handle[h] < 0) continue;   // Skip unconnected board
    // Start run then wait for trigger
    sCAEN = CAENComm_Write32(handle[h], DT5751_SW_CLEAR, 0);
    sleep(1);
    odt5751_AcqCtl(handle[h], DT5751_RUN_START);
  }  
  
  printf("Modules started\n");
 
  for (loop=0;loop<Nloop;loop++) {
    do {
      sCAEN = CAENComm_Read32(handle[0], DT5751_ACQUISITION_STATUS, &lam);
      //printf("lam:0x%x, sCAEN:%d\n",lam, sCAEN);
      lam &= 0x8;
    } while (lam == 0);

    // Read all modules
    for (h=0; h<Nh; h++) {
      // Skip unconnected devices
      if (handle[h] < 0) continue;   // Skip unconnected board
      
      // Check if data ready
      lcount=LARGE_NUMBER;
      do {
	sCAEN = CAENComm_Read32(handle[h], DT5751_VME_STATUS, &lam);
	lam &= 0x1;
      } while ((lam==0) && (lcount--));
      if (h==1) savelcount = LARGE_NUMBER - lcount;
      if (lcount == 0) {
	printf("timeout on LAM for module %d\n", h);
	goto out;
      }
      // buffer info
      sCAEN = CAENComm_Read32(handle[h], DT5751_EVENT_STORED, &eStored);
      sCAEN = CAENComm_Read32(handle[h], DT5751_EVENT_SIZE, &eSize);
      
      // Some info display
      if ((h==0) && ((loop % Nmodulo) == 0)) {
	gettimeofday(&t1, NULL);
	ct = t1.tv_sec * 1e6 + t1.tv_usec;
	dt1 = ct-pct;
	pct = ct;
	printf("B:%02d Hndle:%d sCAEN:%d Evt#:%d Event Stored:0x%x Event Size:0x%x try:%d KB/s:%6.2f BLTl:%d\n"
	       , h, handle[h], sCAEN, loop, eStored, eSize, savelcount, (float) 1e3*tcount/dt1, eloop);
	tcount = 0;
      }
      
      // Read data
      pdata = &data[0];
      eloop = 0;
      do {
	sCAEN = CAENComm_BLTRead(handle[h], DT5751_EVENT_READOUT_BUFFER, pdata, eSize < 1028 ? eSize : 1028, &nw);
	eSize -= nw;
	pdata += nw;
	tcount += nw;  // debugging
	eloop++;       // debugging
      } while (eSize);
      
      if (bshowData) printf("Module:%d nw:%d data: 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n"
			    ,h , nw, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
    } // loop over Nh
    
    //    printf("Modules all readout\n");
  }

#endif

  // Exit port
 out:
  for (h=0;h<Nh;h++) {
    sCAEN = odt5751_AcqCtl(handle[h], DT5751_RUN_STOP);
  }
  printf("Modules stopped\n");

  for (h=0;h<Nh;h++) {
    sCAEN = CAENComm_CloseDevice(handle[h]); 
  }
  printf("Handles released\n");

  return 1;

}
#endif

/* emacs
 * Local Variables:
 * mode:C
 * mode:font-lock
 * tab-width: 8
 * c-basic-offset: 2
 * End:
 */

//end

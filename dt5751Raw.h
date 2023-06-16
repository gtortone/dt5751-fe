/***************************************************************************/
/*                                                                         */
/*  Filename: dt5751Raw.h                                                  */
/*                                                                         */
/*  Function: headerfile for DT5751 (RAW or DPP-ZLE firmware).             */
/*                                                                         */
/*  Names starting with: DT5751RAW_ are only valid for Raw firmware        */
/*                       DT5751ZLE_ are only valid for DPP-ZLE firmware    */
/*                       DT5751_    are valid for both Raw and DPP-ZLE     */
/*                                                                         */
/* ----------------------------------------------------------------------- */
/* $Id$                                                                    */
/***************************************************************************/

#ifndef DT5751_INCLUDE_H
#define DT5751_INCLUDE_H

#define DT5751_EVENT_READOUT_BUFFER            0x0000

#define DT5751_BOARD_CONFIG                  0x8000      /* R/W       ; D32 */ 
#define DT5751_BOARD_CFG_BIT_SET             0x8004      /* write only  D32 */ 
#define DT5751_BOARD_CFG_BIT_CLR             0x8008      /* write only; D32 */
 
#define DT5751RAW_BUFFER_ORGANIZATION          0x800C      /* R/W       ; D32 */ 
#define DT5751RAW_CUSTOM_SIZE                  0x8020      /* R/W       ; D32 */
#define DT5751ZLE_RECORD_LENGTH                0x8020      /* R/W       ; D32 */
#define DT5751ZLE_PRE_TRIGGER_SETTING          0x8038      /* R/W       ; D32 */ 
#define DT5751_ADC_CALIBRATION                 0x809C      /* R/W       ; D32 */ 
#define DT5751_ACQUISITION_CONTROL             0x8100      /* R/W       ; D32 */ 
#define DT5751_ACQUISITION_STATUS              0x8104      /* read  only; D32 */ 
#define DT5751_SW_TRIGGER                      0x8108      /* write only; D32 */ 
#define DT5751_TRIG_SRCE_EN_MASK               0x810C      /* R/W       ; D32 */ 
#define DT5751_FP_TRIGGER_OUT_EN_MASK          0x8110      /* R/W       ; D32 */ 
#define DT5751RAW_POST_TRIGGER_SETTING         0x8114      /* R/W       ; D32 */ 
#define DT5751_FP_IO_DATA                      0x8118      /* R/W       ; D32 */ 
#define DT5751_FP_IO_CONTROL                   0x811C      /* R/W       ; D32 */  
#define DT5751_CHANNEL_EN_MASK                 0x8120      /* R/W       ; D32 */ 
#define DT5751_ROC_FPGA_FW_REV                 0x8124      /* read  only; D32 */ 
#define DT5751_EVENT_STORED                    0x812C      /* read  only; D32 */ 
#define DT5751_SET_MONITOR_DAC                 0x8138      /* R/W       ; D32 */ 
#define DT5751_SW_CLOCK_SYNCH                  0x813C      /* W       ; D32 */ 
#define DT5751_BOARD_INFO                      0x8140	  /* read  only; D32 */ 
#define DT5751_MONITOR_MODE                    0x8144      /* R/W       ; D32 */ 
#define DT5751_EVENT_SIZE                      0x814C	  /* read  only; D32 */
#define DT5751_FAN_SPEED_CONTROL               0x8168	  /* R/W       ; D32 */
#define DT5751RAW_ALMOST_FULL_LEVEL            0x816C      /* R/W       ; D32 */
#define DT5751_RUN_START_STOP_DELAY            0x8170      /* R/W       ; D32 */
#define DT5751_BOARD_FAILURE_STATUS            0x8178      /* R/W       ; D32 */
#define DT5751_FP_LVDS_IO_CRTL                 0x81A0      /* R/W       ; D32 */
#define DT5751_BUFFER_OCCUPANCY_GAIN           0x81B4      /* R/W       ; D32 */
#define DT5751_EXTENDED_VETO_DELAY             0x8170      /* R/W       ; D32 */

#define DT5751_READOUT_CONTROL                 0xEF00      /* R/W       ; D32 */ 
#define DT5751_READOUT_STATUS                  0xEF04      /* read  only; D32 */ 
#define DT5751_BOARD_ID                        0xEF08      /* R/W       ; D32 */ 
#define DT5751_MULTICAST_BASE_ADDCTL           0xEF0C      /* R/W       ; D32 */ 
#define DT5751_RELOC_ADDRESS                   0xEF10      /* R/W       ; D32 */ 
#define DT5751_INTERRUPT_STATUS_ID             0xEF14      /* R/W       ; D32 */ 
#define DT5751_INTERRUPT_EVT_NB                0xEF18      /* R/W       ; D32 */ 
#define DT5751_BLT_EVENT_NB                    0xEF1C      /* R/W       ; D32 */ 
#define DT5751_SCRATCH                         0xEF20      /* R/W       ; D32 */ 
#define DT5751_SW_RESET                        0xEF24      /* write only; D32 */ 
#define DT5751_SW_CLEAR                        0xEF28      /* write only; D32 */ 
#define DT5751_CONFIG_RELOAD                   0xEF34      /* write only; D32 */ 
#define DT5751_CONFIG_ROM                      0xF000      /* read  only; D32 */ 

#define DT5751ZLE_ZS_BASELINE                  0x1034      /* For channel 0 **** */
#define DT5751ZLE_ZS_NSAMP_BEFORE              0x1054      /* For channel 0 **** */
#define DT5751ZLE_ZS_NSAMP_AFTER               0x1058      /* For channel 0 **** */
#define DT5751ZLE_ZS_THRESHOLD                 0x105C      /* For channel 0 **** */
#define DT5751ZLE_CHANNEL_THRESHOLD            0x1060      /* For channel 0 */
#define DT5751ZLE_INPUT_CONTROL                0x1064      /* For channel 0 */
#define DT5751RAW_CHANNEL_THRESHOLD            0x1080      /* For channel 0 */
#define DT5751_CHANNEL_STATUS                  0x1088      /* For channel 0 */
#define DT5751_FPGA_FWREV                      0x108C      /* For channel 0 */
#define DT5751_CHANNEL_DAC                     0x1098      /* For channel 0 */
#define DT5751_CHANNEL_CONFIG		       0x109C      /* For channel 0 */
#define DT5751_CHANNEL_TEMPERATURE             0x10A8      /* For channel 0 */

// bitmasks

#define DT5751_RUN_START                             0x0001
#define DT5751_RUN_STOP                              0x0002
#define DT5751_REGISTER_RUN_MODE                     0x0003
#define DT5751_SIN_RUN_MODE                          0x0004
#define DT5751_SIN_GATE_RUN_MODE                     0x0005
#define DT5751_MULTI_BOARD_SYNC_MODE                 0x0006
#define DT5751_COUNT_ACCEPTED_TRIGGER                0x0007
#define DT5751_COUNT_ALL_TRIGGER                     0x0008
#define DT5751_TRIGGER_OVERTH                        0x0001
#define DT5751_TRIGGER_UNDERTH                       0x0002
#define DT5751_PACK25_ENABLE                         0x0003
#define DT5751_PACK25_DISABLE                        0x0004
#define DT5751_NO_ZERO_SUPPRESSION                   0x0005
#define DT5751_ZLE                                   0x0006
#define DT5751_ZS_AMP                                0x0007

#define DT5751_ALIGN64                           0x20

#endif


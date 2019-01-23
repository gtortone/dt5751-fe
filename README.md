# Documentation for the DS-Proto V1725 DAQ


## Useful Information about ODB settings

* Post Trigger in 16ns steps
* Custom size  in 16ns steps
* The trigger threshold (reg 0x1n60) is in DC counts relative to the calculated
baseline.  The polarity of the trigger threshold is hardcoded to be negative 
(bit 6 of register 1n64)
* Setting baseline DAC = 1000 sets the digitizer baseline (for 0V baseline) to 
approximately full scale (~16400 DC counts).  Setting DAC = 62000
sets the digitizer baseline to near zero (~400 DC counts)


## Other information

* This code works for the V1725 firmware that supports ZLE; the register map is
different for the standard V1725 firmware that only does waveform readout.
* However, we will usually use the boards without ZLE compression.



## Prototype code

The code for the prototype DAQ programs is here:

https://bitbucket.org/ttriumfdaq/dsproto_daq/src/master/

The code for the prototype analyzer programs is here:

https://bitbucket.org/ttriumfdaq/dsproto_analyzer/src/master/





## 

## ELOG

Ongoing work on the DAQ is also documented on the elog 

https://ds-proto-daq.triumf.ca/elog/DS+Prototype/



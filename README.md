# Documentation for the DS-Proto V1725 DAQ

## MIDAS DAQ Installation

The MIDAS DAQ is installed on a desktop machine currently called 

ds-proto-daq.triumf.ca 

We have a 4-port A3818 CONET2 installed in ds-proto-daq, connected to
4 V1725s.

The MIDAS webserver for the prototype setup can be seen here:

https://ds-proto-daq.triumf.ca/

Username is 'dsproto' and the password is the regular Indico password.


## Online analyzer

There is a javascript-based analyzer that can be used to see the waveforms and
histograms in real time:

https://ds-proto-daq.triumf.ca/CS/webdisplay

The same plots are also available with the built-in JSROOT display provided by ROOT:

https://ds-proto-daq.triumf.ca/rootana/

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



## Prototype code locations

The code for the prototype DAQ programs is here:

https://bitbucket.org/ttriumfdaq/dsproto_daq/src/master/

The code for the prototype analyzer programs is here:

https://bitbucket.org/ttriumfdaq/dsproto_analyzer/src/master/


## Simple data analysis

### Build analyzer code

The analyzer program is based on [ROOTANA](https://bitbucket.org/tmidas/rootana/src/master/)
which is a package for looking at MIDAS data online (as it is being taken) or offline (from MIDAS
files).  The analyzer code has been compiled and run on Centos-7 (using ROOT 6.12)
and MacOS 10.13 (using ROOT 6.10).
It should work on other linux variants and other ROOT versions,
though there might be some minor compiler problems to fix.

To build the analyzer code, you need to first build ROOTANA (and have ROOT available); follow instructions
on ROOTANA page to build it.  Once you have build ROOTANA, do the following

    git clone https://bitbucket.org/ttriumfdaq/dsproto_analyzer
    make

The following executables will be built:

* protoDisplay.exe: a graphical display (based on ROOT plotting tools) that shows V1725 waveforms
and cumulative histograms.
* ana.exe: a batch program that creates most of the same histograms as protoDisplay.exe
* midas2root.exe: a program to convert the MIDAS data to ROOT Tree.  NOT YET WRITTEN!

Some of the other important classes are:

* TV1725RawData: this class does the decoding of the V1725 banks happens.
* TV1725Waveform: this class makes the plots of the V1725 waveforms.

### Accessing MIDAS files

The MIDAS files are temporarily being served here:

https://ds-proto-daq.triumf.ca/proto_data/

You can grab a file from there.  For instance, grab the file run00183sub000.mid.gz which has at least
some pulses in it.

Now you can examine this file by doing

    protoDisplay.exe run00183sub000.mid.gz



## ELOG

Ongoing work on the DAQ is also documented on the elog 

https://ds-proto-daq.triumf.ca/elog/DS+Prototype/



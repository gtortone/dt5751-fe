#!/usr/bin/bash
# This script should be run after the CDM boots
# (e.g. after the VME crate is power-cycled) to ensure
# it gets into a good state.

# This version configures the board to output a 50MHz clock 
# based only on internal clocks (no external reference).
#
# This version is to be used if the CDM should use only
# INTERNAL oscillators to generate the clock (no external source).
#
# This version should work with CDM firmware versions close to 0.2.454
# Use  `esper-tool read <ip_addr> build version_sh` to see what you're running.
# E.g. `esper-tool read 172.16.21.85 build version_sh`.

CDM_IPADDR=172.16.21.85

function write_lmk {
    # Args for this bash function: param_name value
    esper-tool write -d $2 $CDM_IPADDR lmk $1
    status=$?
    if [[ $status != 0 ]]; then echo Command \"esper-tool write -d $2 $CDM_IPADDR lmk $1\" failed with status $status!; fi
}

echo "Configuring CDM (will take a few seconds)..."
esper-tool write -d 1 $CDM_IPADDR template current_setup

sleep 1

# 0 for internal, 1 for eSATA, 2 for extsernal
esper-tool write -d 0 $CDM_IPADDR cdm sel_ext
esper-tool write -d 0 $CDM_IPADDR cdm sel_nim
esper-tool write -d 0 $CDM_IPADDR cdm sel_atomic_clk

# 2 for external, 0 for internal
write_lmk clkin_sel_mode 0
write_lmk pll2_nclk_mux false
write_lmk pll1_nclk_mux false
write_lmk fb_mux 0
write_lmk fb_mux_en false
write_lmk clkin0_en true
write_lmk clkin1_en true
write_lmk clkin2_en false
write_lmk clkin_sel_mode 0
write_lmk clkin_sel_pol false
write_lmk clkin0_out_mux 2
write_lmk clkin1_out_mux 2
# CLKin_override not found for this CDM? Can 'ls' it, but not read/write...
# Fortunately it seems to default to 'true' anyway.
#write_lmk CLKin_override true
write_lmk hldo_en false
write_lmk clkin0_r 1
write_lmk clkin1_r 1
write_lmk clkin2_r 1
write_lmk pll1_n 10
write_lmk pll2_r 20
write_lmk pll2_p 2
write_lmk pll2_ref_2x_en true
write_lmk pll2_n_cal 60
write_lmk pll2_n 75
write_lmk dclkoutx_div "[30,30,30,30,30,30,30]"
write_lmk oscout_fmt 0

# Wait for PLLs to lock
echo "Waiting for PLLs to stabilize..."
sleep 5

# Check freqeuency - param not present on CDM2
#echo "Checking output frequency. Value should be close to 50MHz:"
#esper-tool read $CDM_IPADDR cdm cc_freq

# Check PLL locks
echo "Checking PLL lock status. Both values should be 1:"
esper-tool read $CDM_IPADDR lmk pll1_ld
esper-tool read $CDM_IPADDR lmk pll2_ld
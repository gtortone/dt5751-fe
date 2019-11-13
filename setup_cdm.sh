# This script should be run after the CDM boots
# (e.g. after the VME crate is power-cycled) to ensure
# it gets into a good state.

# The commented-out lines are the original instructions if
# running on the board itself.
# The esper-tool lines are command-line equivalents.

# CDM setup : Thomas, Nov 2019
# esper-tool write -d false 192.168.1.3 mod_tdm run

# cd template
# write current_setup 1
esper-tool write -d 1 192.168.1.5 template current_setup

#cd cdm
# write sel_nim 0
esper-tool write -d 0 192.168.1.5 cdm sel_nim
sleep 5
echo "Next value should be close to 50MHz"
# read ext_clk  (verify this is 50 MHz)
esper-tool read 192.168.1.5 cdm ext_clk

#cd lmk
#write oscout_fmt 0
esper-tool write -d 0 192.168.1.5 lmk oscout_fmt
#write clkin_sel_mode 2
esper-tool write -d 2 192.168.1.5 lmk clkin_sel_mode
#write pll2_n 5
esper-tool write -d 5 192.168.1.5 lmk pll2_n
#write clkin2_r 5
esper-tool write -d 5 192.168.1.5 lmk clkin2_r
sleep 2
echo "Next values should both be 1"
#read pll1_ld (verify this is "1")
esper-tool read  192.168.1.5 lmk pll1_ld
#read pll2_ld (verify this is"1")
esper-tool read  192.168.1.5 lmk pll2_ld
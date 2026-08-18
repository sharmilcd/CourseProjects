`include "D_FF_ED.v"

module RIPPLE_COUNTER (
    input CLK,
    input RESET,
    output [3:0] COUNT
);

D_FF_ED dut1 (~COUNT[0], CLK, ~RESET, COUNT[0]);
D_FF_ED dut2 (~COUNT[1], ~COUNT[0], ~RESET, COUNT[1]);
D_FF_ED dut3 (~COUNT[2], ~COUNT[1], ~RESET, COUNT[2]);
D_FF_ED dut4 (~COUNT[3], ~COUNT[2], ~RESET, COUNT[3]);

endmodule

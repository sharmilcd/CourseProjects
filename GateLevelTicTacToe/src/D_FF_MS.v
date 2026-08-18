`include "D_a.v"

module D_FF_MS (input D, input CLK, input RESET, output Q);

    wire Qm;
    D_a uut1 (D, CLK, RESET, Qm);
    D_a uut2 (Qm, ~CLK, RESET, Q);

endmodule
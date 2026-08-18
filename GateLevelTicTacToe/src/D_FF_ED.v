`include "D_a.v"

module D_FF_ED (input D, input CLK, input RESET, output Q);

wire nd;
not (nc, CLK);
wire inp;
and (inp, CLK, nc);
D_a uut (D, inp, RESET, Q);

endmodule
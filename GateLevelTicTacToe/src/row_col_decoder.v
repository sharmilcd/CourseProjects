`include "decoder.v"

module row_col_decoder(input [1:0] row, input [1:0] col, output [8:0] out);

    wire [3:0] derow;
    wire [3:0] decol;

    decoder row_decoder (.i(row), .o(derow));
    decoder col_decoder (.i(col), .o(decol));

    and (out[0], derow[1], decol[1]);
    and (out[1], derow[1], decol[2]);
    and (out[2], derow[1], decol[3]);

    and (out[3], derow[2], decol[1]);
    and (out[4], derow[2], decol[2]);
    and (out[5], derow[2], decol[3]);

    and (out[6], derow[3], decol[1]);
    and (out[7], derow[3], decol[2]);
    and (out[8], derow[3], decol[3]);

endmodule

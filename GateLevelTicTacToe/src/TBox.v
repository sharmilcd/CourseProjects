`include "TCell.v"
`include "row_col_decoder.v"

module TBox(input clk, set, reset, input [1:0] row, input [1:0] col, output [8:0] valid, output [8:0] symbol, output reg [1:0] game_state);

reg toggle;
initial begin
    game_state = 2'b00;
    toggle = 1'b1;
end

wire [8:0] cel;
row_col_decoder rcd(row, col, cel);

wire [8:0] set_bit;
wire [8:0] new_move;
wire lock_board;
assign lock_board = (game_state != 2'b00);

genvar i;
generate
    for (i = 0; i < 9; i = i + 1) begin
        and (set_bit[i], set, cel[i], ~lock_board);
        and (new_move[i], set_bit[i], ~valid[i]);
    end
endgenerate

wire move_valid;
wire or1, or2, or3;
or (or1, new_move[0], new_move[1], new_move[2]);
or (or2, new_move[3], new_move[4], new_move[5]);
or (or3, new_move[6], new_move[7], new_move[8]);
or (move_valid, or1, or2, or3);

always @(posedge clk) begin
    if (reset)
        toggle <= 1'b1; 
    else if (set && move_valid)
        toggle <= ~toggle;
end

generate
    for (i = 0; i < 9; i = i + 1) begin : tcell_instances
        TCell TC (.clk(clk), .set(set_bit[i]), .reset(reset), .valid(valid[i]), .set_symbol(toggle), .symbol(symbol[i]));
    end
endgenerate

wire x_wins, o_wins;

and (x_row0, symbol[0], symbol[1], symbol[2], valid[0], valid[1], valid[2]);
and (x_row1, symbol[3], symbol[4], symbol[5], valid[3], valid[4], valid[5]);
and (x_row2, symbol[6], symbol[7], symbol[8], valid[6], valid[7], valid[8]);

and (x_col0, symbol[0], symbol[3], symbol[6], valid[0], valid[3], valid[6]);
and (x_col1, symbol[1], symbol[4], symbol[7], valid[1], valid[4], valid[7]);
and (x_col2, symbol[2], symbol[5], symbol[8], valid[2], valid[5], valid[8]);

and (x_diag1, symbol[0], symbol[4], symbol[8], valid[0], valid[4], valid[8]);
and (x_diag2, symbol[2], symbol[4], symbol[6], valid[2], valid[4], valid[6]);

and (o_row0, ~symbol[0], ~symbol[1], ~symbol[2], valid[0], valid[1], valid[2]);
and (o_row1, ~symbol[3], ~symbol[4], ~symbol[5], valid[3], valid[4], valid[5]);
and (o_row2, ~symbol[6], ~symbol[7], ~symbol[8], valid[6], valid[7], valid[8]);

and (o_col0, ~symbol[0], ~symbol[3], ~symbol[6], valid[0], valid[3], valid[6]);
and (o_col1, ~symbol[1], ~symbol[4], ~symbol[7], valid[1], valid[4], valid[7]);
and (o_col2, ~symbol[2], ~symbol[5], ~symbol[8], valid[2], valid[5], valid[8]);

and (o_diag1, ~symbol[0], ~symbol[4], ~symbol[8], valid[0], valid[4], valid[8]);
and (o_diag2, ~symbol[2], ~symbol[4], ~symbol[6], valid[2], valid[4], valid[6]);

or (x_wins, x_row0, x_row1, x_row2, x_col0, x_col1, x_col2, x_diag1, x_diag2);
or (o_wins, o_row0, o_row1, o_row2, o_col0, o_col1, o_col2, o_diag1, o_diag2);

wire all_cells_filled;
and (all_cells_filled, valid[0], valid[1], valid[2], valid[3], valid[4], valid[5], valid[6], valid[7], valid[8]);

wire draw;
and (draw, all_cells_filled, ~x_wins, ~o_wins);

always @(posedge clk) begin
    if (reset)
        game_state <= 2'b00;
    else if (x_wins)
        game_state <= 2'b01; 
    else if (o_wins)
        game_state <= 2'b10; 
    else if (draw)
        game_state <= 2'b11; 
end

endmodule
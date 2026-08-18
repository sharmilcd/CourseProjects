module TCell(input clk, set, reset, set_symbol, output reg valid, symbol);

initial begin
    valid <= 1'b0;
    symbol <= 1'b0;
end
always @(posedge clk) begin
    if(reset) begin
        valid <= 1'b0;
        symbol <= 1'b0;
    end else if (set & ~valid) begin
        symbol <= set_symbol;
        valid <= 1'b1;
    end
end

endmodule
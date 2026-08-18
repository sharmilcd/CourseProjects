`include "RIPPLE_COUNTER.v"

module RippleCounter_tb;

reg clk;           
reg reset;
wire [3:0] count;      

RIPPLE_COUNTER uut (
    .CLK(clk),
    .RESET(reset),
    .COUNT(count)
);

initial begin
    clk = 0;
    forever #10 clk = ~clk;
end

initial begin
    reset = 1;
    #20;
    reset = 0;

    $display("Time\tCounter");
    $monitor("%0d\t%b", $time, count);

    #400;         

    reset = 1;
    $display("Counter RESET");
    #20;
    reset = 0;

    #100;

    $display("Simulation Done");
    $finish;
end

endmodule

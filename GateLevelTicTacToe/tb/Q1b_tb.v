`include "D_FF_ED.v"

// Testbench for D Flip Flop Master-Slave
module D_FF_ED_tb;
    // Testbench signals
    reg D, CLK;
    wire Q;
    reg expected_Q;
    integer errors;
    
    // Instantiate the D flip flop
    D_FF_ED dut(
        .D(D),
        .CLK(CLK),
        .Q(Q)
    );
    
    // Clock generation
    initial begin
        CLK = 0;
        forever begin
            #5 CLK = 1;
            #5 CLK = 0;
        end
    end
    
    // Expected output generation
    always @(posedge CLK) begin
        expected_Q <= D;  // Update expected output on raising edge
    end

    // Test stimulus and verification
    initial begin
        // Initialize variables
        D = 0;
        errors = 0;
        
        // Dump waveforms
        $dumpfile("D_FF_ED_tb.vcd");
        $dumpvars(0, D_FF_ED_tb);
        
        // Wait for initial stabilization
        #8;
        
        // Test Case 1: D = 1
        D = 1;
        #4;  // Wait for one clock cycle
        
        // Test Case 2: D = 0
        D = 0;
        #4;
        
        // Test Case 3: D = 1
        D = 1;
        #8;
        
        // Test Case 4: D = 0
        D = 0;
        #12;
        
        // Test Case 5: D = 1
        D = 1;
        #8;
        
        // Test Case 6: Rapid transitions
        repeat(4) begin
            #7 D = ~D;
        end
        
        // Wait for final transitions to complete
        #15;
        
        // Report results
        $display("\nTest Complete!");
        $display("Total Errors: %0d", errors);
        
        // End simulation
        $finish;
    end
    
    // Verification process
    always @(posedge CLK) begin
        #1; // Small delay to allow for output stabilization
        if (Q !== expected_Q) begin
            $display("Error at time %0t: D=%b, Expected Q=%b, Actual Q=%b",
                    $time, D, expected_Q, Q);
            errors = errors + 1;
        end
    end
    
    // Monitor changes
    initial begin
        $display("Time\tD\tCLK\tQ\tExpected_Q");
        $monitor("%0t\t%b\t%b\t%b\t%b", $time, D, CLK, Q, expected_Q);
    end
    
endmodule
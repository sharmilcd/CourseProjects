module D_a(input D, input en, input rstn, output reg Q);

always @ (*) begin
    
    if(rstn == 0) begin
        Q=1'b0;
    end
    else begin
        if(en == 1) begin
            Q = D;
        end
        else begin
            Q = Q;
        end
    end

end

endmodule
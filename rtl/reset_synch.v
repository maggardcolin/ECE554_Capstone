`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/06/2026 08:20:01 PM
// Design Name: 
// Module Name: reset_synch
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module reset_synch( 
    input clk,
    input rstn_async,
    input IO_rstp,
    output rstn_sync
    );
    
    reg[1:0] ff;
    
    wire globrst_n_async;
    assign globrst_n_async = rstn_async & (~IO_rstp);
    
    always @(posedge clk, negedge globrst_n_async) begin
        if(!globrst_n_async) begin
            ff <= 2'b0;
        end else begin
            ff <= {1'b1, ff[1]};
        end
    end
    
    assign rstn_sync = ff[0];
endmodule

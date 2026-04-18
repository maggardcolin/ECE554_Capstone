`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/11/2026 04:16:46 PM
// Design Name: 
// Module Name: regdst_debug
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


module regdst_debug(
    input clk,
    input rst_n,
    input interrupt,
    input [3:0] regdst,
    output reg[7:0] ledr
    );

always @(posedge clk, negedge rst_n) begin
    if(!rst_n) begin
        ledr <= 8'b0;
    end else if(interrupt) begin
        ledr <= {ledr[7:4]+1, regdst};
    end
end


endmodule

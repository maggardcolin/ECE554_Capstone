`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/06/2026 08:25:16 PM
// Design Name: 
// Module Name: PB_breakout
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


module PB_breakout(
    input [3:0] PL_USER_PB,
    output PB0,
    output PB1,
    output PB2,
    output PB3
    );
    assign PB0 = PL_USER_PB[0];
    assign PB1 = PL_USER_PB[1];
    assign PB2 = PL_USER_PB[2];
    assign PB3 = PL_USER_PB[3];
endmodule

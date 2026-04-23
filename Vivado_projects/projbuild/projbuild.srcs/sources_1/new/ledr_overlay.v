`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/06/2026 11:24:41 PM
// Design Name: 
// Module Name: ledr_overlay
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


module ledr_overlay(
        input wire[7:0] LEDR_0,
        input wire[7:0] LEDR_1,
        input wire[7:0] LEDR_2,
        input wire[7:0] LEDR_3,
        input wire SW0,
        input wire SW1,
        output wire[7:0] LEDR
    );
    assign LEDR = SW1 ? 
                    (SW0 ? LEDR_3 : LEDR_2)
                  :
                    (SW0 ? LEDR_1 : LEDR_0);
endmodule

`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/06/2026 11:22:31 PM
// Design Name: 
// Module Name: sw_breakout
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


module sw_breakout(
        input wire[7:0] PL_USER_SW,
        output wire SW0,
        output wire SW1,
        output wire SW2,
        output wire SW3,
        output wire SW4,
        output wire SW5,
        output wire SW6,
        output wire SW7
    );
    assign {SW7,SW6,SW5,SW4,SW3,SW2,SW1,SW0} = PL_USER_SW;
endmodule

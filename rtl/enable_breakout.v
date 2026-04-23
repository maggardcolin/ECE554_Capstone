`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/12/2026 10:49:04 PM
// Design Name: 
// Module Name: enable_breakout
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


module enable_breakout(
    input [31:0] enable_in,
    output en0,
    output en1,
    output en2,
    output en3,
    output en4,
    output en5,
    output en6,
    output en7,
    output en8,
    output en9,
    output en10,
    output en11,
    output en12,
    output en13,
    output en14,
    output en15,
    output en16,
    output en17,
    output en18,
    output en19
    );
    assign en0 = enable_in[0];
    assign en1 = enable_in[1];
    assign en2 = enable_in[2];
    assign en3 = enable_in[3];
    assign en4 = enable_in[4];
    assign en5 = enable_in[5];
    assign en6 = enable_in[6];
    assign en7 = enable_in[7];
    assign en8 = enable_in[8];
    assign en9 = enable_in[9];
    assign en10 = enable_in[10];
    assign en11 = enable_in[11];
    assign en12 = enable_in[12];
    assign en13 = enable_in[13];
    assign en14 = enable_in[14];
    assign en15 = enable_in[15];
    assign en16 = enable_in[16];
    assign en17 = enable_in[17];
    assign en18 = enable_in[18];
    assign en19 = enable_in[19];
endmodule

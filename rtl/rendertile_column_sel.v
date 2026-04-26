`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/21/2026 05:12:33 PM
// Design Name: 
// Module Name: rendertile_column_sel
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


module rendertile_column_sel(
    input clk,
    input wire [255:0] rendertile0,
    input wire [255:0] rendertile1,
    input wire [255:0] rendertile2,
    input wire [255:0] rendertile3,
    input wire [255:0] rendertile4,
    input wire [255:0] rendertile5,
    input wire [255:0] rendertile6,
    input wire [255:0] rendertile7,
    input wire [255:0] rendertile8,
    input wire [255:0] rendertile9,
    input wire [255:0] rendertile10,
    input wire [255:0] rendertile11,
    input wire [255:0] rendertile12,
    input wire [255:0] rendertile13,
    input wire [255:0] rendertile14,
    input wire [255:0] rendertile15,
    input wire [255:0] rendertile16,
    input wire [255:0] rendertile17,
    input wire [255:0] rendertile18,
    input wire [255:0] rendertile19,
    input wire [5:0] rendertile_addr,
    output reg [255:0] bram_out
    );
    
    always @(posedge clk) begin
        case (rendertile_addr)
            0: bram_out <= rendertile0;
            1: bram_out <= rendertile1;
            2: bram_out <= rendertile2;
            3: bram_out <= rendertile3;
            4: bram_out <= rendertile4;
            5: bram_out <= rendertile5;
            6: bram_out <= rendertile6;
            7: bram_out <= rendertile7;
            8: bram_out <= rendertile8;
            9: bram_out <= rendertile9;
            10: bram_out <= rendertile10;
            11: bram_out <= rendertile11;
            12: bram_out <= rendertile12;
            13: bram_out <= rendertile13;
            14: bram_out <= rendertile14;
            15: bram_out <= rendertile15;
            16: bram_out <= rendertile16;
            17: bram_out <= rendertile17;
            18: bram_out <= rendertile18;
            19: bram_out <= rendertile19;
            default: bram_out <= rendertile0;
        endcase
    end
    
    
endmodule

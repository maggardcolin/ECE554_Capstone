`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/06/2026 08:09:02 PM
// Design Name: 
// Module Name: Control_DMA
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: Breaks out all DMA registers into normal named components
// 
// Dependencies: 
// 
// Revision:
// Revision 0.01 - File Created
// Additional Comments:
// 
//////////////////////////////////////////////////////////////////////////////////


module Control_DMA(
        input wire[31:0] slv_reg0,
        input wire[31:0] slv_reg1,
        input wire[31:0] slv_reg2,
        input wire[31:0] slv_reg3,
        input wire[31:0] slv_reg4,
        input wire[31:0] slv_reg5,
        input wire[31:0] slv_reg6,
        input wire[31:0] slv_reg7,
        input wire[31:0] slv_reg8,
        input wire[31:0] slv_reg9,
        input wire[31:0] slv_reg10,
        input wire[31:0] slv_reg11,
        input wire[31:0] slv_reg12,
        input wire[31:0] slv_reg13,
        input wire[31:0] slv_reg14,
        input wire[31:0] slv_reg15,
        
        input wire[3:0] regdest,
        input wire interrupt,
        
        // REG BLOCK 0
        output wire[7:0] PL_USER_LED,
        output wire[2:0] PL_LEDRGB0,
        output wire[2:0] PL_LEDRGB1,
        output wire[2:0] PL_LEDRGB2,
        output wire[2:0] PL_LEDRGB3,
        output wire instruction_valid,
        output wire datastream_valid,
        output wire reset_pl,
        output wire ping,
        
        // REG BLOCK 1, 2
        output wire[63:0] ps_instruction,
        
        // REG BLOCK 3, 4
        output wire[63:0] ps_datastream,
        
        // REG BLOCK 5
        // REG BLOCK 6 reserved for future expansion
        output wire[32:0] tilemask,
        output wire[31:0] audio_bus
    );
    assign PL_USER_LED = slv_reg0[7:0];
    assign PL_LEDRGB0 = slv_reg0[10:8];
    assign PL_LEDRGB1 = slv_reg0[13:11];
    assign PL_LEDRGB2 = slv_reg0[16:12];
    assign PL_LEDRGB3 = slv_reg0[19:17];
    assign reset_pl = slv_reg0[22];
    assign ping = slv_reg0[23];
    
    assign ps_instruction = {slv_reg2, slv_reg1};
    // Write to lower bits of instruction makes a new one valid
    assign instruction_valid = ((regdest == 4'h1) & interrupt);
    assign ps_datastream = {slv_reg4, slv_reg3};
    
    assign tilemask = {1'b0, slv_reg5}; // IDK why I made it 33 bits but whatever.
    assign audio_bus = slv_reg6;
endmodule

`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/14/2026
// Design Name: 
// Module Name: dispatcher
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


module dispatcher(
    input wire clk,
    input wire rst_n,
    
    // Raw instruction in from PS side through MMIO DMA
    // The instruction is polled on rising edge of instruction_valid
    // ACK is unused for now since the PL side will dispatch the instruction 
    // much, much faster than the PS side can issue a new one.
    input wire[63:0] instruction_in,
    input wire instruction_valid,
    // Instruction ACK. This is sent to tell PS to deassert instruction valid
    // This will then deassert.
    output reg ack,
    
    // Data stream that is used for shifting in data to the render tile sprite
    // and texture buffers. Each render tile will have its own chunk of BRAM
    // and will be able to read from it as fast as needed.
    // Each of these BRAMs will have two 256 bit ports allowing both sprite
    // and mask data to be read in the same cycle, then applied to the working
    // memory in a massive parallel operation
    input wire [63:0] datastream_in,
    input wire datastream_valid,
    output reg datastream_ack,
    
    // one hot description of which render tiles a particular instruction applies to
    // For instance, usual rendering using one row of tiles would be:
    // tile mask applies to all, set y coordinate
    // Tile mask applies to 1 at a time, set x coordinate
    // Repeat only the first one to render each screen row at a time
    input wire [32:0] tilemask_in,
    
    // Decoded instruction to be sent to render tile
    // Tile mask will only be high for one cycle depending on the current state.
    output reg[4:0] instruction_to_rt,
    output reg[11:0] arg1_to_rt,
    output reg[11:0] arg2_to_rt,
    output reg[11:0] arg3_to_rt,
    output reg[11:0] arg4_to_rt,
    output reg[10:0] arg5_to_rt,
    output reg[32:0] enable,
    input wire busy,
    input wire[3:0] error_code
    );
    // 
    
endmodule
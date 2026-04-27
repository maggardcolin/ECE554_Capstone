`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/06/2026 10:07:32 PM
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
    // much, much faster than the PS side can issue a new one.
    input wire[63:0] instruction_in,
    input wire instruction_valid,
    // Used to signal instruction complete
    output wire ack,
    
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
    output reg[63:0] instruction_to_rt,
    output reg[31:0] enable,
    input wire[31:0] busy,
    input wire[3:0] error_code,
    output reg[7:0] ledr_debug
    );
    // Debug logic - show the instruction that was issued
    always @(posedge clk, negedge rst_n) begin
        if(!rst_n) begin
            ledr_debug <= 8'b0;
        end else if(instruction_valid) begin
            ledr_debug <= {instruction_in[63:56]};
        end
    end
    // Control the output enable signal
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            enable <= 32'b0; // No render tiles active
        end else if(instruction_valid) begin
            // Flop and repeat the enable signal. This is used to control the tile mask
            enable <= tilemask_in;
        end else begin
            enable <= 32'b0;
        end
    end
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            instruction_to_rt <= 63'b0;
        end else if(instruction_valid) begin
            instruction_to_rt <= instruction_in;
        end
    end
    // Signal instruction completion when all bits of busy are 0.
    // This is driven straight from the render tile array
    assign ack = |(~busy);
endmodule
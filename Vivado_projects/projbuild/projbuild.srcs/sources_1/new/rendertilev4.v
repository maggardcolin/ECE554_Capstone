`timescale 1ns / 1ps
//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 04/11/2026 08:52:16 PM
// Design Name: 
// Module Name: rendertilev2
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

module rendertilev4(
        input wire clk,
        input wire rst_n,
        
        // Interface from dispatcher unit
        input wire en,
        input wire[63:0] instruction_in,
        
        // TODO interface to BRAM
        output wire[5:0] read_row,
        output wire[5:0] read_en,
        input wire[255:0] read_data,
        output wire write_en,
        output wire[5:0] write_row,
        output wire[255:0] write_data,
        
        // Debug output
        output wire[7:0] LEDR_DEBUG,
        input wire DMA_en,
        input wire [5:0] DMA_row,
        output wire [7:0] DMA_LED_DEBUG
    );
    
    reg[4:0] instr;
    reg[11:0] arg1;
    reg[11:0] arg2;
    reg[11:0] arg3;
    reg[11:0] arg4;
    reg[10:0] arg5;
    
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            instr <= 5'b0;
            arg1 <= 12'b0;
            arg2 <= 12'b0;
            arg3 <= 12'b0;
            arg4 <= 12'b0;
            arg5 <= 11'b0;
        end else if(en && state == STATE_IDLE) begin
            instr <= instruction_in[63:59];
            arg1 <= instruction_in[58:47];
            arg2 <= instruction_in[46:35];
            arg3 <= instruction_in[34:23];
            arg4 <= instruction_in[22:11];
            arg5 <= instruction_in[10:0];
        end
    end
    
    localparam STATE_IDLE = 5'b0;
    localparam STATE_SETCOORD = 5'b1;
    localparam STATE_FILLSCREEN = 5'b10;
    localparam STATE_PUTPIXEL = 5'b11;
    localparam STATE_PUTPIXEL_INTERMEDIATE = 5'b100;
    localparam STATE_PUTPIXEL_WRITE = 5'b101;
    
    localparam INSTR_NOP = 5'b0;
    localparam INSTR_SETCOORD = 5'b1;
    localparam INSTR_FILLSCREEN = 5'b10;
    localparam INSTR_PUTPIXEL = 5'b11;
    

    reg[4:0] state;
    reg[4:0] next_state; // Actually of type wire

    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            state <= STATE_IDLE;
        end else begin
            state <= next_state;
        end
    end
    
    reg[5:0] x_index;
    reg[5:0] y_index;
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            x_index <= 6'b0;
            y_index <= 6'b0;
        end else if(state == STATE_SETCOORD) begin
            if(arg3[0]) begin
                x_index <= arg1[5:0];
            end
            if(arg4[0]) begin
                y_index <= arg2[5:0];
            end
        end
    end
    
    // Put Pixel stuff
    wire PUTPIXEL_inrange; // This render tile needs to activate now
    assign PUTPIXEL_inrange = (arg1[10:6]==x_index)&(arg2[10:6]==y_index);
    reg[5:0] putpixel_address;
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            putpixel_address <= 5'b0;
        end else if(state == STATE_IDLE && next_state == STATE_PUTPIXEL) begin
            putpixel_address <= instruction_in[40:35]; // Grab lower bits as the row to read/write
        end else if(state == STATE_IDLE) begin
            putpixel_address <= 5'b0; // If we're in the idle state, deassert this to not mess with other blocks
        end
    end
    // Data to write
    reg[255:0] putpixel_wdata;
    wire[7:0] putpixel_doffset;
    assign putpixel_doffset = arg1[5:0] << 2;
    always @(*) begin
        putpixel_wdata = read_data;
        putpixel_wdata[putpixel_doffset +: 4] = arg3[3:0];
    end
    wire putpixel_wen;
    assign putpixel_wen = (state == STATE_PUTPIXEL_WRITE);
    
    // FillScreen stuff
    reg fillscreen_done;
    reg[5:0] fillscreen_row;
    wire fillscreen_wen;
    wire[256:0] fillscreen_wdata;
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            fillscreen_done <= 1'b0;
        end else if(state == STATE_IDLE) begin
            fillscreen_done <= 1'b0;
        end else if(state == STATE_FILLSCREEN && fillscreen_row == 6'h3F) begin
            fillscreen_done <= 1'b1;
        end
    end
    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            fillscreen_row <= 6'b0;
        end else if(state == STATE_IDLE) begin
            fillscreen_row <= 6'b0;
        end else if(state == STATE_FILLSCREEN) begin
            if(fillscreen_row != 6'h3F) begin
                fillscreen_row <= fillscreen_row + 1;
            end
        end
    end
    assign fillscreen_wen = (state == STATE_FILLSCREEN);
    assign fillscreen_wdata = {64{arg1[3:0]}};
    
    // State machine
    
    
    // Main control FSM
    always@(*) begin
        next_state = state;
        case(state)
        STATE_IDLE: begin
            if(en) begin
                case(instruction_in[63:59])
                INSTR_NOP:
                    next_state = STATE_IDLE;
                INSTR_SETCOORD:
                    next_state = STATE_SETCOORD;
                INSTR_FILLSCREEN:
                    next_state = STATE_FILLSCREEN;
                INSTR_PUTPIXEL:
                    next_state = STATE_PUTPIXEL;
                endcase
            end
        end

        STATE_SETCOORD: begin
            // Logic handled by external block
            // Single cycle latency
            // SETCOORD(X,Y,X_EN,Y_EN)
            next_state = STATE_IDLE;
        end
        STATE_FILLSCREEN: begin
            // Fill the screen
            if(fillscreen_done) begin
                next_state = STATE_IDLE;
            end
        end
        STATE_PUTPIXEL: begin
            // By transitioning into this state, putpixel line will be set. Arguments will also have been captured by now.
            // First, read from the memory
            // This happens naturally as the address is presented during this clock on port A (read port)\
            if(PUTPIXEL_inrange) begin
                next_state = STATE_PUTPIXEL_INTERMEDIATE;
            end else begin
                // No work to be done
                next_state = STATE_IDLE;
            end
        end
        STATE_PUTPIXEL_INTERMEDIATE: begin
            next_state = STATE_PUTPIXEL_WRITE;
        end
        STATE_PUTPIXEL_WRITE: begin
            next_state = STATE_IDLE;
        end
        default: next_state = STATE_IDLE;
        endcase
    end
    
    assign LEDR_DEBUG = {en, 2'b0, state};
    
    // OR together the control signals
    assign read_row = DMA_en ? DMA_row : putpixel_address;
    assign write_row = putpixel_address|fillscreen_row;
    assign write_en = putpixel_wen|fillscreen_wen;
    assign write_data = ({256{putpixel_wen}}&putpixel_wdata) | ({256{fillscreen_wen}}&fillscreen_wdata);
    
    
    reg DMA_en_latch;
    
    always @(posedge clk, negedge rst_n) begin
        if (~rst_n)
            DMA_en_latch <= 1'b0;
        else if (DMA_en)
            DMA_en_latch <= 1'b1;
    end
    
    assign DMA_LED_DEBUG = {DMA_en_latch, 1'b0, DMA_row};

endmodule

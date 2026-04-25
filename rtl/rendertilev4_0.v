`timescale 1ns / 1ps
`default_nettype none
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
    
    localparam STATE_IDLE = 5'b0;
    localparam STATE_SETCOORD = 5'b1;
    localparam STATE_FILLSCREEN = 5'b10;
    localparam STATE_PUTPIXEL = 5'b11;
    localparam STATE_PUTPIXEL_INTERMEDIATE = 5'b100;
    localparam STATE_PUTPIXEL_WRITE = 5'b101;
    localparam STATE_DRAWRECT = 5'b110;
    localparam STATE_FILLRECT = 5'b111;
    localparam STATE_HORIZLINE = 5'b1000;
    localparam STATE_VERTLINE = 5'b1001;

    localparam STATE_RMW = 5'b1010;
    
    localparam INSTR_NOP = 5'b0; // Replaced the blit operation
    localparam INSTR_SETCOORD = 5'b1; // SetCoord(X,Y,X_en, Y_en)
    localparam INSTR_FILLSCREEN = 5'b10; // FillScreen(color)
    localparam INSTR_PUTPIXEL = 5'b11; // PutPixel(X,Y,Color)
    localparam INSTR_DRAWRECT = 5'b100; // DrawRect(X, Y, W, H, Color)
    localparam INSTR_FILLRECT = 5'b101; // FillRect(X, Y, W, H, Color)
    localparam INSTR_DRAWALIEN = 5'b110; // DrawAlien(X, Y, Color)
    localparam INSTR_HORIZLINE = 5'b111; // HorizLine(X, Y, W, Color)
    localparam INSTR_VERTLINE = 5'b1000; // VertLine(X, Y, H, Color)
    
    reg[4:0] state;
    reg[4:0] next_state; // Actually of type wire
    
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
    


 

    always @(posedge clk, negedge rst_n) begin
        if(~rst_n) begin
            state <= STATE_IDLE;
        end else if(DMA_en) begin
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
                INSTR_NOP:         next_state = STATE_IDLE;
                INSTR_SETCOORD:    next_state = STATE_SETCOORD;
                INSTR_FILLSCREEN:  next_state = STATE_FILLSCREEN;
                INSTR_PUTPIXEL:    next_state = STATE_PUTPIXEL;
                INSTR_DRAWRECT:    next_state = STATE_DRAWRECT;
                INSTR_FILLRECT:    next_state = STATE_FILLRECT;
                INSTR_HORIZLINE:   next_state = STATE_HORIZLINE;
                INSTR_VERTLINE:    next_state = STATE_VERTLINE;
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
        STATE_DRAWRECT,
        STATE_FILLRECT,
        STATE_HORIZLINE,
        STATE_VERTLINE: begin
            next_state = STATE_RMW;
        end
        STATE_RMW: begin
            
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

wire[255:0] rect_out;
wire[255:0] fillrect_out;
wire[255:0] horiz_out;
wire[255:0] vert_out;

DrawRect drawrect_inst(
    .buf_in(read_data),
    .buf_index_x(x_index),
    .buf_coord_y({y_index,read_row}),
    .rect_x(arg1),
    .rect_y(arg2),
    .rect_w(arg3),
    .rect_h(arg4),
    .line_color(arg5[3:0]),
    .buf_out(rect_out)
);

FillRect fillrect_inst(
    .buf_in(read_data),
    .buf_index_x(x_index),
    .buf_coord_y({y_index,read_row}),
    .rect_x(arg1),
    .rect_y(arg2),
    .rect_w(arg3),
    .rect_h(arg4),
    .line_color(arg5[3:0]),
    .buf_out(fillrect_out)
);

HorizLineDraw horiz_inst(
    .buf_in(read_data),
    .buf_index_x(x_index),
    .buf_coord_y({y_index,read_row}),
    .line_x(arg1),
    .line_y(arg2),
    .line_width(arg3),
    .line_color(arg4[3:0]),
    .buf_out(horiz_out)
);

VertLineDraw vert_inst(
    .buf_in(read_data),
    .buf_index_x(x_index),
    .buf_coord_y({y_index,read_row}),
    .line_x(arg1),
    .line_y(arg2),
    .line_height(arg3),
    .line_color(arg4[3:0]),
    .buf_out(vert_out)
);

endmodule

module VertLineDraw(
    input wire[255:0] buf_in, // Buffer consisting of a 256 bit (64 pixel) slice of a row. One pixel tall.
    input wire[5:0] buf_index_x, // X index of the start of this buffer. Since it's 64 bits, the X coordinate is buf_index_x << 6. It is guaranteed to be 64 pixel aligned.
    input wire[11:0] buf_coord_y,// Y coordinate of this buffer. This is the full 12 bits.
    input wire[11:0] line_x, // Starting X coordinate of the line
    input wire[11:0] line_y, // Starting Y coordinate of the line
    input wire[11:0] line_height, // Height of the line
    input wire[3:0] line_color, // The color to draw. This is just an index and it will index into a color palette later
    output wire[255:0] buf_out // The modified buf_in data to draw the line
);
    wire [11:0] line_end_y;
    wire [5:0]  target_pixel_x;
    wire        y_hit;
    wire        x_hit;
    wire        draw_en;
    assign line_end_y = line_y + line_height;
    assign y_hit = (buf_coord_y >= line_y) && (buf_coord_y < line_end_y);
    assign x_hit = (line_x[11:6] == buf_index_x);
    assign draw_en = x_hit && y_hit;
    assign target_pixel_x = line_x[5:0];
    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin
            assign buf_out[i*4 +: 4] = (draw_en && (target_pixel_x == i)) ? 
                                        line_color : 
                                        buf_in[i*4 +: 4];
        end
    endgenerate
endmodule

module HorizLineDraw(
    input wire[255:0] buf_in, // Buffer consisting of a 256 bit (64 pixel) slice of a row. One pixel tall.
    input wire[5:0] buf_index_x, // X index of the start of this buffer. Since it's 64 bits, the X coordinate is buf_index_x << 6. It is guaranteed to be 64 pixel aligned.
    input wire[11:0] buf_coord_y,// Y coordinate of this buffer. This is the full 12 bits.
    input wire[11:0] line_x, // Starting X coordinate of the line
    input wire[11:0] line_y, // Starting Y coordinate of the line
    input wire[11:0] line_width, // Width of the line
    input wire[3:0] line_color, // The color to draw. This is just an index and it will index into a color palette later
    output wire[255:0] buf_out // The modified buf_in data to draw the line
);
    wire [11:0] line_end_x;
    wire [11:0] buf_start_x;
    wire [11:0] buf_end_x;
    wire        y_hit;
    assign line_end_x  = line_x + line_width;
    assign buf_start_x = {buf_index_x, 6'd0};     // buf_index_x << 6
    assign buf_end_x   = {buf_index_x, 6'd63};    // (buf_index_x << 6) + 63
    assign y_hit = (buf_coord_y == line_y);
    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin : pixel_logic
            wire [11:0] current_pixel_x = buf_start_x + i;
            wire x_hit;
            assign x_hit = (current_pixel_x >= line_x) && (current_pixel_x < line_end_x);
            assign buf_out[i*4 +: 4] = (y_hit && x_hit) ? 
                                        line_color : 
                                        buf_in[i*4 +: 4];
        end
    endgenerate
endmodule

module DrawRect( // Draw the outline of a rectangle, one pixel wide lines.
    input wire[255:0] buf_in, // Buffer consisting of a 256 bit (64 pixel) slice of a row. One pixel tall.
    input wire[5:0] buf_index_x, // X index of the start of this buffer. Since it's 64 bits, the X coordinate is buf_index_x << 6. It is guaranteed to be 64 pixel aligned.
    input wire[11:0] buf_coord_y,// Y coordinate of this buffer. This is the full 12 bits.
    input wire[11:0] rect_x, // Starting X coordinate of the rect
    input wire[11:0] rect_y, // Starting Y coordinate of the rect
    input wire[11:0] rect_w, // Width of the rect
    input wire[11:0] rect_h, // Height of the rect
    input wire[3:0] line_color, // The color to draw. This is just an index and it will index into a color palette later
    output wire[255:0] buf_out // The modified buf_in data to draw the rect
);
    wire [11:0] rect_end_x = rect_x + rect_w - 1;
    wire [11:0] rect_end_y = rect_y + rect_h - 1;
    
    wire [11:0] buf_start_x = {buf_index_x, 6'd0};

    wire is_top_edge    = (buf_coord_y == rect_y);
    wire is_bottom_edge = (buf_coord_y == rect_end_y);
    wire is_within_y    = (buf_coord_y >= rect_y) && (buf_coord_y <= rect_end_y);

    wire is_left_x_in_buf  = (rect_x[11:6] == buf_index_x);
    wire is_right_x_in_buf = (rect_end_x[11:6] == buf_index_x);

    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin : pixel_logic
            wire [11:0] curr_x = buf_start_x + i;
            
            wire h_hit = (is_top_edge || is_bottom_edge) && 
                         (curr_x >= rect_x && curr_x <= rect_end_x);
         
            wire v_hit = is_within_y && 
                         ((is_left_x_in_buf  && (curr_x == rect_x)) || 
                          (is_right_x_in_buf && (curr_x == rect_end_x)));

            assign buf_out[i*4 +: 4] = (h_hit || v_hit) ? 
                                        line_color : 
                                        buf_in[i*4 +: 4];
        end
    endgenerate
endmodule

module FillRect( // Draw the outline of a rectangle, one pixel wide lines.
    input wire[255:0] buf_in, // Buffer consisting of a 256 bit (64 pixel) slice of a row. One pixel tall.
    input wire[5:0] buf_index_x, // X index of the start of this buffer. Since it's 64 bits, the X coordinate is buf_index_x << 6. It is guaranteed to be 64 pixel aligned.
    input wire[11:0] buf_coord_y,// Y coordinate of this buffer. This is the full 12 bits.
    input wire[11:0] rect_x, // Starting X coordinate of the rect
    input wire[11:0] rect_y, // Starting Y coordinate of the rect
    input wire[11:0] rect_w, // Width of the rect
    input wire[11:0] rect_h, // Height of the rect
    input wire[3:0] line_color, // The color to draw. This is just an index and it will index into a color palette later
    output wire[255:0] buf_out // The modified buf_in data to draw the rect
);
wire [11:0] rect_end_x;
    wire [11:0] rect_end_y;
    wire [11:0] buf_start_x;
    wire        y_hit;

    assign rect_end_x  = rect_x + rect_w;
    assign rect_end_y  = rect_y + rect_h;
    assign buf_start_x = {buf_index_x, 6'd0};

    assign y_hit = (buf_coord_y >= rect_y) && (buf_coord_y < rect_end_y);

    genvar i;
    generate
        for (i = 0; i < 64; i = i + 1) begin : pixel_logic
            wire [11:0] curr_x = buf_start_x + i;
            wire x_hit;

            assign x_hit = (curr_x >= rect_x) && (curr_x < rect_end_x);

            assign buf_out[i*4 +: 4] = (y_hit && x_hit) ? 
                                        line_color : 
                                        buf_in[i*4 +: 4];
        end
    endgenerate
endmodule
`default_nettype wire

`default_nettype none
module rendertile(
	input wire clk,
	input wire rst_n,

	input wire[4:0] instruction_in,
	input wire enable,

	input wire[11:0] arg1_in,
	input wire[11:0] arg2_in,
	input wire[11:0] arg3_in,
	input wire[11:0] arg4_in,
	input wire[10:0] arg5_in, // One less to fit in 64. Do not use this for coordinates.

	output logic busy,
	output reg[3:0] error_code

	// TODO add local storage interface (BRAM)
	// TODO add output for blit unit
);

reg[4:0] X_INDEX;
reg[4:0] Y_INDEX;

reg[3:0] state;
logic[3:0] next_state;
localparam STATE_READY = 0;
localparam STATE_PREPARE = 1;
localparam STATE_FILLCOLOR = 2;
localparam STATE_PUTPIXEL = 3;

localparam E_NOERR = 4'h0;
localparam E_UNIMP = 4'h1;
localparam E_IINST = 4'h2;

localparam INSTR_BLIT = 5'h0;
localparam INSTR_SETCOORD = 5'h1;
localparam INSTR_FILLCOLOR = 5'h2;
localparam INSTR_PUTPIXEL = 5'h3;
localparam INSTR_DRAWSPRITE_V1 = 5'h4;

reg[4:0] instruction;
reg[11:0] arg1;
reg[11:0] arg2;
reg[11:0] arg3;
reg[11:0] arg4;
reg[10:0] arg5; // One less to fit in 64. Do not use this for coordinates.

always @(posedge clk, negedge rst_n) begin
	state <= next_state;
end

// Control signals for fill color unit
// Input from color fill block. Used to indicate
// Render Tile has placed all pixels into backing
// memory.
logic done_fillcolor;
// Hold high for one clock cycle and deassert
// Indicates to color fill to start filling pixels
// in serial/parallel/whatever
logic go_fillcolor;
// Brings color fill fsm back to IDLE state
logic clear_done_fillcolor;

// Control signals for error reporting
// Standard set/clear/code
logic set_error;
logic[3:0] set_error_code;
logic clear_error;

// Logic 1 when should capture instruction_in
logic capture_instruction;
logic SETCOORD_capture_coordinates;

// If instruction is SETCOORD, capture arg1 and arg2
// into X_INDEX and Y_INDEX
always @(posedge clk, negedge rst_n) begin
	if(~rst_n) begin
		X_INDEX <= '0;
		Y_INDEX <= '0;
	end else if(SETCOORD_capture_coordinates) begin
		X_INDEX <= arg1[4:0];
		Y_INDEX <= arg2[4:0];
	end
end

// Instruction data for putpixel
// Calculated automatically
wire[5:0] PUTPIXEL_xcoord;
wire[5:0] PUTPIXEL_ycoord;
wire PUTPIXEL_inrange;
// X and Y coordinates from arg1 and arg2. Pull lowest 6 bits (since 64x64)
// to get local coordinates
assign PUTPIXEL_xcoord = arg1[5:0];
assign PUTPIXEL_ycoord = arg2[5:0];
// Use upper bits to determine if we are in range. If we're not the target,
// no need.
assign PUTPIXEL_inrange = (arg1[10:6]==X_INDEX)&(arg2[10:6]==Y_INDEX);

always_comb begin
	capture_instruction = 1'b0;
	next_state = state;
	clear_error = 1'b0;
	set_error = 1'b0;
	set_error_code = 4'b0;
	busy = 1'b1;
	go_fillcolor = 1'b0;
	SETCOORD_capture_coordinates = 1'b0;
	case (state)
	STATE_READY: begin
		busy = 1'b0; // Only state where we're not busy
		// If enable is 0 that means the instruction is meant for other tiles
		if(enable) begin
			// We have received an instruction, trap it so that 
			// multiple requests can be in flight at once
			capture_instruction = 1'b1;
			clear_error = 1'b1;
			next_state = STATE_PREPARE;
		end
	end
	STATE_PREPARE: begin
		// Dispatch instruction.
		// If the instruction can resolve in one cycle, resolve it here
		// without a separate state.
		case(instruction)
			INSTR_BLIT: begin
				// Send line data to screen. Unimplemented as no backing
				// memory yet
				// TODO
				set_error_code = E_UNIMP;
				set_error = 1'b1;
				next_state = STATE_READY;
			end
			INSTR_SETCOORD: begin
				// SETCOORD(x_index, y_index, NC, NC, NC)
				// This one is fine. We can just do this now
				// no need for a separate state for it
				// since it's guaranteed to take one clock
				// Capture coordinates into X_INDEX and Y_INDEX
				// These will be used for future coordinate
				// computations
				SETCOORD_capture_coordinates = 1'b1;
				next_state = STATE_READY;
			end
			INSTR_FILLCOLOR: begin
				// FILLCOLOR(COLOR_INDEX);
				// write COLOR_INDEX into all pixels in screen.
				// Actual details will depend on backing memory.
				// (e.g. word size to write, how many, etc)
				go_fillcolor = 1'b1;
				next_state = STATE_FILLCOLOR;
			end
			INSTR_PUTPIXEL: begin
				// PUTPIXEL(XCOORDINATE, YCOORDINATE, PIXELCOLOR)
				// This one needs to write to memory. Only enter state
				// if we need to
				// Don't know if we need a state or not actually
				if(PUTPIXEL_inrange) begin
					//TODO putpixel
				end else begin
					next_state = STATE_READY;
				end
			end
			default: begin
				set_error_code = E_IINST;
				set_error = 1'b1;
				next_state = STATE_READY;
			end
		endcase
	end
	STATE_FILLCOLOR: begin
		// Here, we're filling the color
		// go_fillcolor was high for 1 clock, now we wait until done_fillcolor
		// asserted
		if(done_fillcolor) begin
			next_state = STATE_READY;
			clear_done_fillcolor = 1'b1; // Tell color fill unit we're finished here
		end
	end
	STATE_PUTPIXEL: begin
		
	end

	endcase
end

always @(posedge clk, negedge rst_n) begin
	if(~rst_n) begin
		instruction <= '0;
		arg1 <= '0;
		arg2 <= '0;
		arg3 <= '0;
		arg4 <= '0;
		arg5 <= '0;
	end else if(capture_instruction) begin
		instruction <= instruction_in;
		arg1 <= arg1_in;
		arg2 <= arg2_in;
		arg3 <= arg3_in;
		arg4 <= arg4_in;
		arg5 <= arg5_in;
	end
end

always @(posedge clk, negedge rst_n) begin
	if(~rst_n) begin
		error_code <= E_NOERR;
	end else if(set_error) begin
		error_code <= set_error_code;
	end else if(clear_error) begin
		error_code <= E_NOERR;
	end
end

endmodule
`default_nettype wire

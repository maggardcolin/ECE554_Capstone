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

logic set_error;
logic[3:0] set_error_code;
logic clear_error;
logic capture_instruction;
logic SETCOORD_capture_coordinates;

always @(posedge clk, negedge rst_n) begin
	if(~rst_n) begin
		X_INDEX <= '0;
		Y_INDEX <= '0;
	end else if(SETCOORD_capture_coordinates) begin
		X_INDEX <= arg1[4:0];
		Y_INDEX <= arg2[4:0];
	end
end

always_comb begin
	capture_instruction = 1'b0;
	next_state = state;
	clear_error = 1'b0;
	set_error = 1'b0;
	set_error_code = 4'b0;
	busy = 1'b1;
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
		case(instruction)
			INSTR_BLIT: begin
				set_error_code = E_UNIMP;
				set_error = 1'b1;
				next_state = STATE_READY;
			end
			INSTR_SETCOORD: begin
				// SETCOORD(x_index, y_index, NC, NC, NC)
				// This one is fine. We can just do this now
				// no need for a separate state for it
				// since it's guaranteed to take one clock
				SETCOORD_capture_coordinates = 1'b1;
				next_state = STATE_READY;
			end
			INSTR_FILLCOLOR: begin
				// FILLCOLOR(COLOR_INDEX);
				set_error_code = E_UNIMP;
				set_error = 1'b1;
				next_state = STATE_READY;
			end
		endcase
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

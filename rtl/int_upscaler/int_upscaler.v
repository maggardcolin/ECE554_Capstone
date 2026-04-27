
`timescale 1 ns / 1 ps

	module int_upscaler #
	(
        parameter integer NUM_PIXEL = 4,
        parameter integer UPSCALE_VAL = 3
	)
	(
	    input wire clk,
	    input wire rst_n,
	
		// Ports of Axi Slave Bus Interface S00_AXIS
		output wire  s00_axis_tready,
		input wire [(NUM_PIXEL*16)-1 : 0] s00_axis_tdata,
		input wire [((NUM_PIXEL*16)/8)-1 : 0] s00_axis_tstrb,
		input wire  s00_axis_tlast,
		input wire  s00_axis_tvalid,

		// Ports of Axi Master Bus Interface M00_AXIS
		output wire  m00_axis_tvalid,
		output wire [(NUM_PIXEL*16*UPSCALE_VAL)-1 : 0] m00_axis_tdata,
		output wire [((NUM_PIXEL*16*UPSCALE_VAL)/8)-1 : 0] m00_axis_tstrb,
		output wire  m00_axis_tlast,
		input wire  m00_axis_tready
	);
	
	assign s00_axis_tready = m00_axis_tready;
	assign m00_axis_tlast = s00_axis_tlast;
	assign m00_axis_tvalid = s00_axis_tvalid;
	assign m00_axis_tstrb = {UPSCALE_VAL{s00_axis_tstrb}};
	
	generate
	   for (genvar i = 0; i < NUM_PIXEL; i = i + 1) begin
	       assign m00_axis_tdata[(16*UPSCALE_VAL*i)+ (UPSCALE_VAL*16-1):(16*UPSCALE_VAL*i)] = {UPSCALE_VAL{s00_axis_tdata[(16*i)+15:(16*i)]}};
	   end
	endgenerate

	endmodule


`timescale 1 ns / 1 ps

	module pixel_cntr #
	(
		parameter integer NUM_PIXEL = 4,
		parameter integer UPSCALE_VAL = 3,
		parameter integer VRES = 720,
		parameter integer SLICE_HEIGHT = 64
	)
	(
		input wire clk,
		input wire rst_n,
		
		// Ports of Axi Master Bus Interface M00_AXIS
		output wire  m00_axis_tvalid,
		output wire [(NUM_PIXEL*16)-1 : 0] m00_axis_tdata,
		output wire [(((NUM_PIXEL*16))/8)-1 : 0] m00_axis_tstrb,
		output reg  m00_axis_tlast,
		input wire  m00_axis_tready,

		// Ports of Axi Slave Bus Interface S00_AXIS
		output wire  s00_axis_tready,
		input wire [(NUM_PIXEL*16)-1 : 0] s00_axis_tdata,
		input wire [(((NUM_PIXEL*16))/8)-1 : 0] s00_axis_tstrb,
		input wire  s00_axis_tlast,
		input wire  s00_axis_tvalid,
		
		output wire DMA_en
	);
	
	assign m00_axis_tvalid = s00_axis_tvalid;
	assign m00_axis_tdata = s00_axis_tdata;
	assign m00_axis_tstrb = s00_axis_tstrb;
	assign s00_axis_tready = m00_axis_tready;
	assign DMA_en = m00_axis_tready;
	
	localparam integer PX_CAP = UPSCALE_VAL * UPSCALE_VAL * VRES * SLICE_HEIGHT;
//	localparam integer PX_CAP = 16*SLICE_HEIGHT;
	localparam integer BEAT_CAP = PX_CAP / NUM_PIXEL;
	
	wire beat;
	assign beat = s00_axis_tvalid & m00_axis_tready;
	
	reg [$clog2(BEAT_CAP):0] cntr;
	
	always @(posedge clk, negedge rst_n) begin
	   if (~rst_n) begin
	       cntr <= 0;
	       m00_axis_tlast <= 1'b0;
	   end else if (beat) begin
	       if (cntr == (BEAT_CAP - 2)) begin
	           m00_axis_tlast <= 1'b1;
	           cntr <= 0;
	       end else begin
	           m00_axis_tlast <= 1'b0;
               cntr <= cntr + 1;
	       end
	   end else begin
	       m00_axis_tlast <= 1'b0;
	       cntr <= 0;
	   end
	end

	endmodule

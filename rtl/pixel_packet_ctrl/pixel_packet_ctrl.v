
`timescale 1 ns / 1 ps

	module pixel_packet_ctrl #
	(
		parameter integer NUM_PIXEL = 4,
		parameter integer UPSCALE_VAL = 3
	)
	(
	    input wire clk,
	    input wire rst_n,


		// Ports of Axi Slave Bus Interface S00_AXIS
		output reg  s00_axis_tready,
		input wire [(NUM_PIXEL * 16 * UPSCALE_VAL)-1 : 0] s00_axis_tdata,
		input wire [((NUM_PIXEL * 16 * UPSCALE_VAL)/8)-1 : 0] s00_axis_tstrb,
		input wire  s00_axis_tlast,
		input wire  s00_axis_tvalid,

		// Ports of Axi Master Bus Interface M00_AXIS
		output reg  m00_axis_tvalid,
		output reg [(NUM_PIXEL * 16)-1 : 0] m00_axis_tdata,
		output wire [((NUM_PIXEL * 16)/8)-1 : 0] m00_axis_tstrb,
		output reg  m00_axis_tlast,
		input wire  m00_axis_tready
	);
	
	assign m00_axis_tstrb = s00_axis_tstrb;

    localparam STATE_WAIT_FOR_DATA = 2'b00;
	localparam STATE_WAIT_FOR_UPSTREAM = 2'b01;
	localparam STATE_SEND_DATA = 2'b11;
	
	reg [(NUM_PIXEL * 16 * UPSCALE_VAL)-1 : 0] data_buf;
	
	reg [$clog2(UPSCALE_VAL):0] cntr;
	
	reg [1:0] state, next_state;
	
	always @(posedge clk, negedge rst_n) begin
	   if (~rst_n) begin
	       state <= STATE_WAIT_FOR_DATA;
	   end else begin
	       state <= next_state;
	   end
	end
	
	always @(posedge clk) begin
	   if (s00_axis_tready && s00_axis_tvalid)
	       data_buf <= s00_axis_tdata;
	end
	
	always @(*) begin
	   s00_axis_tready = 1'b0;
	   m00_axis_tvalid = 1'b0;
	   m00_axis_tdata  = 0;
       m00_axis_tlast  = 1'b0;
	   next_state = state;
	   
	   case (state)
	       STATE_WAIT_FOR_DATA: begin
	           s00_axis_tready = 1'b1;
	           if (s00_axis_tvalid)
	               next_state = STATE_WAIT_FOR_UPSTREAM;
	       end
	       
	       STATE_WAIT_FOR_UPSTREAM: begin
	           if (m00_axis_tready) next_state = STATE_SEND_DATA;
	       end
	       
	       STATE_SEND_DATA: begin
	           m00_axis_tvalid = 1'b1;
	           m00_axis_tdata  = data_buf >> (cntr << 6);
	           
	           if (cntr == (UPSCALE_VAL - 1)) begin
	               m00_axis_tlast = 1'b1;
	               next_state = STATE_WAIT_FOR_DATA;
	           end else begin
	               m00_axis_tlast = 1'b0;
	               next_state = STATE_WAIT_FOR_UPSTREAM;
	           end
	       end
	   endcase
	end
	
	always @(posedge clk, negedge rst_n) begin
	   if (~rst_n) cntr <= 0;
	   else if (state == STATE_SEND_DATA) cntr <= cntr + 1;
	end
	

	endmodule

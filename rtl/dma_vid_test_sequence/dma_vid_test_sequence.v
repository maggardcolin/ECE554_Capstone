
`timescale 1 ns / 1 ps

	module dma_vid_test_sequence #
	(
		parameter integer NUM_PIXEL = 4
	)
	(
		input wire  clk,
		input wire  rst_n,
		
		output wire  m00_axis_tvalid,
		output wire [(NUM_PIXEL*4)-1 : 0] m00_axis_tdata,
		output wire [((NUM_PIXEL*4)/8)-1 : 0] m00_axis_tstrb,
		output wire  m00_axis_tlast,
		input wire  m00_axis_tready
	);

	assign m00_axis_tstrb = 16'hFFFF;

	reg [1:0] cntr;
	
    always @(posedge clk, negedge rst_n) begin
        if (~rst_n) begin
            cntr <= 0;
        end else if (m00_axis_tready) begin
            cntr <= cntr + 1;
        end
    end
    
    assign m00_axis_tdata = {NUM_PIXEL{1'b0,1'b0,cntr}};
    assign m00_axis_tvalid = 1'b1;
    assign m00_axis_tlast  = 1'b1;

	endmodule

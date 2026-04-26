
`timescale 1 ns / 1 ps

	module color_mapper #
	(
        parameter integer NUM_PIXELS = 4
	)
	(
	    input wire clk,
	    input wire rst_n,
	
		// Ports of Axi Slave Bus Interface S00_AXIS
		output wire  s00_axis_tready,
		input wire [(NUM_PIXELS * 4)-1 : 0] s00_axis_tdata,
		input wire [((NUM_PIXELS * 4)/8)-1 : 0] s00_axis_tstrb,
		input wire  s00_axis_tlast,
		input wire  s00_axis_tvalid,

		// Ports of Axi Master Bus Interface M00_AXIS
		output wire  m00_axis_tvalid,
		output wire [(NUM_PIXELS * 16)-1 : 0] m00_axis_tdata,
		output wire [((NUM_PIXELS * 16)/8)-1 : 0] m00_axis_tstrb,
		output wire  m00_axis_tlast,
		input wire  m00_axis_tready
	);
	
	assign s00_axis_tready = m00_axis_tready;
	assign m00_axis_tlast = s00_axis_tlast;
	assign m00_axis_tstrb = s00_axis_tstrb;
	assign m00_axis_tvalid = {4{s00_axis_tvalid}};
	
    generate
        for (genvar i = 0; i < NUM_PIXELS; i = i + 1) begin
            reg [15:0] color;
            always @(*) begin
                case (s00_axis_tdata[(4*i)+3:(4*i)])
                    4'b0000: color = 16'b00000_000000_00000;
                    4'b0001: color = 16'b00000_000000_11111;
                    4'b0010: color = 16'b00000_111111_00000;
                    4'b0011: color = 16'b11111_000000_00000;
                    default: color = 16'b11111_111111_11111;
                endcase
            end
            assign m00_axis_tdata[(16*i)+15:(16*i)] = {<<{color}};
        end
    endgenerate

	endmodule

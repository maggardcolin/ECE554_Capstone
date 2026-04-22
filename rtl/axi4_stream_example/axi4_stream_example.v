
`timescale 1 ns / 1 ps

	module axi4_stream_example #
	(
		// Users to add parameters here

		// User parameters ends
		// Do not modify the parameters beyond this line


		// Parameters of Axi Master Bus Interface M00_AXIS
		parameter integer C_M00_AXIS_TDATA_WIDTH	= 32,
		parameter integer C_M00_AXIS_START_COUNT	= 32
	)
	(
		// Users to add ports here

		// User ports ends
		// Do not modify the ports beyond this line


		// Ports of Axi Master Bus Interface M00_AXIS
		input wire  m00_axis_aclk,
		input wire  m00_axis_aresetn,
		output reg  m00_axis_tvalid,
		output reg [C_M00_AXIS_TDATA_WIDTH-1 : 0] m00_axis_tdata,
		output wire [(C_M00_AXIS_TDATA_WIDTH/8)-1 : 0] m00_axis_tstrb,
		output reg  m00_axis_tlast,
		input wire  m00_axis_tready
	);

	// Add user logic here
    reg [C_M00_AXIS_TDATA_WIDTH-1:0] counter = 32'h0;
    
    always @(posedge m00_axis_aclk, negedge m00_axis_aresetn) begin
        if (~m00_axis_aresetn) begin
            counter <= 32'h0;
            m00_axis_tvalid <= 1'b0;
            m00_axis_tdata <= 32'h0;
            m00_axis_tlast <= 1'b0;
        end
        else begin
            if (m00_axis_tready & m00_axis_tvalid) begin
                counter <= counter + 1'b1;
                m00_axis_tdata <= counter;
                
                if (&counter[9:0]) begin
                    m00_axis_tlast <= 1'b1;
                end else begin
                    m00_axis_tlast <= 1'b0;
                end
            end
            
            m00_axis_tvalid <= 1'b1;
        end
    end
    
    assign m00_axis_tstrb = 4'b1111;
	// User logic ends

	endmodule


`timescale 1 ns / 1 ps

	module bram_stream_controller #
	(
		parameter integer NUM_PIXEL = 4,
		parameter integer READ_DELAY = 2
	)
	(
		input wire [NUM_PIXEL*4-1:0] bram_read,
		output reg DMA_en,
		output reg [5:0] DMA_row,
		output reg [5:0] rendertile_addr,

		// Ports of Axi Master Bus Interface M00_AXIS
		input wire  clk,
		input wire  rst_n,
		output reg  m00_axis_tvalid,
		output wire [NUM_PIXEL*4-1 : 0] m00_axis_tdata,
		output wire [(NUM_PIXEL*4/8)-1 : 0] m00_axis_tstrb,
		output reg  m00_axis_tlast,
		input wire  m00_axis_tready
	);
	
	// assign m00_axis_tlast = 1'b1;
	assign m00_axis_tstrb = {(NUM_PIXEL*4/8){1'b1}};
	// assign m00_axis_tdata = {<<{bram_read}};
	
	generate
	   for (genvar i = 0; i < NUM_PIXEL*4; i = i + 16) begin
           for (genvar j = 0; j < 4; j = j + 1) begin
               assign m00_axis_tdata[(j*4 + i) +: 4] = bram_read[((12-j*4) + i) +: 4];
           end
	   end
	endgenerate
	
	reg [$clog2(READ_DELAY):0] clk_delay;
	reg streaming;
	
	localparam integer LAST_COL = 19;
    localparam integer LAST_ROW = 63;
    
    reg [6:0] rst_delay_chain;
    
    always @(posedge clk, negedge rst_n) begin
        if (~rst_n) begin
            DMA_en <= 1'b0;
        end else if (&clk_delay) begin
            if (m00_axis_tlast) DMA_en <= 1'b0;
            else if (m00_axis_tready) DMA_en <= 1'b1;
        end
    end
    
    always @(posedge clk, negedge rst_n) begin
        if (~rst_n) begin
            DMA_en <= 1'b0;
            DMA_row <= 0;
            rendertile_addr <= 0;
            
            clk_delay <= {($clog2(READ_DELAY)+1){1'b0}};
            streaming <= 1'b0;
            
            m00_axis_tvalid <= 1'b0;
            m00_axis_tlast <= 1'b1;
            rst_delay_chain <= 0;
        end else begin
            if (~&rst_delay_chain) begin
                rst_delay_chain <= rst_delay_chain + 1;
            end else begin
                if (~streaming) begin
                    if (m00_axis_tready && clk_delay < (READ_DELAY - 1)) begin
                        clk_delay <= clk_delay + 1'b1;
                        m00_axis_tvalid <= 1'b0;
                        m00_axis_tlast <= 1'b0;
                    end else if (m00_axis_tready) begin
                        streaming <= 1'b1;
                        m00_axis_tvalid <= 1'b1;
                        m00_axis_tlast <= 1'b0;
                        clk_delay <= clk_delay;
                    end else begin
                        streaming <= 1'b0;
                        m00_axis_tvalid <= 1'b0;
                        m00_axis_tlast <= 1'b0;
                        clk_delay <= {($clog2(READ_DELAY)+1){1'b0}};
                    end
                end else begin
                    m00_axis_tvalid <= 1'b1;
                    
                    if (DMA_row == LAST_ROW && rendertile_addr == (LAST_COL - 1))
                        m00_axis_tlast <= 1'b1;
                        
                    if (m00_axis_tvalid & m00_axis_tready) begin
                        if (m00_axis_tlast) begin
                            streaming <= 1'b0;
                            m00_axis_tvalid <= 1'b0;
                            m00_axis_tlast <= 1'b0;
                            
                            DMA_row <= 0;
                            rendertile_addr <= 0;
                            clk_delay <= {($clog2(READ_DELAY)+1){1'b0}};
                        end else begin
                            if (rendertile_addr == LAST_COL - READ_DELAY) begin
                                DMA_row <= DMA_row + 1'b1;
                            end else begin
                                DMA_row <= DMA_row;
                            end
                            if (rendertile_addr == LAST_COL) begin
                                rendertile_addr <= 0;
                            end else begin
                                rendertile_addr <= rendertile_addr + 1'b1;
                            end
                        end
                    end
                end
            end
        end
    end

	
//	always @(posedge clk, negedge rst_n) begin
//	   if (~rst_n) begin
//	       clk_delay <= 0;
//	       m00_axis_tvalid <= 1'b0;
//	       DMA_en <= 1'b0;
//	   end else if (m00_axis_tready) begin
//	       DMA_en <= 1'b1;
//	       if (clk_delay >= (READ_DELAY - 1)) begin
//	           m00_axis_tvalid <= 1'b1;
//	       end else begin
//	           clk_delay <= clk_delay + 1'b1;
//	           m00_axis_tvalid <= 1'b0;
//           end
//	   end else if (m00_axis_tlast) begin
//	       DMA_en <= 1'b0;
//	       m00_axis_tvalid <= 1'b0;
//	       clk_delay <= 0;
//	   end
//	end
	
//	always @(posedge clk, negedge rst_n) begin
//	   if (~rst_n) begin
//	       DMA_row <= 0;
//	       rendertile_addr <= 0;
//	       m00_axis_tlast <= 1'b0;
//	   end else begin
//	       if (m00_axis_tvalid) begin
//	           rendertile_addr <= rendertile_addr + 1;
//	           if (rendertile_addr == (19 - READ_DELAY))
//	               DMA_row <= DMA_row + 1;
//	           if (rendertile_addr == 19)
//	               rendertile_addr <= 0;
//	           if (&DMA_row && rendertile_addr == 18)
//	               m00_axis_tlast <= 1'b1;
//	       end else if (m00_axis_tlast) begin
//               m00_axis_tlast <= 1'b0;
//               rendertile_addr <= 0;
//	           DMA_row <= 0;
//	       end
//	   end
//	end
	

	endmodule

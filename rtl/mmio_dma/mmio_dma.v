
`timescale 1 ns / 1 ps

	module mmio_dma #
	(
		// Users to add parameters here

		// User parameters ends
		// Do not modify the parameters beyond this line


		// Parameters of Axi Slave Bus Interface S00_AXI
		parameter integer C_S00_AXI_DATA_WIDTH	= 32,
		parameter integer C_S00_AXI_ADDR_WIDTH	= 6
	)
	(
		// Users to add ports here
	output		wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg0,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg1,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg2,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg3,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg4,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg5,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg6,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg7,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg8,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg9,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg10,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg11,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg12,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg13,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg14,
output	wire [C_S00_AXI_DATA_WIDTH-1:0]	slv_reg15,
output reg interrupt,
output wire[3:0] regdest,
		// User ports ends
		// Do not modify the ports beyond this line


		// Ports of Axi Slave Bus Interface S00_AXI
		input wire  s00_axi_aclk,
		input wire  s00_axi_aresetn,
		input wire [C_S00_AXI_ADDR_WIDTH-1 : 0] s00_axi_awaddr,
		input wire [2 : 0] s00_axi_awprot,
		input wire  s00_axi_awvalid,
		output wire  s00_axi_awready,
		input wire [C_S00_AXI_DATA_WIDTH-1 : 0] s00_axi_wdata,
		input wire [(C_S00_AXI_DATA_WIDTH/8)-1 : 0] s00_axi_wstrb,
		input wire  s00_axi_wvalid,
		output wire  s00_axi_wready,
		output wire [1 : 0] s00_axi_bresp,
		output wire  s00_axi_bvalid,
		input wire  s00_axi_bready,
		input wire [C_S00_AXI_ADDR_WIDTH-1 : 0] s00_axi_araddr,
		input wire [2 : 0] s00_axi_arprot,
		input wire  s00_axi_arvalid,
		output wire  s00_axi_arready,
		output wire [C_S00_AXI_DATA_WIDTH-1 : 0] s00_axi_rdata,
		output wire [1 : 0] s00_axi_rresp,
		output wire  s00_axi_rvalid,
		input wire  s00_axi_rready
	);
// Instantiation of Axi Bus Interface S00_AXI
	mmio_dma_slave_lite_v1_0_S00_AXI # ( 
		.C_S_AXI_DATA_WIDTH(C_S00_AXI_DATA_WIDTH),
		.C_S_AXI_ADDR_WIDTH(C_S00_AXI_ADDR_WIDTH)
	) mmio_dma_slave_lite_v1_0_S00_AXI_inst (
		.S_AXI_ACLK(s00_axi_aclk),
		.S_AXI_ARESETN(s00_axi_aresetn),
		.S_AXI_AWADDR(s00_axi_awaddr),
		.S_AXI_AWPROT(s00_axi_awprot),
		.S_AXI_AWVALID(s00_axi_awvalid),
		.S_AXI_AWREADY(s00_axi_awready),
		.S_AXI_WDATA(s00_axi_wdata),
		.S_AXI_WSTRB(s00_axi_wstrb),
		.S_AXI_WVALID(s00_axi_wvalid),
		.S_AXI_WREADY(s00_axi_wready),
		.S_AXI_BRESP(s00_axi_bresp),
		.S_AXI_BVALID(s00_axi_bvalid),
		.S_AXI_BREADY(s00_axi_bready),
		.S_AXI_ARADDR(s00_axi_araddr),
		.S_AXI_ARPROT(s00_axi_arprot),
		.S_AXI_ARVALID(s00_axi_arvalid),
		.S_AXI_ARREADY(s00_axi_arready),
		.S_AXI_RDATA(s00_axi_rdata),
		.S_AXI_RRESP(s00_axi_rresp),
		.S_AXI_RVALID(s00_axi_rvalid),
		.S_AXI_RREADY(s00_axi_rready),
		.slv_reg0(slv_reg0),
		.slv_reg1(slv_reg1),
		.slv_reg2(slv_reg2),
		.slv_reg3(slv_reg3),
		.slv_reg4(slv_reg4),
		.slv_reg5(slv_reg5),
		.slv_reg6(slv_reg6),
		.slv_reg7(slv_reg7),
		.slv_reg8(slv_reg8),
		.slv_reg9(slv_reg9),
		.slv_reg10(slv_reg10),
		.slv_reg11(slv_reg11),
		.slv_reg12(slv_reg12),
		.slv_reg13(slv_reg13),
		.slv_reg14(slv_reg14),
		.slv_reg15(slv_reg15),
		.regdest(regdest)
	);

	// Add user logic here
//    assign interrupt = s00_axi_awvalid;
always @(posedge s00_axi_aclk, negedge s00_axi_aresetn) begin
if(~s00_axi_aresetn)
    interrupt <= 1'b0;
else
    interrupt <= s00_axi_awvalid;
end
    //assign regdest = s00_axi_awaddr[3:0];
	// User logic ends

	endmodule

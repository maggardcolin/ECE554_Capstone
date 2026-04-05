module uram_64bit_primitive_wrapper (
    input  wire        clk,
    input  wire        rst,
    
    // Port A: Write Only
    input  wire        we_a,   // Write Enable
    input  wire [11:0] addr_a, // 4096 depth (12-bit)
    input  wire [63:0] din_a,  // 64-bit Data Input
    
    // Port B: Read Only
    input  wire        re_b,   // Read Enable
    input  wire [11:0] addr_b, // 4096 depth (12-bit)
    output wire [63:0] dout_b  // 64-bit Data Output
);

    // Internal wires for the 72-bit interface
    wire [71:0] dout_b_72;

    URAM288 #(
        .ADDR_WIDTH_A(12),               // Address width
        .ADDR_WIDTH_B(12),
        .DATA_WIDTH_A(72),               // Physical width is 72
        .DATA_WIDTH_B(72),
        .EN_ECC_RD_A("FALSE"),           // Disable ECC
        .EN_ECC_RD_B("FALSE"),
        .EN_ECC_WR_A("FALSE"),
        .EN_ECC_WR_B("FALSE"),
        .OREG_A("FALSE"),                // Output registers (set to TRUE for better timing)
        .OREG_B("FALSE"),
        .RST_POLARITY_A("ACTIVE_HIGH"),
        .RST_POLARITY_B("ACTIVE_HIGH"),
        .USE_EXT_CE_A("FALSE"),
        .USE_EXT_CE_B("FALSE")
    ) uram_inst (
        // Port A (Write Only)
        .ADDR_A(addr_a),
        .DIN_A({8'h00, din_a}),          // Pad 64-bit to 72-bit
        .EN_A(1'b1),                     // Port A always enabled
        .WE_A({9{we_a}}),                // Write all bytes when we_a is high
        .RST_A(rst),
        .CLK_A(clk),
        .BWE_A(9'h1FF),                  // Byte Write Enable (all bits)
        .DBITERR_A(),                    // Unused ECC outputs
        .SBITERR_A(),
        
        // Port B (Read Only)
        .ADDR_B(addr_b),
        .DIN_B(72'h0),                   // Port B input unused
        .EN_B(re_b),
        .WE_B(9'h000),                   // Never write on Port B
        .RST_B(rst),
        .CLK_B(clk),
        .BWE_B(9'h000),
        .DOUT_B(dout_b_72),
        .DBITERR_B(),
        .SBITERR_B(),
        
        // Unused Scan/Sleep signals
        .INJECT_DBITERR_A(1'b0),
        .INJECT_SBITERR_A(1'b0),
        .INJECT_DBITERR_B(1'b0),
        .INJECT_SBITERR_B(1'b0),
        .OREG_CE_A(1'b1),
        .OREG_CE_B(1'b1),
        .OREG_ECC_QUITE_A(1'b0),
        .OREG_ECC_QUITE_B(1'b0),
        .SLEEP(1'b0)
    );

    // Truncate output back to 64 bits
    assign dout_b = dout_b_72[63:0];

endmodule

/* SPDX-License-Identifier: [MIT] */
`timescale 1ns / 1ps
`default_nettype wire

// This mini spi slave uses mode 0
//    CPOL = 0
//    CPHA = 0
//
//  This means the slave drives valid data on MISO when chip select goes low
//  (and master idles with clock signal low), samples on rising edge of spi
//  clock, shifts on falling edge.
//
//  MISO and MOSI are connected via the 16 bit shift register. Therefore, any
//  data shifted in will be shifted out and delayed by 16 SPI clock cycles.
//
//  The shift register is parallel loaded using data_i on the falling edge of
//  chip select. Data is shifted most significant bit first.

module minispi(
    input  clk,

    input  spi_clk,
    output spi_miso,
    input  spi_mosi,
    input  spi_csn,
    input  [15:0] data_i,
    output [15:0] data_o,
     
    output spi_sot, // start of transaction, chip select falling edge
    output spi_eot  // end of transaction, chip select rising edge
);

    reg [15:0] spi_shift;
    assign data_o = spi_shift;
    assign spi_miso = spi_shift[15];

    reg       spi_mosi_smp;
    reg [2:0] spi_clk_sync;
    reg [2:0] spi_csn_sync;
    wire csn_dn = (spi_csn_sync[2:1] == 2'b10);
    wire csn_up = (spi_csn_sync[2:1] == 2'b01);
    wire clk_dn = (spi_clk_sync[2:1] == 2'b10);
    wire clk_up = (spi_clk_sync[2:1] == 2'b01);
     
    assign spi_sot = csn_dn;
    assign spi_eot = csn_up;

    always @(posedge clk) begin
        spi_clk_sync <= {spi_clk_sync[1:0], spi_clk};
        spi_csn_sync <= {spi_csn_sync[1:0], spi_csn};
          
        // Sample MOSI on rising clock rising edge
        if (clk_up)
            spi_mosi_smp <= spi_mosi;

        // Load spi shift register on chip select assertion, shift it on spi
        // clock negedge
        if (csn_dn)
            spi_shift <= data_i;
        else if (clk_dn)
            spi_shift <= {spi_shift[14:0], spi_mosi_smp};

    end

endmodule

/* SPDX-License-Identifier: [MIT] */
`default_nettype wire
`timescale 1ns / 1ps

module pulsemeter(
    input  clk,
    input  spi_clk,
    output spi_miso,
    input  spi_mosi,
    input  spi_csn,

    input  pulse_in,
    output reg led_d2,
    output reg led_d3
);
 
    reg [15:0]  pulse_cnt = 16'h0;
    wire        pulse;
    wire        spi_sot;
    wire        spi_eot;
    wire [15:0] data_o;
 
    always @(posedge clk) begin
        // If pulse arrive on same clock as start of transaction then
        // initialize the accumulator to 1 instead of zero to avoid missing
        // a pulse.
        if (spi_sot)
            pulse_cnt <= (pulse) ? 1 : 0;
        else if (pulse)
            pulse_cnt <= pulse_cnt + 1;

        // Register LED state on end of transaction. They're active
        // low so invert.
        if (spi_eot)
            {led_d3, led_d2} <= ~data_o[1:0];
    end
 
    minispi spi(
        .clk(clk),
        .spi_clk(spi_clk),  .spi_csn(spi_csn),
        .spi_miso(spi_miso),.spi_mosi(spi_mosi),
        .data_i(pulse_cnt), .data_o(data_o),
        .spi_sot(spi_sot),  .spi_eot(spi_eot)
    );
    
    debounce db(.clk(clk), .sig(pulse_in), .pulse(pulse));

endmodule


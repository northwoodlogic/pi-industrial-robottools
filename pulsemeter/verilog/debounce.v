/* SPDX-License-Identifier: [MIT] */
`timescale 1ns / 1ps
`default_nettype wire

// pulse-in synchronizer and debounce.
// uses this debouncer tutorial as a reference:
//   https://www.fpga4fun.com/Debouncer2.html
//

module debounce(
    input  clk,  // system clock, 1.8432MHz on servo board
    input  sig,  // active low async input from pulse meter
    output pulse // synchronized and debounced negedge detected output flag
);

	// The debounce period is ~139uS with an 8-bit counter and 1.8432MHz
	// clock. This assumes the pulse meter input is wired in an active low
	// configuration. At power up, if the input signal is pulled high and no
	// active pulses being generated then there will be no pulse output
	// glitches. 

    reg state = 1;
    reg [7:0] cnt = 8'h0;
    reg [1:0] sync = 2'h0;

    wire max = &cnt;

    wire idle = (state == sync[1]);
    assign pulse = ~idle & max & state;

    always @(posedge clk) begin
        sync <= {sync[0], sig};

        if (idle)
            cnt <= 8'h0;

        else begin
            cnt <= cnt + 8'h1;
            if (max)
                state <= ~state;
            
        end
    end

endmodule


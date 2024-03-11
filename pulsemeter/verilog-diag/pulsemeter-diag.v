/* SPDX-License-Identifier: [MIT] */
`default_nettype wire
`timescale 1ns / 1ps

module pulsemeter(
    input  clk,

    output uart_tx,
    input  uart_rx,

    input opto_ch0, // 240Vac pressure switch status
    input opto_ch1, // spare
    input opto_ch2, // 24Vac power good status
    inout opto_ch3, // 24Vac relay coil enable

    output sig_0,   // 7J1-22 | line=79, relay coil inhibit status
    output sig_1,   // 7J1-29 | line=96, pressure switch sts (io_sel=1) or 24vac sts (io_sel=0)
    input  sig_2,   // 7J1-32 | line=95, io_sel, selects pressure switch sts or 24vac good sts on sig_1
    input  sig_3,   // 7J1-36 | line=81, UART enable, todo; fix this so loopback enabled when this is hi, 
    input  sig_4,   // 7J1-38 | line=82, Relay coil inhibit clear, active falling edge
    input  sig_5,   // 7J1-40 | line=83, Relay coid inhibit set, active falling edge

    input  pulse_in,
    output led_d2,
    output led_d3
);

    wire io_sel  = ~sig_2;
    wire uart_en = ~sig_3;

    wire  io_24vac_sts = ~opto_ch2;
    wire  io_prswt_sts = ~opto_ch0;

    reg coil_inhibit = 1'b0;
    assign sig_0 = coil_inhibit;
    assign sig_1 = io_sel ? io_24vac_sts : io_prswt_sts;

    wire coil_inhibit_clr = ~sig_4;
    wire coil_inhibit_set = ~sig_5;

    reg [2:0] coil_inhibit_clr_sync;
    reg [2:0] coil_inhibit_set_sync;

    /*
     * Relay rack is active low. The coil enable output is on channel 3.
     * Enable the coil only if it's not inhibited and the pressure switch is
     * on.
     */ 
    wire   coil_on  = ~coil_inhibit & io_prswt_sts;
    assign opto_ch3 = (coil_on) ? 1'b0 : 1'bz; 

    /*
     * The LED indicators are active low, indicate coil and coil inhibit
     * status.
     */
    assign led_d2 = ~coil_on;
    assign led_d3 = ~coil_inhibit;

    wire set_inhibit = (coil_inhibit_set_sync[2:1] == 2'b01);
    wire clr_inhibit = (coil_inhibit_clr_sync[2:1] == 2'b01);
 
    always @(posedge clk) begin
        coil_inhibit_clr_sync <= {coil_inhibit_clr_sync[1:0], coil_inhibit_clr};
        coil_inhibit_set_sync <= {coil_inhibit_set_sync[1:0], coil_inhibit_set};

        if (set_inhibit)
            coil_inhibit <= 1'b1;
        else if (clr_inhibit)
            coil_inhibit <= 1'b0;
    end


    reg [7:0] pulse_cnt = 8'h0;
    wire pulse;


    reg [2:0] uart_rx_sync;
    wire txd;
    wire tx_busy;
    wire tx_go = (uart_rx_sync[2:1] == 2'b10) & ~tx_busy;
    assign uart_tx = uart_en ? txd : uart_rx;
    // wire [7:0] uart_tx_data = uart_en_pat ? 8'h21 : pulse_cnt;
    wire [7:0] uart_tx_data = pulse_cnt;

    always @(posedge clk) begin
        uart_rx_sync <= {uart_rx_sync[1:0], uart_rx};
    end

    muart_tx #(
        .DATA_BITS(8),
        .BAUD_GEN_WIDTH(4),
        .BAUD_GEN_INCRM(1)
    ) xmitter (
        .clk(clk),
        .rst(1'b0),
        .data_i(uart_tx_data),
        .go(tx_go),
        .busy(tx_busy),
        .txd(txd)
    );


    always @(posedge clk) begin
        if (tx_go)
            pulse_cnt <= (pulse) ? 1 : 0;
        else if (pulse)
            pulse_cnt <= pulse_cnt + 1;
        // this is test code, gives a way to increment the pulse counter from
	// the host system in a controlled way.
        else if (clr_inhibit)
	    pulse_cnt <= pulse_cnt + 1;
    end

    debounce db(.clk(clk), .sig(pulse_in), .pulse(pulse));
 

endmodule

module muart_tx
#(
    parameter DATA_BITS = 8,
    parameter BAUD_GEN_WIDTH = 16,
    parameter BAUD_GEN_INCRM = 151
)(
    input clk,
    input rst,
    input [DATA_BITS-1:0] data_i,
    input go,
    output busy,
    output txd
);

    parameter  state_idle = 1'h0;
    parameter  state_shift = 1'h1;
    reg        state;

    /*
     * N + 2 bits of data are shifted out: 1 start, N data, and 1 stop.
     */
    reg [DATA_BITS+1:0] shift;
    reg [3:0] shift_cnt;

    /*
     * This is a fractional baud rate generator
     * reg [BAUD_GEN_WIDTH:0] baud_gen;
     * wire baud_tick = baud_gen[BAUD_GEN_WIDTH];
     */

    /*
     * This is a modulo baud rate generator
     */
     reg [BAUD_GEN_WIDTH-1:0] baud_gen;
     wire baud_tick = &baud_gen;
     

    assign txd = shift[0];
    assign busy = (state != state_idle);

    always @(posedge clk) begin

        if (rst) begin
            baud_gen    <= 0;
            state       <= 0;
            shift <= {DATA_BITS+2{1'b1}};
            shift_cnt   <= 0;
        end else begin

            // baud_gen <= baud_gen[BAUD_GEN_WIDTH-1:0] + BAUD_GEN_INCRM;
            baud_gen <= baud_gen + BAUD_GEN_INCRM;

            case (state)
                /*
                 * Hang around waiting for the 'go' signal. On assertion
                 * latch incoming data byte along with start and stop bits
                 * into the shift register. Zero out the baud rate
                 * accumulator to minimize typical baud tick latency that
                 * other implementations may incur.
                 */
                state_idle: begin
                    shift <= {DATA_BITS+2{1'b1}};
                    shift_cnt <= 0;
                    if (go) begin
                        baud_gen <= 0;
                        state <= state_shift;
                        shift <= {1'b1, data_i, 1'b0};
                    end
                end

                /*
                 * Shift until there are no more bits to shift.
                 */
                state_shift: begin
                    if (baud_tick) begin
                        shift <= {1'b1, shift[DATA_BITS+1:1]};
                        shift_cnt <= {shift_cnt + 1'b1};
                        if (shift_cnt == DATA_BITS+1)
                            state <= state_idle;
                    end
                end

                /*
                 * This is impossible to reach, but here anyway to quash
                 * warnings from certain Verilog simulators.
                 */
                default:
                    state <= state_idle;
            endcase
        end
    end

endmodule


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


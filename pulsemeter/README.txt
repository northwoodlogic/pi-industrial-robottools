This code implements a pulse counting peripheral and application for use with
the EKM Metering NSF 61 rated pulse output water meter, model SPWM-075-HD-NSF.
The pulse counter is implemented in a XC9572XL based I/O board which is in
turn connected to a host system via SPI.

A 16-bit pulse count accumulator is atomically reset to zero when read over
the SPI bus. The SPI shift register is loaded and accumulator reset on the
falling edge of chip select. In the unlikely event a pulse-in is detected on
the same clock cycle as a chip select falling edge, the accumulator is
re-initialized to 1 instead of zero to avoid missing counts.

Therefore, monitoring software only needs to "read, add, read, add, etc.."
when measuring total volume or computing flow rate. Analytic functions may
simply use aggregate query type operations when reporting water usage over
time, "select sum(count) from waterlog where date > ...".

The pulse meter blips every 1/100'th of a cubic foot, or approximately one
pulse per 0.075 gallons.

The SPI interface uses Mode 0, CPOL=0 CPHA = 0. A 16-bit accumulator value is
shifted out most significant bit first, big endian byte order. Additional data
shifted beyond 16 bits is looped back from MOSI to MISO. This allows SPI host
controller software to verify a functioning communication interface. For
example, over shifting a known 32-bit pattern will echo back the 16 most
significant bits delayed by 16 SPI clock cycles. The host software may then
verify the expected pattern has been received. The SPI bus speed operates
reliably around 100KHz.

The I/O board uses a 1.8432MHz oscillator. The pulse meter input is physically
wired between header J11-2 and J11-1. The input signal is pulled up to 3.3V
via 10K resistor and shorted to ground when pulsed (The meter implements a
mechanical reed relay switch.) The input signal is fed through a ~139uS
debounce module. After the debounce period has elapsed, the pulse count
accumulator is incremented by one.

The SPI interface is implemented on header J9 and conforms to the 10 pin UEXT
header specification. RPI SPI header pins are shown for reference.

    Signal        |  CPLD Pin  |  Header         RPI - 40 Pin Header
    ------------------------------------         -------------------
    SPI CLK       |  29        |  J9-9           23
    SPI CSn       |  28        |  J9-10          24
    SPI MOSI      |  30        |  J9-8           19
    SPI MISO      |  31        |  J9-7           21       (GND = 25)
    ------------------------------------
    PULSE IN      |  20        |  J11-2
    PULSE GND     |  N/A       |  J11-1
    ------------------------------------
    CLK 1.8432MHz |  44        |  N/A


A serial vector format (SVF) programming file is located at
"ise/pulsemeter.svf" relative to to this readme. Program it using the svfload
tool:

    svfload -p -f pulsemeter.svf


A detailed datasheet for the water meter may be found at the following link:

    https://documents.ekmmetering.com/EKM-SPWM-075-HD-NSF-water-meter-spec-sheet.pdf



The Other Implementation
========================

The Verilog and ISE project for this CPLD board is in the 'verilog-diag'
subdirectory relative to this readme. Load in the svf file called
'pulsemeter-io.svf'

The SPI controller on the AML-S905X-CC board is too fast for the CPLD board.
Instead of using SPI it uses a UART + some GPIO lines. Plus, this is more than
just the pulsemeter. It's also the pump status and inhibit control.


SBC to I/O board:

    Signal      aml-s905x-cc       io/board header   cpld pin  notes
    -----------------------------------------------------------------------
    UART TX     2J1-2               J11-2            20        sbc --> cpld              (pull up)
    UART RX     2J1-3               J13-2            19        sbc <-- cpld              (pull up)

    JTAG TMS    7J1-31 (GPIOX_18)   J10-1            10
    JTAG TDI    7J1-33 (GPIOX_6)    J10-2             9
    JTAG TDO    7J1-35 (GPIOX_7)    J10-3            24
    JTAG TCK    7J1-37 (GPIOX_5)    J10-4            11
    JTAG GND    7J1-39              J10-5

    SIG  0      7J1-22 (GPIOX_0)    J6-1              3        relay coil inhibit status
    SIG  1      7J1-29 (GPIOX_17)   J7-1              2        pressure switch sts
    SIG  2      7J1-32 (GPIOX_16)   J8-9              5        uart loopback test        (pull up)
    SIG  GND    7J1-34
    SIG  3      7J1-36 (GPIOX_2)    J8-7              6        uart enable               (pull up)
    SIG  4      7J1-38 (GPIOX_3)    J8-5              7        relay coil inhibit clear  (pull up)
    SIG  5      7J1-40 (GPIOX_4)    J8-3              8        relay coil inhibit set    (pull up)

                                                               If set & clear asserted at same time then
                                                               force relay coil active. These signals can
                                                               level trigger and still latch the status.

I/O board to relay rack:

    Signal      Relay Board         io/board header  cpld pin  notes
    -----------------------------------------------------------------------
    CH 3        Logic TB-9          J9-10            28        240 VAC pressure switch on (input)
    CH 2        Logic TB-7          J9-8             30        spare
    CH 1        Logic TB-5          J9-6             32        24 VAC power good          (input)
    CH 0        Logic TB-3          J9-4             38        24 VAC relay coil          (output)


I/O board misc:

    Signal                         i/o board header  cpld pin  notes
    -----------------------------------------------------------------------
    Pulsemeter SIG                 J8-1              12                                   (pull up)
    Pulsemeter GND                 J8-2
    LED D2                                           21
    LED D3                                           22


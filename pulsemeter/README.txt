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
header specification.

    Signal        |  CPLD Pin  |  Header
    ------------------------------------
    SPI CLK       |  29        |  J9-9
    SPI CSn       |  28        |  J9-10
    SPI MOSI      |  30        |  J9-8
    SPI MISO      |  31        |  J9-7
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


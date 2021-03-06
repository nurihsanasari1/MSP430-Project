#include <msp430g2553.h>

/*
 * SPI Receiver
 * ---------------
 * Carlton Duffett
 * Neeraj Basu
 *
 * Receives a byte over the SPI interface from a second Transmitter microcontroller.
 * The byte contains a value generated by the ADC on the other device. This value  is based
 * on a variable voltage generated using a potentiometer and voltage divider circuit. This
 * receiver displays the value received from the ADC on a 7-Segment display as a decimal
 * number between 0 and 8.
 */

// SPI interface definitions
#define ACTION_INTERVAL 1
#define BIT_RATE_DIVISOR 32
#define SPI_CLK 0x20
#define SPI_SOMI 0x40
#define SPI_SIMO 0x80

// 7-segment display addresses
#define ADDR_A 0x01
#define ADDR_B 0x02
#define ADDR_C 0x04
#define ADDR_D 0x08

volatile unsigned char data_received = 0;   // most recent byte received
volatile unsigned long rx_count = 0;        // total number received handler calls

// ===== Initialization Functions =====
void init_spi(void);
void init_wdt(void);
void init_7seg(void);

// ===== Main Program =====
void main(){

    WDTCTL = WDTPW + WDTHOLD;       // Stop watchdog timer
    BCSCTL1 = CALBC1_8MHZ;          // 8Mhz calibration for clock
    DCOCTL  = CALDCO_8MHZ;

    init_spi();
    init_wdt();
    init_7seg();

    _bis_SR_register(GIE+LPM0_bits);
}

// ===== Initialize the Watchdog Timer
void init_wdt(){

    // setup the watchdog timer as an interval timer
    WDTCTL =(WDTPW +    // (bits 15-8) password
                        // bit 7=0 => watchdog timer on
                        // bit 6=0 => NMI on rising edge (not used here)
                        // bit 5=0 => RST/NMI pin does a reset (not used here)
          WDTTMSEL +    // (bit 4) select interval timer mode
          WDTCNTCL      // (bit 3) clear watchdog timer counter
                        // bit 2=0 => SMCLK is the source
                        // bits 1-0 = 00 => source/32K
          );
    IE1 |= WDTIE;       // enable the WDT interrupt (in the system interrupt register IE1)
}

// calculate the lo and hi bytes of the bit rate divisor
#define BRLO (BIT_RATE_DIVISOR &  0xFFFF)
#define BRHI (BIT_RATE_DIVISOR / 0x10000)

// ===== Initialize the SPI Interface =====
void init_spi(){

    UCB0CTL1 = UCSSEL_2 + UCSWRST;  // Reset state machine; SMCLK source;
    UCB0CTL0 = UCCKPH               // Data capture on rising edge
                                    // read data while clock high
                                    // lsb first, 8 bit mode,
                                    // slave
             +UCMODE_0              // 3-pin SPI mode
             +UCSYNC;               // sync mode (needed for SPI or I2C)
    UCB0BR0 = BRLO;                 // set divisor for bit rate
    UCB0BR1 = BRHI;
    UCB0CTL1 &= ~UCSWRST;           // enable UCB0 (must do this before setting
                                    //              interrupt enable and flags)
    IFG2 &= ~UCB0RXIFG;             // clear UCB0 RX flag
    IE2 |= UCB0RXIE;                // enable UCB0 RX interrupt

    // Connect I/O pins to UCB0 SPI
    P1SEL  = SPI_CLK + SPI_SOMI + SPI_SIMO;
    P1SEL2 = SPI_CLK + SPI_SOMI + SPI_SIMO;
}

// ===== Initialize the 7-Segment Display ====
void init_7seg() {

    // Setup P1.0 -> P1.3 to drive a BCD to 7-segment decoder
    P1DIR |= ADDR_A + ADDR_B + ADDR_C + ADDR_D;   // Set P1.0 -> P1.3 as outputs
    P1OUT &= ~(ADDR_A + ADDR_B + ADDR_C + ADDR_D);// initally display 0 (0b0000)
}

// ===== Watchdog Timer Interrupt Handler =====
interrupt void WDT_interval_handler(){

    // Translate the ADC conversion to a
    // BCD number between 0 and 8.

    if (data_received < 28) {
        // display 0 (0b0000)
        P1OUT &= ~(ADDR_A + ADDR_B + ADDR_C + ADDR_D);
    }
    else if (data_received < 56){
        // display 1 (0b0001)
        P1OUT |= (ADDR_A);
        P1OUT &= ~(ADDR_B + ADDR_C + ADDR_D);
    }
    else if (data_received < 84){
        // display 2 (0b0010)
        P1OUT |= (ADDR_B);
        P1OUT &= ~(ADDR_A + ADDR_C + ADDR_D);
    }
    else if (data_received < 112){
        // display 3 (0b0011)
        P1OUT |= (ADDR_A + ADDR_B);
        P1OUT &= ~(ADDR_C + ADDR_D);
    }
    else if (data_received < 140){
        // display 4 (0b0100)
        P1OUT |= (ADDR_C);
        P1OUT &= ~(ADDR_A + ADDR_B + ADDR_D);
    }
    else if (data_received < 168){
        // display 5 (0b0101)
        P1OUT |= (ADDR_A + ADDR_C);
        P1OUT &= ~(ADDR_B + ADDR_D);
    }
    else if (data_received < 196){
        // display 6 (0b0110)
        P1OUT |= (ADDR_B + ADDR_C);
        P1OUT &= ~(ADDR_A + ADDR_D);
    }
    else if (data_received < 224){
        // display 7 (0b0111)
        P1OUT |= (ADDR_A + ADDR_B + ADDR_C);
        P1OUT &= ~(ADDR_D);
    }
    else if (data_received < 256){
        // display 8 (0b1000)
        P1OUT |= ADDR_D;
        P1OUT &= ~(ADDR_A + ADDR_B + ADDR_C);
    }
}
ISR_VECTOR(WDT_interval_handler, ".int10")

// ===== SPI Receive Handler =====
void interrupt spi_rx_handler(){

    data_received = UCB0RXBUF; // copy data to global variable
    ++rx_count;                // increment the counter
    IFG2 &= ~UCB0RXIFG;        // clear UCB0 RX flag
}
ISR_VECTOR(spi_rx_handler, ".int07")

#ifndef __LC3_IO_H__
#define __LC3_IO_H__

#define STDIN_S   0xfe00u
#define STDIN_D   0xfe02u
#define STDOUT_S  0xfe04u
#define STDOUT_D  0xfe06u
#define SW_S      0xfe08u
#define SW_D      0xfe0Au
#define BTN_S     0xfe0cu
#define BTN_D     0xfe0eu
#define SSEG_S    0xfe10u
#define SSEG_D    0xfe12u
#define LED_S     0xfe14u
#define LED_D     0xfe16u
#define PS2KBD_S  0xfe18u
#define PS2KBD_D  0xfe1au

/* Interrupt control registers */
#define INT0_CR   0xfff0u
#define INT1_CR   0xfff1u
#define INT2_CR   0xfff2u
#define INT3_CR   0xfff3u
#define INT4_CR   0xfff4u
#define INT5_CR   0xfff5u
#define INT6_CR   0xfff6u
#define INT7_CR   0xfff7u
/* Machine control register */
#define LC3_MCR   0xfffeu


#define IO_READY  -0x8000

/* Primitive I/O routines */
short io_read(unsigned short io_addr);
void io_write(unsigned short io_addr, short val);

/* Optimized blocking routines */
short stdin_wait_read();
void stdout_wait_write(short val);
short ps2kbd_wait_read();

#endif

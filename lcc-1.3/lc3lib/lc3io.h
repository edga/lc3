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

#define IO_READY  0x8000u

/* Primitive I/O routines */
short io_read(unsigned short io_addr);
void io_write(unsigned short io_addr, short val);

/* Optimized blocking routines */
short stdin_wait_read();
void stdout_wait_write(short val);
short ps2kbd_wait_read();

#endif

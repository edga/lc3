/*\
 *  LC-3 Simulator
 *  Copyright (C) 2004  Anthony Liguori <aliguori@cs.utexas.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\*/

#include <stdio.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "cpu.hpp"
#include "memory.hpp"
#include "hardware.hpp"


static int data_available (int fd)
{
  struct timeval tv;
  fd_set rdfs;

  tv.tv_sec = 0;
  tv.tv_usec = 0;

  FD_ZERO(&rdfs);
  FD_SET (fd, &rdfs);

  int ret = select(fd+1, &rdfs, NULL, NULL, &tv);
//  printf("sel(%d): %d\n", fd, ret);
  return FD_ISSET(fd, &rdfs);

}

struct KBSR : public MappedWord
{
  enum { ADDRESS = 0xFE00 };
  KBSR() {
    fd = fileno(stdin);
  }
  operator int16_t() const {
    return data_available(fd) ? 0x8000 : 0;
  }

  void set_tty(int _fd) {
    fd = _fd;
  }

private:
  int fd;
};

struct KBDR : public MappedWord
{
  enum { ADDRESS = 0xFE02 };
  KBDR() {
    fd = fileno(stdin);
  }

  operator int16_t() const {
    static unsigned char prev_char;
    // Note: terminal is setup in Hardware class
    if (data_available(fd)) {
      int ret = read(fd,&prev_char, 1);
 //     printf("read(%d): %d = %d\n", fd, ret, prev_char);
    }
    return prev_char;
  }

  void set_tty(int _fd) {
    fd = _fd;
  }

private:
  int fd;
};

struct DSR : public MappedWord
{
  enum { ADDRESS = 0xFE04 };
  operator int16_t() const { return 0x8000; }
};

struct DDR : public MappedWord
{
  enum { ADDRESS = 0xFE06 };
  DDR() : fd(-1) { }
  DDR &operator=(int16_t data) {
    if (fd == -1) {
      putchar(data & 0xFF);
      fflush(stdout);
    } else {
      int8_t byte = data & 0xFF;
      write(fd, &byte, 1);
    }
    return *this;
  }

  void set_tty(int _fd) {
    fd = _fd;
  }

private:
  int fd;
};

struct CCR : public MappedWord
{
  enum { ADDRESS = 0xFFFF };
  CCR() : ccr(0) { }
  operator int16_t() const { return (int16_t)ccr; }
  operator uint16_t() const { return ccr; }
  CCR &operator=(int16_t value) {
    ccr = (uint16_t)value;
    return *this;
  }
  void mycycle() {
    ccr++;
  }
private:
  uint16_t ccr;
};

struct MCR : public MappedWord
{
  MCR(CCR &ccr, LC3::CPU &cpu) : mcr(0x0030), ccr(ccr), cpu(cpu) { }

  enum { ADDRESS = 0xFFFE };
  operator int16_t() const { return mcr; }
  MCR &operator=(int16_t value) {
    mcr = value;
    return *this;
  }

  void cycle() {
    ccr.mycycle();
    if ((uint16_t)ccr >= (mcr & 0x3FFF) && 
	mcr & 0x4000) {
      cpu.interrupt(0x02, 1);
    }
  }
private:
  int16_t mcr;
  CCR &ccr;
  LC3::CPU &cpu;
};

#ifdef USE_LC3_IO_EMU    

extern "C" {
#include <lc3io.h>
}

struct GenericIO : public MappedWord
{
  GenericIO(uint16_t _addr) {
    address = _addr;
  }

  operator int16_t() const { return (int16_t) io_read(address); }
  GenericIO &operator=(int16_t value) {
    io_write(address, value);
    return *this;
  }
private:
  uint16_t address;
};
#endif

class Hardware::Implementation
{
public:
  Implementation(Memory &mem, LC3::CPU &cpu) : 
    mem(mem), mcr(ccr, cpu), ifd(-1), ofd(-1)
  {
    mem.register_dma(KBSR::ADDRESS, &kbsr);
    mem.register_dma(KBDR::ADDRESS, &kbdr);
    mem.register_dma(DSR::ADDRESS, &dsr);
    mem.register_dma(DDR::ADDRESS, &ddr);
    mem.register_dma(MCR::ADDRESS, &mcr);
    mem.register_dma(CCR::ADDRESS, &ccr);

#ifdef USE_LC3_IO_EMU    
	uint16_t addr;
    addr = SW_S; mem.register_dma(addr, new GenericIO(addr));
    addr = SW_D; mem.register_dma(addr, new GenericIO(addr));
    addr = BTN_S; mem.register_dma(addr, new GenericIO(addr));
    addr = BTN_D; mem.register_dma(addr, new GenericIO(addr));
    addr = SSEG_S; mem.register_dma(addr, new GenericIO(addr));
    addr = SSEG_D; mem.register_dma(addr, new GenericIO(addr));
    addr = LED_S; mem.register_dma(addr, new GenericIO(addr));
    addr = LED_D; mem.register_dma(addr, new GenericIO(addr));
    addr = PS2KBD_S; mem.register_dma(addr, new GenericIO(addr));
    addr = PS2KBD_D; mem.register_dma(addr, new GenericIO(addr));
#endif

    // Set the terminal to non-echo mode
    set_tty(fileno(stdin), fileno(stdout));

  }
  ~Implementation() {
    if (ifd != -1) {
      // restore previous settings
      tcsetattr(ifd, TCSANOW, &termios_original);
    }
  }

  void set_tty(int _ifd, int _ofd) {
    setup_input_tty(_ifd);
    kbsr.set_tty(_ifd);
    kbdr.set_tty(_ifd);

    ddr.set_tty(_ofd);
    ofd = _ofd;
  }

  void set_tty(int fd) {
    set_tty(fd, fd);
  }
  
  void setup_input_tty(int fd) {
    struct termios new_termios = {0,};
    if (ifd != -1) {
      // restore previous settings
      tcsetattr(ifd, TCSANOW, &termios_original);
    }
    tcgetattr(fd, &new_termios);
    termios_original = new_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    // FixMe: this is work around for using same tty for both program and commands
    if (fd != fileno(stdin)) {
      tcsetattr(fd, TCSAFLUSH, &new_termios);
    }
    ifd = fd;
  }


private:
  int ifd;
  int ofd;
  struct termios termios_original;
  Memory &mem;
  KBSR kbsr;
  KBDR kbdr;
  DSR dsr;
  DDR ddr;
  CCR ccr;
  MCR mcr;
};

Hardware::Hardware(Memory &mem, LC3::CPU &cpu) :
  impl(new Implementation(mem, cpu))
{
}

Hardware::~Hardware()
{
  delete impl;
}

void Hardware::set_tty(int fd)
{
  impl->set_tty(fd);
}

/**
 * @file arch/x86/pit.c
 *
 * @author Hiroyuki Chishiro
 */
/*
 * i8253/i8254-compatible PIT
 *
 * Copyright (C) 2009-2010 Ahmed S. Darwish <darwish.07@gmail.com>
 *
 * Check Intel's "82C54 CHMOS Programmable Interval Timer" datasheet
 * for more details.
 *
 * The PIT contains three timers (counters). Each timer is attached to a
 * GATE and an OUT pin, leading to the pins GATE-0, OUT-0, GATE-1, OUT-1,
 * and GATE-2, OUT-2.
 *
 * If GATE-x is positive, it enables the timer x to count, otherwise the
 * timer's count value stand still.
 *
 * In a PC compatible machine, GATE-0 and GATE-1 are always positive.
 * GATE-2 is controlled by Port 0x61 bit 0.
 *
 * OUT-x is the output pin of timer x. OUT-0 is traditionally mapped to
 * IRQ 0. OUT-1 was typically connected to DRAM refresh logic and is now
 * obsolete. OUT-2 is connected to port 0x61 bit 5.
 *
 * NOTE! Port 0x61 bit 1 is an input pin to a glue AND logic with the
 * OUT-2 pin, which leads to outputting counter 2 ticks to the system
 * speaker if set.
 *
 * Delays were tested against wall clock time.
 *
 * Finally, remember that for legacy hardware, it typically takes about
 * 1-us to access an I/O port.
 */

#include <mcube/mcube.h>

#define PIT_CLOCK_RATE  1193182ul  /* Hz (ticks per second) */

/*
 * Extract a PIT-related bit from port 0x61
 */
enum {
  PIT_GATE2   = 0x1,    /* bit 0 - PIT's GATE-2 input */
  PIT_SPEAKER = 0x2,    /* bit 1 - enable/disable speaker */
  PIT_OUT2    = 0x20,    /* bit 5 - PIT's OUT-2 */
};

/*
 * The PIT's system interface: an array of peripheral I/O ports.
 * The peripherals chip translates accessing below ports to the
 * right PIT's A0 and A1 address pins, and RD/WR combinations.
 */
enum {
  PIT_COUNTER0 =  0x40,    /* read/write Counter 0 latch */
  PIT_COUNTER1 =  0x41,    /* read/write Counter 1 latch */
  PIT_COUNTER2 =  0x42,    /* read/write Counter 2 latch */
  PIT_CONTROL  =  0x43,    /* read/write Chip's Control Word */
};

/*
 * Control Word format
 */
union pit_cmd {
  uint8_t raw;
  struct {
    uint8_t bcd: 1,   /* BCD format for counter divisor? */
            mode: 3,   /* Counter modes 0 to 5 */
            rw: 2,   /* Read/Write latch, LSB, MSB, or 16-bit */
            timer: 2; /* Which timer of the three (0-2) */
  } __packed;
};

/*
 * Read/Write control bits
 */
enum {
  RW_LATCH =  0x0,    /* Counter latch command */
  RW_LSB   =  0x1,    /* Read/write least sig. byte only */
  RW_MSB   =  0x2,    /* Read/write most sig. byte only */
  RW_16bit =  0x3,    /* Read/write least then most sig byte */
};

/*
 * Counters modes
 */
enum {
  MODE_0 =  0x0,    /* Single interrupt on timeout */
  MODE_1 =  0x1,    /* Hardware retriggerable one shot */
  MODE_2 =  0x2,    /* Rate generator */
  MODE_3 =  0x3,    /* Square-wave mode */
};


/*
 * Test PIT's monotonic mode code
 */

/*
 * Increase the counter for each periodic PIT tick.
 */
volatile int pit_ticks_count = 0;

void __pit_periodic_handler(void)
{
  pit_ticks_count++;
}

/*
 * Start counter 2: raise the GATE-2 pin.
 * Disable glue between OUT-2 and the speaker in the process
 */
static inline void timer2_start(void)
{
  uint8_t val;

  val = (inb(0x61) | PIT_GATE2) & ~PIT_SPEAKER;
  outb(val, 0x61);
}

/*
 * Freeze counter 2: clear the GATE-2 pin.
 */
static inline void timer2_stop(void)
{
  uint8_t val;

  val = inb(0x61) & ~PIT_GATE2;
  outb(val, 0x61);
}

/*
 * Set the given PIT counter with a count representing given
 * milliseconds value relative to the PIT clock rate.
 *
 * @counter_reg: PIT_COUNTER{0, 1, 2}
 *
 * Due to default oscillation frequency and the max counter
 * size of 16 bits, maximum delay is around 53 milliseconds.
 *
 * Countdown begins once counter is set and GATE-x is up.
 */
static void pit_set_counter(uint32_t us, int counter_reg)
{
  uint32_t counter;
  uint8_t counter_low, counter_high;

  /* counter = ticks per second * seconds to delay
   *         = PIT_CLOCK_RATE * (us / (1000 * 1000))
   *         = PIT_CLOCK_RATE / ((1000 * 1000) / us)
   * We use last form to avoid float arithmetic */
  assert(us > 0);
  assert(((1000 * 1000) / us) > 0);
  //  printk("us = %d\n", us);
  counter = PIT_CLOCK_RATE / ((1000 * 1000) / us);
  //  printk("counter = %u\n", counter);
  
  assert(counter <= UINT16_MAX);
  counter_low = counter & 0xff;
  counter_high = counter >> 8;

  outb(counter_low, counter_reg);
  outb(counter_high, counter_reg);
}

/*
 * Did we program PIT's counter0 to monotonic mode?
 */
static bool timer0_monotonic;

/*
 * Delay/busy-loop for @us milliseconds.
 */
void pit_mdelay(uint32_t us)
{
  union pit_cmd cmd = { .raw = 0 };

  /* GATE-2 down */
  timer2_stop();

  cmd.bcd = 0;
  cmd.mode = MODE_0;
  cmd.rw = RW_16bit;
  cmd.timer = 2;
  outb(cmd.raw, PIT_CONTROL);

  pit_set_counter(us, PIT_COUNTER2);

  /* GATE-2 up */
  timer2_start();

  while ((inb(0x61) & PIT_OUT2) == 0) {
    cpu_pause();
  }
}

/*
 * Trigger PIT IRQ pin (OUT-0) after given timeout
 */
void pit_oneshot(uint32_t us)
{
  union pit_cmd cmd = { .raw = 0 };

  /* No control over GATE-0: it's always positive */

  if (timer0_monotonic == true) {
    panic("PIT: Programming timer0 as one-shot will "
          "stop currently setup monotonic mode");
  }

  cmd.bcd = 0;
  cmd.mode = MODE_0;
  cmd.rw = RW_16bit;
  cmd.timer = 0;
  outb(cmd.raw, PIT_CONTROL);

  pit_set_counter(us, PIT_COUNTER0);
}

void init_timer(unsigned long tick_us)
{
  pit_set_counter(tick_us, PIT_COUNTER0);
}

void start_timer(__unused unsigned int ch)
{
  union pit_cmd cmd = { .raw = 0 };
  /* No control over GATE-0: it's always positive */

  timer0_monotonic = true;

  cmd.bcd = 0;
  cmd.mode = MODE_2;
  cmd.rw = RW_16bit;
  cmd.timer = 0;
  outb(cmd.raw, PIT_CONTROL);
  //  outb(inb(PIC0_IMR) & unmask_irq(PIT_IRQ), PIC0_IMR);
}

void stop_timer(__unused unsigned int ch)
{
  union pit_cmd cmd = { .raw = 0 };

  cmd.bcd = 0;
  cmd.mode = MODE_0;
  cmd.rw = RW_16bit;
  cmd.timer = 0;
  outb(cmd.raw, PIT_CONTROL);
}

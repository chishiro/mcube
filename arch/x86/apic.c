/**
 * @file arch/x86/apic.c
 *
 * @author Hiroyuki Chishiro
 */
/*
 * Local APIC configuration
 *
 * Copyright (C) 2009-2010 Ahmed S. Darwish <darwish.07@gmail.com>
 */

#include <mcube/mcube.h>

static union apic_id bootstrap_apic_id;
static bool bootstrap_apic_id_saved;

/*
 * Internal and external clock frequencies
 * @cpu_clock: CPU internal clock (speed)
 * @apic_clock: CPU external bus clock (APIC timer, ..)
 */
static uint64_t cpu_clock;
static uint64_t apic_clock;

/*
 * Where the APIC registers are virtually mapped
 */
static void *apic_virt_base;


/*
 * Test APIC timer periodic ticks against PIT delays
 */

/*
 * Increase the counter for each periodic timer tick.
 */
volatile int apic_ticks_count;

void __apic_timer_handler(void)
{
  apic_ticks_count++;
}

/*
 * Clock calibrations
 */

/*
 * Calculate the processor clock using the PIT and the time-
 * stamp counter: it's increased by one for each clock cycle.
 *
 * There's a possibility of being hit by a massive SMI, we
 * repeat the calibration to hope this wasn't the case. Some
 * SMI handlers can take many milliseconds to complete!
 *
 * FIXME: For Intel core2duo cpus and up, ask the architect-
 * ural event monitoring interface to calculate cpu clock.
 *
 * Return cpu clock ticks per second.
 */
static uint64_t pit_calibrate_cpu(int repeat)
{
  int us_delay;
  uint64_t tsc1, tsc2, diff, diff_min, cpu_clock;

  us_delay = 5 * 1000;
  diff_min = UINT64_MAX;

  for (int i = 0; i < repeat; i ++) {
    tsc1 = rdtsc();
    pit_udelay(us_delay);
    tsc2 = rdtsc();

    diff = tsc2 - tsc1;

    if (diff < diff_min) {
      diff_min = diff;
    }
  }

  /* ticks per second = ticks / delay-in-seconds
   *                  = ticks / (delay / 1000)
   *                  = ticks * (1000 / delay)
   * We use last form to avoid float arithmetic */
  cpu_clock = diff_min * ((1000 * 1000) / us_delay);

  return cpu_clock;
}

/*
 * Calibrate CPU external bus clock, which is the time
 * base of the APIC timer.
 */
static uint64_t pit_calibrate_apic_timer(void)
{
  int us_delay;
  uint32_t counter1, counter2, ticks, ticks_min;
  uint64_t apic_clock;
  union apic_lvt_timer lvt_timer;

  /* Before setting up the counter */
  lvt_timer.value = apic_read(APIC_LVTT);
  lvt_timer.timer_mode = APIC_TIMER_ONESHOT;
  lvt_timer.mask = APIC_MASK;
  apic_write(APIC_LVTT, lvt_timer.value);
  apic_write(APIC_DCR, APIC_DCR_1);

  /* Guarantee timer won't overflow */
  counter1 = ticks_min = UINT32_MAX;
  us_delay = 5 * 1000;

  for (int i = 0; i < 5; i++) {
    apic_write(APIC_TIMER_INIT_CNT, counter1);
    pit_udelay(us_delay);
    counter2 = apic_read(APIC_TIMER_CUR_CNT);

    assert(counter1 > counter2);
    ticks = counter1 - counter2;

    if (ticks < ticks_min) {
      ticks_min = ticks;
    }
  }

  /* ticks per second = ticks / delay-in-seconds
   *                  = ticks / (delay / 1000)
   *                  = ticks * (1000 / delay)
   * We use last form to avoid float arithmetic */
  apic_clock = ticks_min * ((1000 * 1000) / us_delay);

  return apic_clock;
}

/*
 * Local APIC
 */

/*
 * Intialize (the now VM-mapped) APIC registers of the calling CPU
 * to a well-known state, enabling the local APIC in the process.
 */
void apic_local_regs_init(void)
{
  union apic_tpr tpr = { .value = APIC_TPR_RESET };
  union apic_ldr ldr = { .value = APIC_LDR_RESET };
  union apic_dfr dfr = { .value = APIC_DFR_RESET };
  union apic_spiv spiv = { .value = APIC_SPIV_RESET };

  union apic_lvt_timer timer = { .value = APIC_LVT_RESET };
  union apic_lvt_thermal thermal = { .value = APIC_LVT_RESET };
  union apic_lvt_perfc perfc = { .value = APIC_LVT_RESET };
  union apic_lvt_lint lint0 = { .value = APIC_LVT_RESET };
  union apic_lvt_lint lint1 = { .value = APIC_LVT_RESET };

  msr_apicbase_setaddr(APIC_PHBASE);

  tpr.subclass = APIC_TPR_DISABLE_IRQ_BALANCE;
  tpr.priority = APIC_TPR_DISABLE_IRQ_BALANCE;
  apic_write(APIC_TPR, tpr.value);

  /* In the logical apic destination flat model, a _unique_
   * logical ID can be established for up to 8 cores by setting
   * a unique bit for each core's LDR logical id.
   *
   * Each local APIC performs a logical AND of the Message
   * Destination Address (set in the IO-APIC irq entry) and its
   * logical APIC ID. If a true (non-zero) result is detected,
   * the core accepts the message.
   *
   * To support broadcasting interrupts even in the case of
   * more than 8 cores, we set all cores logical IDs to all 1s
   * (0xff). By this, we lose the unique identification feature
   * while gaining the ability to broadcast messages to all. */
  ldr.logical_id = 0xff;
  apic_write(APIC_LDR, ldr.value);

  /* "All processors that have their APIC software enabled
   * must have their DFRs programmed identically." --Intel */
  dfr.apic_model = APIC_MODEL_FLAT;
  apic_write(APIC_DFR, dfr.value);

  timer.vector = APIC_TIMER_VECTOR;
  timer.mask = APIC_MASK;
  apic_write(APIC_LVTT, timer.value);

  thermal.vector = APIC_THERMAL_VECTOR;
  thermal.mask = APIC_MASK;
  apic_write(APIC_LVTTHER, thermal.value);

  perfc.vector = APIC_PERFC_VECTOR;
  perfc.mask = APIC_MASK;
  apic_write(APIC_LVTPC, perfc.value);

  lint0.vector = APIC_LINT0_VECTOR;
  lint0.mask = APIC_MASK;
  apic_write(APIC_LVT0, lint0.value);

  lint1.vector = APIC_LINT1_VECTOR;
  lint1.mask = APIC_MASK;
  apic_write(APIC_LVT1, lint1.value);

  /* A spurious APIC interrupt occurs when a CPU core raises
   * its task priority (TPR) >= the level of interrupt being
   * _currently_ asserted @ the core's INTR wire.
   *
   * Since we don't use hardware IRQ balancing at all (TPR
   * fields always = 0), this shouldn't bother us now; APIC
   * spurious IRQs don't need to get acked with an APIC EOI. */
  spiv.vector = APIC_SPURIOUS_VECTOR;

  /* Finally, enable our local APIC */
  spiv.apic_enable = 1;
  apic_write(APIC_SPIV, spiv.value);
  msr_apicbase_enable();
}

void set_cpu_clock(void)
{
  cpu_clock = pit_calibrate_cpu(10);
  printk("APIC: Detected %d.%d MHz processor\n",
         cpu_clock / 1000000, (uint8_t)(cpu_clock % 1000000));
  CPU_CLOCK = cpu_clock;
  CPU_CLOCK_MHZ_PER_USEC = CPU_CLOCK / 1000000;
  //  printk("CPU_CLOCK = %lu CPU_CLOCK_MHZ_PER_USEC = %lu\n", CPU_CLOCK, CPU_CLOCK_MHZ_PER_USEC);
}

void apic_init(void)
{
  /*
   * Basic APIC initialization:
   * - Before doing any apic operation, assure the APIC
   *   registers base address is set where we expect it
   * - Map the MMIO registers region before accessing it
   * - Find CPU's internal clock frequency: calibrate TSC
   * - Find APIC timer frequency: calibrate CPU's bus clock
   * - Initialize the APIC's full set of registers
   */

  msr_apicbase_setaddr(APIC_PHBASE);

  apic_virt_base = vm_kmap(APIC_PHBASE, APIC_MMIO_SPACE);

  set_cpu_clock();


  apic_clock = pit_calibrate_apic_timer();
  printk("APIC: Detected %d.%d MHz bus clock\n",
         apic_clock / 1000000, (uint8_t)(apic_clock % 1000000));

  apic_local_regs_init();

  bootstrap_apic_id.raw = apic_read(APIC_ID);
  bootstrap_apic_id_saved = true;

  printk("APIC: bootstrap core lapic enabled, apic_id=0x%x\n",
         bootstrap_apic_id.id);

}

/*
 * APIC Timer
 */

/*
 * Set the APIC timer counter with a count representing
 * given u-seconds value relative to the bus clock.
 */
static void apic_set_counter_us(uint64_t us)
{
  union apic_dcr dcr;
  uint64_t counter;

  dcr.value = 0;
  dcr.divisor = APIC_DCR_1;
  apic_write(APIC_DCR, dcr.value);

  /* counter = ticks per second * seconds to delay
   *         = apic_clock * (us / 1000000)
   *         = apic_clock / (1000000 / us)
   * We use division to avoid float arithmetic */
  assert(us > 0);
  assert((1000000 / us) > 0);
  counter = apic_clock / (1000000 / us);

  assert(counter <= UINT32_MAX);
  apic_write(APIC_TIMER_INIT_CNT, counter);
}

/*
 * micro-second delay
 */
void apic_udelay(uint64_t us)
{
  union apic_lvt_timer lvt_timer;

  /* Before setting up the counter */
  lvt_timer.value = 0;
  lvt_timer.timer_mode = APIC_TIMER_ONESHOT;
  lvt_timer.mask = APIC_MASK;
  apic_write(APIC_LVTT, lvt_timer.value);

  apic_set_counter_us(us);

  while (apic_read(APIC_TIMER_CUR_CNT) != 0) {
    cpu_pause();
  }
}


/*
 * Trigger local APIC timer IRQs at periodic rate
 * @ms: milli-second delay between each IRQ
 * @vector: IRQ vector where ticks handler is setup
 */
void apic_monotonic(uint64_t us, uint8_t vector)
{
  union apic_lvt_timer lvt_timer;

  /* Before setting up the counter */
  lvt_timer.value = 0;
  lvt_timer.vector = vector;
  lvt_timer.mask = APIC_UNMASK;
  lvt_timer.timer_mode = APIC_TIMER_PERIODIC;
  apic_write(APIC_LVTT, lvt_timer.value);

  apic_set_counter_us(us);
}

/*
 * Inter-Processor Interrupts
 */

/* NOTE! This function is implicitly called by panic
 * code: it should not include any asserts or panics. */
static void __apic_send_ipi(int dst_apic_id, int delivery_mode,
                            int vector, enum irq_dest dest)
{
  union apic_icr icr = { .value = 0 };

  icr.vector = vector;
  icr.delivery_mode = delivery_mode;

  switch (dest) {
  case IRQ_BROADCAST:
    icr.dest_shorthand = APIC_DEST_SHORTHAND_ALL_BUT_SELF;
    break;

  case IRQ_SINGLE:
    icr.dest_mode = APIC_DESTMOD_PHYSICAL;
    icr.dest = dst_apic_id;
    break;

  default:
    compiler_assert(false);
  }

  /* "Level" and "deassert" are for 82489DX */
  icr.level = APIC_LEVEL_ASSERT;
  icr.trigger = APIC_TRIGGER_EDGE;

  /* Writing the low doubleword of the ICR causes
   * the IPI to be sent: prepare high-word first. */
  apic_write(APIC_ICRH, icr.value_high);
  apic_write(APIC_ICRL, icr.value_low);
}

void apic_send_ipi(int dst_apic_id, int delivery_mode, int vector)
{
  __apic_send_ipi(dst_apic_id, delivery_mode, vector, IRQ_SINGLE);
}

void apic_broadcast_ipi(int delivery_mode, int vector)
{
  __apic_send_ipi(0, delivery_mode, vector, IRQ_BROADCAST);
}

/*
 * Poll the delivery status bit till the latest IPI is acked
 * by the destination core, or timeout. As advised by Intel,
 * this should be checked after sending each IPI.
 *
 * Return 'true' in case of delivery success.
 * FIXME: fine-grained timeouts using micro-seconds.
 */
bool apic_ipi_acked(void)
{
  union apic_icr icr = { .value = 0 };
  int timeout = 100;

  while (timeout--) {
    icr.value_low = apic_read(APIC_ICRL);

    if (icr.delivery_status == APIC_DELSTATE_IDLE) {
      return true;
    }

    pit_udelay(1 * 1000);
  }

  return false;
}

/*
 * Basic state accessors
 */

uint8_t apic_bootstrap_id(void)
{
  assert(bootstrap_apic_id_saved == true);

  return bootstrap_apic_id.id;
}

void *apic_vrbase(void)
{
  assert(apic_virt_base != NULL);

  return apic_virt_base;
}


void init_apic_timer(unsigned long tick_us, uint8_t vector)
{
  union apic_lvt_timer lvt_timer;

  /* Before setting up the counter */
  lvt_timer.value = 0;
  lvt_timer.vector = vector;
  lvt_timer.mask = APIC_MASK;
  lvt_timer.timer_mode = APIC_TIMER_PERIODIC;
  apic_write(APIC_LVTT, lvt_timer.value);

  apic_set_counter_us(tick_us);
}

void start_apic_timer(void)
{
  union apic_lvt_timer lvt_timer = { .value = apic_read(APIC_LVTT)};
  lvt_timer.mask = APIC_UNMASK;
  apic_write(APIC_LVTT, lvt_timer.value);
}

void stop_apic_timer(void)
{
  union apic_lvt_timer lvt_timer = { .value = apic_read(APIC_LVTT)};
  lvt_timer.mask = APIC_MASK;
  apic_write(APIC_LVTT, lvt_timer.value);
}

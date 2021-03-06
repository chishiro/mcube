/**
 * @file drivers/misc/keyboard.c
 *
 * @author Hiroyuki Chishiro
 */
/*
 * Barebones PS/2 keyboard
 * Motherboard and on-keyboard i8042-compatible controllers
 *
 * Copyright (C) 2009-2011 Ahmed S. Darwish <darwish.07@gmail.com>
 */

#include <mcube/mcube.h>

enum {
  KBD_STATUS_REG  = 0x64,    /* Status register (R) */
  KBD_COMMAND_REG = 0x64,    /* Command register (W) */
  KBD_DATA_REG  = 0x60,    /* Data register (R/W) */
};

/**
 * @union i8042_status
 * @brief Motherboard controller's status; (R) from port 0x64.
 */
union i8042_status {
  /**
   * Raw.
   */
  uint8_t raw;
  struct {
    uint8_t
    /**
     * 1: a byte's waiting to be read.
     */
    output_ready: 1,
                  /**
                   * 1: full input buffer (0x60|0x64).
                   */
                  input_busy: 1,
                  /**
                   * 1: self-test successful.
                   */
                  reset: 1,
                  /**
                   * 0: data sent last. 1: command.
                   */
                  last: 1,
                  /**
                   * Unused.
                   */
                  __unused0: 1,
                  /**
                   * 1: transmit to keyboard timeout.
                   */
                  tx_timeout: 1,
                  /**
                   * 1: receive from keyboard timeout.
                   */
                  rx_timeout: 1,
                  /**
                   * 1: parity error on serial link.
                   */
                  parity_error: 1;
  } __packed /** packed. */;
};

/**
 * @enum i8042_cmd
 * @brief Motherboard controller's commands; (W) to port 0x64.
 */
enum i8042_cmd {
  READ_CMD  = 0x20,    /* Read current command byte */
  WRITE_CMD  = 0x60,    /* Write command byte */
  SELF_TEST  = 0xaa,    /* Internal diagnostic test */
  INT_TEST  = 0xab,    /* Test keyboard clock and data lines */
  READ_P1    = 0xc0,    /* Unused - Read input port (P1) */
  READ_OUTPUT  = 0xd0,    /* Read controller's P2 output port */
  WRITE_OUTPUT  = 0xd1,    /* Write controller's P2 output port */
};

/**
 * @union i8042_p2
 * @brief Motherboard controller's output port (P2) pins description.
 * Such pins are connected to system lines like the on-keyboard
 * controller, IRQ1, system reboot, and the A20 multiplexer.
 */
union i8042_p2 {
  /**
   * Raw.
   */
  uint8_t raw;
  struct {
    uint8_t
    /**
     * Write 0 to system reset.
     */
    reset: 1,
           /**
            * If 0, A20 line is zeroed.
            */
           a20: 1,
           /**
            * Unused.
            */
           __unused0: 1,
           /**
            * Unused.
            */
           __unused1: 1,
           /**
            * Output Buffer Full (IRQ1 high).
            */
           irq1: 1,
           /**
            * Input buffer empty.
            */
           input: 1,
           /**
            * PS/2 keyboard clock line.
            */
           clock: 1,
           /**
            * PS/2 keyboard data line.
            */
           data: 1;
  } __packed /** packed. */;
};

/**
 * @enum keyboard_cmd
 * @brief On-keyboard controller commands; written to port 0x60.
 *
 * If the i8042 is expecting data from a previous command, we
 * write it to 0x60. Otherwise, bytes written to 0x60 are sent
 * directly to the keyboard as _commands_.
 */
enum keyboard_cmd {
  LED_WRITE  = 0xed,    /* Set keyboard LEDs states */
  ECHO    = 0xee,    /* Diagnostics: echo back byte 0xee */
  SET_TYPEMATIC  = 0xf3,    /* Set typematic info */
  KB_ENABLE  = 0xf4,    /* If in tx error, Re-enable keyboard */
  RESET    = 0xf5,    /* Reset keyboard to default state */
  FULL_RESET  = 0xff,    /* Full reset + self test */
};

/**
 * Special-keys scan codes
 */
enum {
  KEY_RSHIFT  = 0x36,    /* Right Shift */
  KEY_LSHIFT  = 0x2a,    /* Left Shift */
  KEY_NONE  = 0xff,    /* No key was pressed - NULL mark */
};

/**
 * @def RELEASE(code)
 * Release code equals system scan code with bit 7 set.
 *
 * @brief code Release code.
 */
#define RELEASE(code) (code | 0x80)

/**
 * @var scancodes[][2]
 * @brief AT+ (set 2) keyboard scan codes table.
 */
static uint8_t scancodes[][2] = {
  {0xff, 0xff},
  {0, 0}, /* 0x01: escape (ESC) */
  {'1', '!'}, /* 0x02: ! */
  {'2', '@'}, /* 0x03 */
  {'3', '#'}, /* 0x04 */
  {'4', '$'}, /* 0x05 */
  {'5', '%'}, /* 0x06 */
  {'6', '^'}, /* 0x07 */
  {'7', '&'}, /* 0x08 */
  {'8', '*'}, /* 0x09 */
  {'9', '('}, /* 0x0a */
  {'0', ')'}, /* 0x0b */
  {'-', '_'}, /* 0x0c */
  {'=', '+'}, /* 0x0d */
  {'\b', 0}, /* 0x0e: FIXME: VGA backspace support */
  {'\t', 0}, /* 0x0f: FIXME: VGA tab support */
  {'q', 'Q'}, /* 0x10 */
  {'w', 'W'}, /* 0x11 */
  {'e', 'E'}, /* 0x12 */
  {'r', 'R'}, /* 0x13 */
  {'t', 'T'}, /* 0x14 */
  {'y', 'Y'}, /* 0x15 */
  {'u', 'U'}, /* 0x16 */
  {'i', 'I'}, /* 0x17 */
  {'o', 'O'}, /* 0x18 */
  {'p', 'P'}, /* 0x19 */
  {'[', '{'}, /* 0x1a */
  {']', '}'}, /* 0x1b */
  {'\n', 0}, /* 0x1c: Enter */
  {0, 0}, /* 0x1d: Ctrl; good old days position */
  {'a', 'A'}, /* 0x1e */
  {'s', 'S'}, /* 0x1f */
  {'d', 'D'}, /* 0x20 */
  {'f', 'F'}, /* 0x21 */
  {'g', 'G'}, /* 0x22 */
  {'h', 'H'}, /* 0x23 */
  {'j', 'J'}, /* 0x24 */
  {'k', 'K'}, /* 0x25 */
  {'l', 'L'}, /* 0x26 */
  {';', ':'}, /* 0x27: Semicolon; colon */
  {'\'', '"'}, /* 0x28: Quote; doubelquote */
  {'`', '~'}, /* 0x29: Backquote; tilde */
  {0, 0}, /* 0x2a: Left shift */
  {'\\', '|'}, /* 0x2b: Backslash; pipe */
  {'z', 'Z'}, /* 0x2c */
  {'x', 'X'}, /* 0x2d */
  {'c', 'C'}, /* 0x2e */
  {'v', 'V'}, /* 0x2f */
  {'b', 'B'}, /* 0x30 */
  {'n', 'N'}, /* 0x31 */
  {'m', 'M'}, /* 0x32 */
  {',', '<'}, /* 0x33 */
  {'.', '>'}, /* 0x34 */
  {'/', '?'}, /* 0x35 */
  {0, 0}, /* 0x36: Right shift */
  {0xff, 0xff}, /* 0x37 */
  {0xff, 0xff}, /* 0x38 */
  {' ', ' '},  /* Space */
};

/**
 * @fn static uint8_t kbd_read_input(void)
 * @brief get a pressed key from the keyboard buffer, if any.
 */
static uint8_t kbd_read_input(void)
{
  union i8042_status status;

  status.raw = inb(KBD_STATUS_REG);

  if (status.output_ready) {
    return inb(KBD_DATA_REG);
  }

  return KEY_NONE;
}

/**
 * @fn static void kbd_flush_buffer(void)
 * @brief Hardware initialization: flush the keyboard buffer.
 * Standard buffer size is 16-bytes; bigger sizes are handled for safety.
 */
static void kbd_flush_buffer(void)
{
  int trials = 128;

  while (trials--) {
    if (kbd_read_input() == KEY_NONE) {
      break;
    }

    apic_udelay(50);
  }
}

/**
 * @var shifted
 * @brief Shift keys pressed?
 */
static int shifted;

void __kb_handler(void)
{
  uint8_t code, ascii;

  /* Implicit ACK: reading the scan code empties the
   * controller's output buffer, making it clear its
   * P2 'output buffer full' pin  (which is actually
   * the  IRQ1 pin) to low -- deasserting the IRQ. */
  code = kbd_read_input();

  switch (code) {
  case KEY_LSHIFT:
  case KEY_RSHIFT:
    shifted = 1;
    break;

  case RELEASE(KEY_LSHIFT):
  case RELEASE(KEY_RSHIFT):
    shifted = 0;
    break;
  };

  if (code >= ARRAY_SIZE(scancodes)) {
    return;
  }

  ascii = scancodes[code][shifted];

  if (ascii) {
    putchar(ascii);
  }
}

void keyboard_init(void)
{
  uint8_t vector;

  vector = KEYBOARD_IRQ_VECTOR;
  set_idt_gate(vector, (void *) kb_handler);
  ioapic_setup_isairq(1, vector, IRQ_BOOTSTRAP);

  /*
   * Keyboard-Initialization Races:
   *
   * After the first key press, an edge IRQ1 get triggered and
   * the  pressed char get  buffered. Remaining  key  presses get
   * sliently buffered (without further edge IRQs) as long as the
   * the buffer is already non-empty. After consuming a char from
   * the buffer, and if the buffer is still non-empty after that
   * consumption, a new edge IRQ1 will get triggered.
   *
   * We may reach here with several chars  buffered, but with the
   * original edge IRQ  lost (not queued in the local APIC's IRR)
   * since the relevant IOAPIC entry was not yet setup, or masked.
   * Thus, make new keypresses  trigger an edge  IRQ1 by flushing
   * the kbd buffer, or they won't get detected in that case!
   *
   * (Test the above race by directly using the keyboard once the
   * kernel boots, with the buffer-flushing code off.)
   *
   * Doing such flush  before unmasking the  IOAPIC IRQ1 entry is
   * racy: an interrupt may occur directly after the flush but
   * before the IOAPIC setup, filling the keyboard buffer without
   * queuing IRQ1 in the local APIC IRR & hence making  us unable
   * to detect any further keypresses!
   *
   * (Test the above race by flushing the  buf before IOAPIC IRQ1
   * ummasking, with 5 seconds of delay in-between to press keys.)
   *
   * Thus, flush the keyboard buffer _after_ the IOAPIC setup.
   *
   * Due to i8042 firmware semantics, the flushing process itself
   * will trigger an interrupt if more than one scancode exist in
   * the buffer (a single keypress = 2 scancodes). A keyboard IRQ
   * may also get triggered after the IOAPIC setup but before the
   * flush. In these cases  an  IRQ1 will get queued in the local
   * APIC's IRR but the relevant scancodes will get flushed, i.e.,
   * after enabling interrupts  the keyboard  ISR will get called
   * with an empty i8042 buffer. That case is handled by checking
   * the "Output Buffer Full" bit before reading any kbd input.
   */
  kbd_flush_buffer();
}

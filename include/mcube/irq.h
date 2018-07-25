/**
 * @file include/mcube/irq.h
 *
 * @author Hiroyuki Chishiro
 */
#ifndef __MCUBE_MCUBE_IRQ_H__
#define __MCUBE_MCUBE_IRQ_H__

#ifndef __ASSEMBLY__

#define LOCAL_IRQ_ENABLED 1
#define LOCAL_IRQ_DISABLED 0

static inline void enable_local_irq(void);
static inline void disable_local_irq(void);

static inline void save_local_irq(unsigned long *flags);
static inline void restore_local_irq(unsigned long *flags);


void wait_until_next_interrupt(void);

void init_irq(void);

struct full_regs;

asmlinkage int do_irq(unsigned long irq, struct full_regs *regs);
unsigned int __do_irq(unsigned long irq);


#endif /* !__ASSEMBLY__ */

#endif /* __MCUBE_MCUBE_IRQ_H__ */

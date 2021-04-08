#ifndef _ASM_RISCV_FENCE_H
#define _ASM_RISCV_FENCE_H

#ifdef CONFIG_SMP
#define RISCV_ACQUIRE_BARRIER		"\tfence r , rw\n"
#define RISCV_RELEASE_BARRIER		"\tfence rw,  w\n"
#else
#define RISCV_ACQUIRE_BARRIER
#define RISCV_RELEASE_BARRIER
#endif

#define __acquire_fence() \
	__asm__ __volatile__(RISCV_ACQUIRE_BARRIER "" ::: "memory")

#define __release_fence() \
	__asm__ __volatile__(RISCV_RELEASE_BARRIER "" ::: "memory")

#endif	/* _ASM_RISCV_FENCE_H */

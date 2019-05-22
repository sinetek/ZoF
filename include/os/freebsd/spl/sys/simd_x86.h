
#ifdef _KERNEL
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <machine/fpu.h>
#include <x86/x86_var.h>
#include <x86/specialreg.h>
#endif

#ifdef _KERNEL
#define	kfpu_begin() {					\
	critical_enter();							\
	fpu_kern_enter(curthread, NULL, FPU_KERN_NOCTX); \
}

#define	kfpu_end()						\
{												   \
	fpu_kern_leave(curthread, NULL);				   \
	critical_exit();								   \
}
#else
#endif
/*
 * Check if OS supports AVX and AVX2 by checking XCR0
 * Only call this function if CPUID indicates that AVX feature is
 * supported by the CPU, otherwise it might be an illegal instruction.
 */
static inline uint64_t
xgetbv(uint32_t index)
{
	uint32_t eax, edx;
	/* xgetbv - instruction byte code */
	__asm__ __volatile__(".byte 0x0f; .byte 0x01; .byte 0xd0"
	    : "=a" (eax), "=d" (edx)
	    : "c" (index));

	return ((((uint64_t)edx)<<32) | (uint64_t)eax);
}


/*
 * Detect register set support
 */
static inline boolean_t
__simd_state_enabled(const uint64_t state)
{
	boolean_t has_osxsave;
	uint64_t xcr0;

#if defined(_KERNEL)

	has_osxsave = !!(cpu_feature2 & CPUID2_OSXSAVE);
#elif !defined(_KERNEL)
	has_osxsave = __cpuid_has_osxsave();
#endif

	if (!has_osxsave)
		return (B_FALSE);

	xcr0 = xgetbv(0);
	return ((xcr0 & state) == state);
}

#define	_XSTATE_SSE_AVX		(0x2 | 0x4)
#define	_XSTATE_AVX512		(0xE0 | _XSTATE_SSE_AVX)

#define	__ymm_enabled() __simd_state_enabled(_XSTATE_SSE_AVX)
#define	__zmm_enabled() __simd_state_enabled(_XSTATE_AVX512)

/*
 * Check if AVX instruction set is available
 */
static inline boolean_t
zfs_avx_available(void)
{
	boolean_t has_avx;

#if defined(_KERNEL)
#ifdef __linux__
#if defined(KERNEL_EXPORTS_X86_FPU)
	has_avx = !!boot_cpu_has(X86_FEATURE_AVX);
#else
	has_avx = B_FALSE;
#endif
#elif defined(__FreeBSD__)
	has_avx = !!(cpu_feature2 & CPUID2_AVX);
#endif		
#elif !defined(_KERNEL)
	has_avx = __cpuid_has_avx();
#endif

	return (has_avx && __ymm_enabled());
}

/*
 * Check if AVX2 instruction set is available
 */
static inline boolean_t
zfs_avx2_available(void)
{
	boolean_t has_avx2;
#if defined(_KERNEL)
#ifdef __linux__
#if defined(X86_FEATURE_AVX2) && defined(KERNEL_EXPORTS_X86_FPU)
	has_avx2 = !!boot_cpu_has(X86_FEATURE_AVX2);
#else
	has_avx2 = B_FALSE;
#endif
#elif defined(__FreeBSD__)
	has_avx2 = !!(cpu_stdext_feature & CPUID_STDEXT_AVX2);
#endif
#elif !defined(_KERNEL)
	has_avx2 = __cpuid_has_avx2();
#endif

	return (has_avx2 && __ymm_enabled());
}


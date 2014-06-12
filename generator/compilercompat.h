
/** http://reviews.llvm.org/rL201729 */
#if (__clang__ == 1) && !defined(__CLANG_MAX_ALIGN_T_DEFINED)
# if __STDC_VERSION__ >= 201112L || __cplusplus >= 201103L
typedef struct {
  long long __clang_max_align_nonce1
      __attribute__((__aligned__(__alignof__(long long))));
  long double __clang_max_align_nonce2
      __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#  define __CLANG_MAX_ALIGN_T_DEFINED
# endif
#endif


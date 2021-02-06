// rusini0.hh

/*    Copyright (C) 2020, 2021 Alexey Protasov (AKA Alex or rusini)

   This is free software: you can redistribute it and/or modify it under the terms of the version 3 of the GNU General Public License
   as published by the Free Software Foundation (and only version 3).

   This software is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this software.  If not, see <https://www.gnu.org/licenses/>.  */


# ifndef RSN_INCLUDED_RUSINI0
# define RSN_INCLUDED_RUSINI0

// Toolchain and Target Platform Checks ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

# if __cplusplus
   # if __cplusplus < 201703/*C++17*/ || !__STDC_HOSTED__ || !__GNUC__/*g++/clang++/icpc...*/ || !__STRICT_ANSI__/*-std=c++NN*/
      # error "Unsupported C++ compiler or compiler mode"
   # endif
# else
   # if __STDC_VERSION__ < 201112/*C11*/ || !__STDC_HOSTED__ || !__GNUC__/*gcc/clang/icc...*/ || !__STRICT_ANSI__/*-std=cNN*/
      # error "Unsupported C compiler or compiler mode"
   # endif
# endif
# if !(__i386__ && __SSE2_MATH__ || __x86_64__ || __AARCH64EL__ || __ARMEL__ && __VFP_FP__)
   # error "Unsupported or not tested target ISA or ABI"
# endif
# if !__linux__ && !__FreeBSD__
   # error "Either __linux__ or __FreeBSD__ is required"
# endif

// Compiler Idiosyncrasies /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

# define RSN_PACK          __attribute__((__packed__))
# define RSN_ALIGN(ALIGN)  __attribute__((__aligned__(ALIGN)))
# define RSN_PRIORITY(PRI) __attribute__((__init_priority__(PRI)))
// the "proper" way of advising about violations of Strict Aliasing Rules:
# define RSN_BARRIER()     __asm__ volatile ("" : : : "memory")
#
# if RSN_USE_PURE
   # define RSN_PURE      __attribute__((__const__))
   # define RSN_NOCLOBBER __attribute__((__pure__))
# elif RSN_USE_NOCLOBBER
   # define RSN_PURE      __attribute__((__pure__))
   # define RSN_NOCLOBBER __attribute__((__pure__))
# else
   # define RSN_PURE
   # define RSN_NOCLOBBER
# endif

# if RSN_USE_EXPECT
   # define RSN_LIKELY(...)   (!__builtin_expect(!(__VA_ARGS__), 0))
   # define RSN_UNLIKELY(...) (!__builtin_expect(!(__VA_ARGS__), 1))
# else
   # define RSN_LIKELY(...)   (__VA_ARGS__)
   # define RSN_UNLIKELY(...) (__VA_ARGS__)
# endif
# if RSN_USE_INLINE
   # define RSN_INLINE   __attribute__((__always_inline__))
   # define RSN_NOINLINE __attribute__((__noinline__))
# else
   # define RSN_INLINE
   # define RSN_NOINLINE
# endif
#
# define RSN_NORETURN      __attribute__((__noreturn__, __noinline__, __cold__))
# define RSN_UNREACHABLE() __builtin_unreachable()

# define RSN_NODISCARD   __attribute__((__warn_unused_result__))
# define RSN_FALLTHROUGH __attribute__((__fallthrough__));

# define RSN_DSO_HIDE         __attribute__(__visibility__("hidden"))
# define RSN_DSO_HIDE_BEGIN   _Pragma("GCC visibility push(hidden)")
# define RSN_DSO_HIDE_END     _Pragma("GCC visibility pop")
# define RSN_DSO_UNHIDE_BEGIN _Pragma("GCC visibility push(default)")
# define RSN_DSO_UNHIDE_END   _Pragma("GCC visibility pop")

// Conditionals ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

# if RSN_WITH_MULTITHREADING
   # define RSN_IF_WITH_MT(...) __VA_ARGS__
   # define RSN_IF_WITHOUT_MT(...)
# else
   # define RSN_IF_WITH_MT(...)
   # define RSN_IF_WITHOUT_MT(...) __VA_ARGS__
# endif

# if RSN_USE_DEBUG
   # include <cstdio> // fprintf, fputc, fputs, stderr
   # define RSN_IF_USING_DEBUG(...) __VA_ARGS__
   # define RSN_IF_NOT_USING_DEBUG(...)
# else
   # define RSN_IF_USING_DEBUG(...)
   # define RSN_IF_NOT_USING_DEBUG(...) __VA_ARGS__
# endif

# endif // # ifndef RSN_INCLUDED_RUSINI0

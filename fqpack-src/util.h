/**
 * @file util.h
 *
 * @date Dec 16, 2011
 * @author seth
 *
 * Copyright (c) 2011, Seth Hillbrand
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.  Redistributions in binary form must reproduce
 * the above copyright notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define fseeko _fseeki64
#define ftello _ftelli64
#endif

#define MIN_ALIGN_SIZE 				__BIGGEST_ALIGNMENT__
#define FQPACK_ALIGN __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)))

#define FQPACK_SAFE_FREE(x) 	\
	do{							\
		if(x) free(x);			\
		(x) = NULL;				\
	}while(0)

#define LT1(n) n, n
#define LT2(n) LT1(n), LT1(n)
#define LT3(n) LT2(n), LT2(n)
#define LT4(n) LT3(n), LT3(n)
#define LT5(n) LT4(n), LT4(n)
#define LT6(n) LT5(n), LT5(n)
#define LT7(n) LT6(n), LT6(n)

static const uint8_t fqpack_lt256[256] FQPACK_ALIGN =
{
		0, 0, LT1(1), LT2(2), LT3(3), LT4(4), LT5(5), LT6(6), LT7(7)
};
static inline uint8_t fqpack_ilog2(uint32_t m_val)
{

	uint8_t retval;
	register uint32_t temp1;
	register uint32_t temp2;

	temp2 = m_val >> 16;
	if (temp2)
	{
		temp1 = temp2 >> 8;
		if (temp1)
			retval = 24 + fqpack_lt256[temp1];
		else
			retval = 16 + fqpack_lt256[temp2];
	}
	else
	{
		temp1 = m_val >> 8;
		if (temp1)
			retval = 8 + fqpack_lt256[temp1];
		else
			retval = fqpack_lt256[m_val];
	}

	return retval;
}

#define IS_POW_TWO(x) (!((x) & ((x)-1)))
static inline size_t fqpack_next_pow_two(size_t m_size)
{
	uint32_t shift_val;
	if (IS_POW_TWO(m_size)) return m_size;
	if ((sizeof(size_t) > 4) && ((shift_val = fqpack_ilog2(m_size >> 32))))
	{
		return (((size_t)1)<<(shift_val + 33));
	}
	return (((size_t)1)<<(fqpack_ilog2(m_size) + 1));
}

/**
 * Provides a wrapper for posix_memalign
 * @param m_size Size of the memory to be allocated (in bytes)
 * @return pointer to the allocated memory or NULL on failure
 */
static inline void *fqpack_aligned_malloc(size_t m_size, bool m_clear)
{
	void *aligned_ptr;

	m_size = (m_size + MIN_ALIGN_SIZE - 1) & (~ (MIN_ALIGN_SIZE - 1));
#ifdef _WIN32
	aligned_ptr = malloc(m_size);
	if (!aligned_ptr)
	{
		return NULL;
	}
#else
	if (posix_memalign(&aligned_ptr, MIN_ALIGN_SIZE, m_size) < 0)
	{
		return NULL;
	}
#endif

	if (m_clear)
	{
		memset(aligned_ptr, 0, m_size);
	}

	return aligned_ptr;
}

static inline void fqpack_debug_output(const char *m_filename, void *m_data, size_t m_size)
{
	FILE *fp = fopen(m_filename, "w+");

	if (!fp) return;

	fwrite(m_data, 1, m_size, fp);
	fclose(fp);
}


#endif /* UTIL_H_ */

/**
 * @file model.h
 *
 * @date Dec 22, 2011
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

#ifndef MODEL_H_
#define MODEL_H_

#include <stdint.h>
#include <stdbool.h>

#include <bytemap.h>
#include <util.h>
#include <mixer.h>

#define FQPACK_MAX_CODEX_LEN 		(256)
#define FQPACK_MAX_CHAR_BITS 		(8)
#define FQPACK_MAX_CHAR_BITS_SHIFT 	(3)
#define FQPACK_MAX_RUN_BITS 		(32)
#define FQPACK_MAX_RUN_BITS_SHIFT 	(5)

#define FQPACK_MAX_SEQ_CODEX_LEN	(8)
#define FQPACK_MAX_SEQ_CONTEXT_BITS	(2)
#define FQPACK_MAX_SEQ_CODEX_BITS	(3)
#define FQPACK_MAX_SEQ_MIXER_BITS 	(16)

#define FQPACK_MAX_QUAL_CONTEXT		(2048)
#define FQPACK_MAX_QUAL_CODEX_LEN	(64)
#define FQPACK_MAX_QUAL_CODEX_BITS	(6)
#define FQPACK_QUAL_BIT_MIXER_LEN	(2 * FQPACK_MAX_QUAL_CODEX_LEN)

#define FQPACK_MAX_CONTEXT 			(256)
#define FQPACK_MAX_CONTEXT_SHIFT 	(8)
#define FQPACK_POS_QUANTA 			(256)
#define FQPACK_CONTEXT_INIT			(2048)

#define FQ_FIXED	(0)
#define FQ_O1		(0)
#define FQ_CTX		(1)
#define FQ_QUAL		(2)
#define FQ_POS		(2)

typedef struct fqpack_stream fqpack_stream_t;

typedef struct
{
	size_t codex_size;
	size_t order;

	mixer_t char_bits_mixer[FQPACK_MAX_CHAR_BITS][FQPACK_MAX_CHAR_BITS] FQPACK_ALIGN;
	mixer_t char_val_mixer[FQPACK_MAX_CHAR_BITS] FQPACK_ALIGN;

	mixer_t run_bits_mixer[FQPACK_MAX_RUN_BITS] FQPACK_ALIGN;
	mixer_t run_length_mixer[FQPACK_MAX_RUN_BITS] FQPACK_ALIGN;

	model_params_t char_bits_param;
	model_params_t char_val_param;
	model_params_t run_bits_param;
	model_params_t run_len_param;

	struct
	{
		struct
		{
			int16_t fixed[FQPACK_MAX_CHAR_BITS] FQPACK_ALIGN;
			int16_t *context; /// CODEX_SIZE ** ORDER * FQPACK_MAX_CHAR_BITS
		} bits FQPACK_ALIGN;

		struct
		{
			int16_t fixed[FQPACK_MAX_CONTEXT] FQPACK_ALIGN;
			int16_t *context; /// CODEX_SIZE ** ORDER * FQPACK_MAX_CONTEXT
		} val[FQPACK_MAX_CHAR_BITS] FQPACK_ALIGN;

	} character FQPACK_ALIGN;

	struct
	{
		struct
		{
			int16_t fixed[FQPACK_MAX_RUN_BITS] FQPACK_ALIGN;
			int16_t *context; /// CODEX_SIZE ** ORDER * FQPACK_MAX_RUN_BITS
		} bits;

		struct
		{
			int16_t fixed[FQPACK_MAX_RUN_BITS] FQPACK_ALIGN;
			int16_t *context; /// CODEX_SIZE ** ORDER * FQPACK_MAX_RUN_BITS
		} length[FQPACK_MAX_RUN_BITS] FQPACK_ALIGN;

	} run FQPACK_ALIGN;
} rle_model_t;

typedef struct
{
	bytemap_t map;
	size_t codex_size;
	size_t order;

	mixer_t char_bits_mixer[FQPACK_MAX_SEQ_MIXER_BITS][FQPACK_MAX_SEQ_CONTEXT_BITS] FQPACK_ALIGN;
	mixer_t char_val_mixer[FQPACK_MAX_SEQ_CODEX_BITS] FQPACK_ALIGN;

	model_params_t char_bits_param;
	model_params_t char_val_param;

	struct
	{
		struct
		{
			int16_t *o1; /// CODEX_SIZE * FQPACK_MAX_SEQ_CONTEXT_BITS
			int16_t *context; /// CODEX_SIZE ** ORDER * FQPACK_MAX_SEQ_CONTEXT_BITS
			int16_t *qual; /// FQPACK_MAX_QUAL_CONTEXT * FQPACK_MAX_SEQ_CONTEXT_BITS
		} bits FQPACK_ALIGN;

		struct
		{
			int16_t *o1; /// CODEX_SIZE
			int16_t *context; /// CODEX_SIZE ** ORDER
			int16_t *qual; /// FQPACK_MAX_QUAL_CONTEXT
		} val[FQPACK_MAX_SEQ_CODEX_BITS] FQPACK_ALIGN;

	} character FQPACK_ALIGN;

} sequence_model_t;


typedef struct
{
	size_t codex_size;
	size_t order;

	mixer_t char_bits_mixer[FQPACK_QUAL_BIT_MIXER_LEN][FQPACK_MAX_QUAL_CODEX_BITS];
	mixer_t char_val_mixer[FQPACK_MAX_QUAL_CODEX_BITS];

	model_params_t char_bits_param;
	model_params_t char_val_param;
	struct
	{
		struct
		{
			int16_t o0[FQPACK_MAX_QUAL_CODEX_BITS]; /// CODEX_SIZE * FQPACK_MAX_QUAL_CODEX_BITS;
			int16_t *pos; /// FQPACK_POS_QUANTA * FQPACK_MAX_QUAL_CODEX_BITS
			int16_t *context; /// CODEX_SIZE ** ORDER * FQPACK_MAX_QUAL_CODEX_BITS
		} bits;

		struct
		{
			int16_t o0[FQPACK_MAX_QUAL_CODEX_LEN]; /// CODEX_SIZE * FQPACK_MAX_QUAL_CODEX_LEN;
			int16_t *pos; /// FQPACK_POS_QUANTA * FQPACK_MAX_QUAL_CODEX_LEN
			int16_t *context; /// (codex_size ** order) * next_pow_two(codex_size)
		} val[FQPACK_MAX_QUAL_CODEX_BITS];

	} character FQPACK_ALIGN;

} quality_model_t;


rle_model_t *model_initialize_rle(rle_model_t *new_model, const size_t m_order, bool m_continue);
sequence_model_t *model_initialize_sequence(sequence_model_t *m_model, size_t m_codex_size, size_t m_order, bool m_continue);
quality_model_t *model_initialize_quality(quality_model_t *m_model, size_t m_codex, size_t m_order, bool m_continue);

void model_free_rle(rle_model_t *m_model);
void model_free_sequence(sequence_model_t *m_model);
void model_free_quality(quality_model_t *m_model);

ssize_t model_compress_quality(fqpack_stream_t *m_stream, size_t m_len);
ssize_t model_compress_sequence(fqpack_stream_t *m_stream, size_t m_len);
ssize_t model_compress_rle(uint8_t *m_input, uint8_t *m_output, rle_model_t *m_model, size_t m_len);

ssize_t model_uncompress_quality(fqpack_stream_t *m_stream);
ssize_t model_uncompress_sequence(fqpack_stream_t *m_stream);
ssize_t model_uncompress_rle(uint8_t *m_input, uint8_t *m_output, rle_model_t *m_model, size_t m_len);

#endif /* MODEL_H_ */

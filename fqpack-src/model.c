/**
 * @file model.c
 *
 * @date Dec 29, 2011
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
#include <stdint.h>
#include <stdbool.h>

#include <fastqpack.h>
#include <arithcoder.h>
#include <bytemap.h>
#include <mixer.h>
#include <util.h>
#include <model.h>

static const uint32_t model_seq_masks[32] FQPACK_ALIGN =
{
		0b11111111111111111111111111111100,
		0,
		0b11111111111111111111111111110011,
		0,
		0b11111111111111111111111111001111,
		0,
		0b11111111111111111111111100111111,
		0,
		0b11111111111111111111110011111111,
		0,
		0b11111111111111111111001111111111,
		0,
		0b11111111111111111100111111111111,
		0,
		0b11111111111111110011111111111111,
		0,
		0b11111111111111001111111111111111,
		0,
		0b11111111111100111111111111111111,
		0,
		0b11111111110011111111111111111111,
		0,
		0b11111111001111111111111111111111,
		0,
		0b11111100111111111111111111111111,
		0,
		0b11110011111111111111111111111111,
		0,
		0b11001111111111111111111111111111,
		0,
		0b00111111111111111111111111111111
};

void model_free_rle(rle_model_t *m_model)
{
	free(m_model->character.bits.context);

	for (size_t i = 0; i < FQPACK_MAX_CHAR_BITS; i++)
	{
		free(m_model->character.val[i].context);
	}
	free(m_model->run.bits.context);

	for (size_t i = 0; i < FQPACK_MAX_RUN_BITS; i++)
	{
		free(m_model->run.length[i].context);
	}
	free(m_model);

}

void model_free_sequence(sequence_model_t *m_model)
{
	free(m_model->character.bits.context);
	free(m_model->character.bits.o1);
	free(m_model->character.bits.qual);

	for (size_t i = 0; i < FQPACK_MAX_SEQ_CODEX_BITS; i++)
	{
		free(m_model->character.val[i].context);
		free(m_model->character.val[i].o1);
		free(m_model->character.val[i].qual);
	}

	free(m_model);

}

void model_free_quality(quality_model_t *m_model)
{
	free(m_model->character.bits.context);
	free(m_model->character.bits.pos);

	for (size_t i = 0; i < FQPACK_MAX_QUAL_CODEX_BITS; i++)
	{
		free(m_model->character.val[i].context);
		free(m_model->character.val[i].pos);
	}

	free(m_model);

}

static rle_model_t *model_new_rle(const size_t m_codex_size, const size_t m_order)
{
	rle_model_t *new_model = NULL;
	size_t order_size = 0;

	if (!(new_model = fqpack_aligned_malloc(sizeof(rle_model_t), true)))
		return NULL;

	order_size = m_codex_size * m_order;

	/**
	 * Allocate and initialize the character predictors
	 */

	new_model->character.bits.context = fqpack_aligned_malloc(sizeof(int16_t) * order_size * FQPACK_MAX_CHAR_BITS, false);

	for (size_t i = 0; i < FQPACK_MAX_CHAR_BITS; i++)
		new_model->character.val[i].context = fqpack_aligned_malloc(sizeof(int16_t) * order_size * FQPACK_MAX_CONTEXT, false);

	/**
	 * Allocate and initialize the run length predictors
	 */

	new_model->run.bits.context = fqpack_aligned_malloc(sizeof(int16_t) * order_size * FQPACK_MAX_RUN_BITS, false);

	for (size_t i = 0; i < FQPACK_MAX_RUN_BITS; i++)
		new_model->run.length[i].context = fqpack_aligned_malloc(sizeof(int16_t) * order_size * FQPACK_MAX_RUN_BITS, false);

	return new_model;
}

rle_model_t *model_initialize_rle(rle_model_t *m_model, const size_t m_order, bool m_continue)
{
	size_t order_size = m_order * 256;

	if (m_model && m_continue) return m_model;

	if (!m_model)
	{
		if (!(m_model = model_new_rle(256, m_order))) return NULL;
	}

	m_model->codex_size = 256;
	m_model->order = m_order;

	/**
	 * Allocate and initialize the mixers
	 */
	for (size_t i = 0; i < FQPACK_MAX_CHAR_BITS; i++)
	{
		mixer_init(&m_model->char_val_mixer[i]);
		for (size_t j = 0; j < FQPACK_MAX_CHAR_BITS; j++)
			mixer_init(&m_model->char_bits_mixer[i][j]);
	}
	for (size_t i = 0; i < FQPACK_MAX_RUN_BITS; i++)
	{
		mixer_init(&m_model->run_length_mixer[i]);
		mixer_init(&m_model->run_bits_mixer[i]);
	}

	/**
	 * Initialize tunable parameter values
	 */
	m_model->char_bits_param.adaptation_rate[FQ_FIXED][0] = 1442;
	m_model->char_bits_param.adaptation_rate[FQ_FIXED][1] = 877;
	m_model->char_bits_param.adaptation_rate[FQ_CTX][0] = 165;
	m_model->char_bits_param.adaptation_rate[FQ_CTX][1] = 120;
	m_model->char_bits_param.adaptation_rate[2][0] = 0;
	m_model->char_bits_param.adaptation_rate[2][1] = 0;

	m_model->char_bits_param.threshhold[FQ_FIXED][0] = 20;
	m_model->char_bits_param.threshhold[FQ_FIXED][1] = 100;
	m_model->char_bits_param.threshhold[FQ_CTX][0] = 100;
	m_model->char_bits_param.threshhold[FQ_CTX][1] = 116;
	m_model->char_bits_param.threshhold[2][0] = 0;
	m_model->char_bits_param.threshhold[2][1] = 0;

	m_model->char_bits_param.learning_rate[FQ_FIXED] = 80;
	m_model->char_bits_param.learning_rate[FQ_CTX] = 75;
	m_model->char_bits_param.learning_rate[2] = 0;

	m_model->char_val_param.adaptation_rate[FQ_FIXED][0] = 1260;
	m_model->char_val_param.adaptation_rate[FQ_FIXED][1] = 980;
	m_model->char_val_param.adaptation_rate[FQ_CTX][0] = 75;
	m_model->char_val_param.adaptation_rate[FQ_CTX][1] = 77;
	m_model->char_val_param.adaptation_rate[2][0] = 0;
	m_model->char_val_param.adaptation_rate[2][1] = 0;

	m_model->char_val_param.threshhold[FQ_FIXED][0] = 20;
	m_model->char_val_param.threshhold[FQ_FIXED][1] = -6;
	m_model->char_val_param.threshhold[FQ_CTX][0] = -40;
	m_model->char_val_param.threshhold[FQ_CTX][1] = -45;
	m_model->char_val_param.threshhold[2][0] = 0;
	m_model->char_val_param.threshhold[2][1] = 0;

	m_model->char_val_param.learning_rate[FQ_FIXED] = 113;
	m_model->char_val_param.learning_rate[FQ_CTX] = 155;
	m_model->char_val_param.learning_rate[2] = 0;

	m_model->run_bits_param.adaptation_rate[FQ_FIXED][0] = 1400;
	m_model->run_bits_param.adaptation_rate[FQ_FIXED][1] = 1400;
	m_model->run_bits_param.adaptation_rate[FQ_CTX][0] = 150;
	m_model->run_bits_param.adaptation_rate[FQ_CTX][1] = 150;
	m_model->run_bits_param.adaptation_rate[2][0] = 0;
	m_model->run_bits_param.adaptation_rate[2][1] = 0;

	m_model->run_bits_param.threshhold[FQ_FIXED][0] = 200;
	m_model->run_bits_param.threshhold[FQ_FIXED][1] = 150;
	m_model->run_bits_param.threshhold[FQ_CTX][0] = 200;
	m_model->run_bits_param.threshhold[FQ_CTX][1] = 150;
	m_model->run_bits_param.threshhold[2][0] = 0;
	m_model->run_bits_param.threshhold[2][1] = 0;

	m_model->run_bits_param.learning_rate[FQ_FIXED] = 125;
	m_model->run_bits_param.learning_rate[FQ_CTX] = 25;
	m_model->run_bits_param.learning_rate[2] = 0;

	m_model->run_len_param.adaptation_rate[FQ_FIXED][0] = 140;
	m_model->run_len_param.adaptation_rate[FQ_FIXED][1] = 100;
	m_model->run_len_param.adaptation_rate[FQ_CTX][0] = 100;
	m_model->run_len_param.adaptation_rate[FQ_CTX][1] = 400;
	m_model->run_len_param.adaptation_rate[2][0] = 0;
	m_model->run_len_param.adaptation_rate[2][1] = 0;

	m_model->run_len_param.threshhold[FQ_FIXED][0] = 2;
	m_model->run_len_param.threshhold[FQ_FIXED][1] = 10;
	m_model->run_len_param.threshhold[FQ_CTX][0] = 84;
	m_model->run_len_param.threshhold[FQ_CTX][1] = 37;
	m_model->run_len_param.threshhold[2][0] = 0;
	m_model->run_len_param.threshhold[2][1] = 0;

	m_model->run_len_param.learning_rate[FQ_FIXED] = 25;
	m_model->run_len_param.learning_rate[FQ_CTX] = 40;
	m_model->run_len_param.learning_rate[2] = 0;

	/**
	 * Allocate and initialize the character predictors
	 */

	for (size_t i = 0; i < FQPACK_MAX_CHAR_BITS; i++) m_model->character.bits.fixed[i] = FQPACK_CONTEXT_INIT;
	for (size_t i = 0; i < order_size * FQPACK_MAX_CHAR_BITS; i++) m_model->character.bits.context[i] = FQPACK_CONTEXT_INIT;

	for (size_t i = 0; i < FQPACK_MAX_CHAR_BITS; i++)
	{
		for (size_t j = 0; j < FQPACK_MAX_CONTEXT; j++) m_model->character.val[i].fixed[j] = FQPACK_CONTEXT_INIT;
		for (size_t j = 0; j < order_size * FQPACK_MAX_CONTEXT; j++) m_model->character.val[i].context[j] = FQPACK_CONTEXT_INIT;
	}

	/**
	 * Allocate and initialize the run length predictors
	 */

	for (size_t i = 0; i < FQPACK_MAX_RUN_BITS; i++) m_model->run.bits.fixed[i] = FQPACK_CONTEXT_INIT;
	for (size_t i = 0; i < order_size * FQPACK_MAX_RUN_BITS; i++) m_model->run.bits.context[i] = FQPACK_CONTEXT_INIT;

	for (size_t i = 0; i < FQPACK_MAX_RUN_BITS; i++)
	{
		for (size_t j = 0; j < FQPACK_MAX_RUN_BITS; j++) m_model->run.length[i].fixed[j] = FQPACK_CONTEXT_INIT;
		for (size_t j = 0; j < order_size * FQPACK_MAX_RUN_BITS; j++) m_model->run.length[i].context[j] = FQPACK_CONTEXT_INIT;
	}

	return m_model;
}


static sequence_model_t *model_new_sequence(size_t m_codex_size, size_t m_order)
{
	sequence_model_t *new_model = NULL;
	size_t order_size;

	if (!(new_model = fqpack_aligned_malloc(sizeof(sequence_model_t), true)))
		return NULL;

	new_model->codex_size = m_codex_size;
	new_model->order = m_order;
	order_size = 1 << (2 * new_model->order);


	/**
	 * Allocate the character predictors
	 */

	new_model->character.bits.context =
			fqpack_aligned_malloc(sizeof(int16_t) * order_size * FQPACK_MAX_SEQ_CONTEXT_BITS, false);
	new_model->character.bits.qual =
			fqpack_aligned_malloc(sizeof(int16_t) * FQPACK_MAX_SEQ_CONTEXT_BITS * FQPACK_MAX_QUAL_CONTEXT, false);
	new_model->character.bits.o1 =
			fqpack_aligned_malloc(sizeof(int16_t) * 4 * FQPACK_MAX_SEQ_CONTEXT_BITS, false);

	for (size_t i = 0; i < FQPACK_MAX_SEQ_CODEX_BITS; i++)
	{
		new_model->character.val[i].context = fqpack_aligned_malloc(sizeof(int16_t) * order_size, false);
		new_model->character.val[i].qual = fqpack_aligned_malloc(sizeof(int16_t) * FQPACK_MAX_QUAL_CONTEXT, false);
		new_model->character.val[i].o1 = fqpack_aligned_malloc(sizeof(int16_t) * 4, false);
	}

	return new_model;
}

sequence_model_t *model_initialize_sequence(sequence_model_t *m_model, size_t m_codex_size, size_t m_order, bool m_continue)
{
	size_t codex_size = 0;
	size_t order_size;

	codex_size = fqpack_next_pow_two(m_codex_size);

	if (m_model && m_continue) return m_model;

	if (!m_model) m_model = model_new_sequence(codex_size, m_order);
	else if (m_order != m_model->order || codex_size != m_model->codex_size)
	{
		model_free_sequence(m_model);
		m_model = model_new_sequence(codex_size, m_order);
	}

	if (!m_model) return NULL;

	order_size = 1 << (2 * m_model->order); /// Sequence codex is always 2.  We mix context for non-standard symbols

	/**
	 * Allocate and initialize the mixers
	 */
	for (size_t j = 0; j < FQPACK_MAX_SEQ_CODEX_BITS; j++)
	{
		mixer_init(&m_model->char_val_mixer[j]);
		for (size_t i = 0; i < FQPACK_MAX_SEQ_MIXER_BITS; i++)
		{
			mixer_init(&m_model->char_bits_mixer[i][j]);
		}
	}

	/**
	 * Initialize tunable parameter values
	 */

	m_model->char_bits_param.adaptation_rate[FQ_O1][0] = 500;
	m_model->char_bits_param.adaptation_rate[FQ_O1][1] = 200;
	m_model->char_bits_param.adaptation_rate[FQ_CTX][0] = 310;
	m_model->char_bits_param.adaptation_rate[FQ_CTX][1] = 250;
	m_model->char_bits_param.adaptation_rate[FQ_QUAL][0] = 40;
	m_model->char_bits_param.adaptation_rate[FQ_QUAL][1] = 100;

	m_model->char_bits_param.threshhold[FQ_O1][0] = 20;
	m_model->char_bits_param.threshhold[FQ_O1][1] = 50;
	m_model->char_bits_param.threshhold[FQ_CTX][0] = 20;
	m_model->char_bits_param.threshhold[FQ_CTX][1] = 30;
	m_model->char_bits_param.threshhold[FQ_QUAL][0] = 60;
	m_model->char_bits_param.threshhold[FQ_QUAL][1] = 50;

	m_model->char_bits_param.learning_rate[FQ_O1] = 140;
	m_model->char_bits_param.learning_rate[FQ_CTX] = 140;
	m_model->char_bits_param.learning_rate[FQ_QUAL] = 120;

	m_model->char_val_param.adaptation_rate[FQ_O1][0] = 600;
	m_model->char_val_param.adaptation_rate[FQ_O1][1] = 400;
	m_model->char_val_param.adaptation_rate[FQ_CTX][0] = 200;
	m_model->char_val_param.adaptation_rate[FQ_CTX][1] = 200;
	m_model->char_val_param.adaptation_rate[FQ_QUAL][0] = 38;
	m_model->char_val_param.adaptation_rate[FQ_QUAL][1] = 50;

	m_model->char_val_param.threshhold[FQ_O1][0] = 20;
	m_model->char_val_param.threshhold[FQ_O1][1] = 15;
	m_model->char_val_param.threshhold[FQ_CTX][0] = 30;
	m_model->char_val_param.threshhold[FQ_CTX][1] = 50;
	m_model->char_val_param.threshhold[FQ_QUAL][0] = 100;
	m_model->char_val_param.threshhold[FQ_QUAL][1] = 200;

	m_model->char_val_param.learning_rate[FQ_O1] = 50;
	m_model->char_val_param.learning_rate[FQ_CTX] = 300;
	m_model->char_val_param.learning_rate[FQ_QUAL] = 50;

	/**
	 * Initialize the character predictors
	 */

	for (size_t i = 0; i < FQPACK_MAX_SEQ_CONTEXT_BITS * FQPACK_MAX_QUAL_CONTEXT; i++)
		m_model->character.bits.qual[i] = FQPACK_CONTEXT_INIT;
	for (size_t j = 0; j < 8; j++)
		m_model->character.bits.o1[j] = FQPACK_CONTEXT_INIT;
	for (size_t i = 0; i < order_size * FQPACK_MAX_SEQ_CONTEXT_BITS; i++)
		m_model->character.bits.context[i] = FQPACK_CONTEXT_INIT;

	for (size_t i = 0; i < FQPACK_MAX_SEQ_CODEX_BITS; i++)
	{
		for (size_t j = 0; j < FQPACK_MAX_QUAL_CONTEXT; j++)
			m_model->character.val[i].qual[j] = FQPACK_CONTEXT_INIT;
		for (size_t j = 0; j < 4; j++)
			m_model->character.val[i].o1[j] = FQPACK_CONTEXT_INIT;
		for (size_t j = 0; j < order_size; j++)
			m_model->character.val[i].context[j] = FQPACK_CONTEXT_INIT;
	}

	return m_model;
}


static quality_model_t *model_new_quality(size_t m_codex_size, size_t m_order)
{
	quality_model_t *new_model = NULL;
	size_t order_size;

	if (!(new_model = fqpack_aligned_malloc(sizeof(quality_model_t), true)))
		return NULL;

	new_model->codex_size = m_codex_size;
	new_model->order = m_order;
	order_size = 1 << ((fqpack_lt256[m_codex_size - 1] + 1) * new_model->order);

	/**
	 * Allocate and initialize the character predictors
	 */

	new_model->character.bits.context =
			fqpack_aligned_malloc(sizeof(int16_t) * order_size * FQPACK_MAX_QUAL_CODEX_BITS, false);
	new_model->character.bits.pos =
			fqpack_aligned_malloc(sizeof(int16_t) * FQPACK_POS_QUANTA * FQPACK_MAX_QUAL_CODEX_BITS, false);

	for (size_t i = 0; i < FQPACK_MAX_QUAL_CODEX_BITS; i++)
	{
		new_model->character.val[i].context =
				fqpack_aligned_malloc(sizeof(int16_t) * order_size * m_codex_size, false);
		new_model->character.val[i].pos =
				fqpack_aligned_malloc(sizeof(int16_t) * FQPACK_POS_QUANTA * m_codex_size, false);
	}

	return new_model;
}

quality_model_t *model_initialize_quality(quality_model_t *m_model, size_t m_codex_size, size_t m_order, bool m_continue)
{
	size_t codex_size = 0;
	size_t order_size;

	codex_size = fqpack_next_pow_two(m_codex_size);

	if (m_model && m_continue) return m_model;

	if (!m_model)
	{
		m_model = model_new_quality(codex_size, m_order);
	}
	else if (m_order != m_model->order || codex_size != m_model->codex_size)
	{
		model_free_quality(m_model);
		m_model = model_new_quality(codex_size, m_order);
	}

	if (!m_model) return NULL;

	m_model->codex_size = codex_size;
	m_model->order = m_order;
	order_size = 1 << ((fqpack_lt256[codex_size - 1] + 1) * m_model->order);


	/**
	 * Allocate and initialize the mixers
	 */
	for (size_t j = 0; j < FQPACK_MAX_QUAL_CODEX_BITS; j++)
	{
		for (size_t i = 0; i < FQPACK_QUAL_BIT_MIXER_LEN; i++)
		{
			mixer_init(&m_model->char_bits_mixer[i][j]);
		}
		mixer_init(&m_model->char_val_mixer[j]);
	}

	/**
	 * Initialize tunable parameter values
	 */

	m_model->char_bits_param.adaptation_rate[FQ_FIXED][0] = 800;
	m_model->char_bits_param.adaptation_rate[FQ_FIXED][1] = 1600;
	m_model->char_bits_param.adaptation_rate[FQ_CTX][0] = 150;
	m_model->char_bits_param.adaptation_rate[FQ_CTX][1] = 110;
	m_model->char_bits_param.adaptation_rate[FQ_POS][0] = 50;
	m_model->char_bits_param.adaptation_rate[FQ_POS][1] = 40;

	m_model->char_bits_param.threshhold[FQ_FIXED][0] = 20;
	m_model->char_bits_param.threshhold[FQ_FIXED][1] = 10;
	m_model->char_bits_param.threshhold[FQ_CTX][0] = 60;
	m_model->char_bits_param.threshhold[FQ_CTX][1] = 40;
	m_model->char_bits_param.threshhold[FQ_POS][0] = 80;
	m_model->char_bits_param.threshhold[FQ_POS][1] = 200;

	m_model->char_bits_param.learning_rate[FQ_FIXED] = 60;
	m_model->char_bits_param.learning_rate[FQ_CTX] = 20;
	m_model->char_bits_param.learning_rate[FQ_POS] = 30;

	m_model->char_val_param.adaptation_rate[FQ_FIXED][0] = 800;
	m_model->char_val_param.adaptation_rate[FQ_FIXED][1] = 1100;
	m_model->char_val_param.adaptation_rate[FQ_CTX][0] = 80;
	m_model->char_val_param.adaptation_rate[FQ_CTX][1] = 70;
	m_model->char_val_param.adaptation_rate[FQ_POS][0] = 15;
	m_model->char_val_param.adaptation_rate[FQ_POS][1] = 30;

	m_model->char_val_param.threshhold[FQ_FIXED][0] = 10;
	m_model->char_val_param.threshhold[FQ_FIXED][1] = 5;
	m_model->char_val_param.threshhold[FQ_CTX][0] = 35;
	m_model->char_val_param.threshhold[FQ_CTX][1] = 30;
	m_model->char_val_param.threshhold[FQ_POS][0] = 50;
	m_model->char_val_param.threshhold[FQ_POS][1] = 100;

	m_model->char_val_param.learning_rate[FQ_FIXED] = 70;
	m_model->char_val_param.learning_rate[FQ_CTX] = 20;
	m_model->char_val_param.learning_rate[FQ_POS] = 45;


	/**
	 * Allocate and initialize the character predictors
	 */

	for (size_t i = 0; i < order_size * FQPACK_MAX_QUAL_CODEX_BITS; i++)
		m_model->character.bits.context[i] = FQPACK_CONTEXT_INIT;
	for (size_t i = 0; i < FQPACK_POS_QUANTA * FQPACK_MAX_QUAL_CODEX_BITS; i++)
		m_model->character.bits.pos[i] = FQPACK_CONTEXT_INIT;
	for (size_t i = 0; i < FQPACK_MAX_QUAL_CODEX_BITS; i++)
		m_model->character.bits.o0[i] = FQPACK_CONTEXT_INIT;

	for (size_t i = 0; i < FQPACK_MAX_QUAL_CODEX_BITS; i++)
	{
		for (size_t j = 0; j < order_size * codex_size; j++)
			m_model->character.val[i].context[j] = FQPACK_CONTEXT_INIT;
		for (size_t j = 0; j < FQPACK_POS_QUANTA * codex_size; j++)
			m_model->character.val[i].pos[j] = FQPACK_CONTEXT_INIT;
		for (size_t j = 0; j < FQPACK_MAX_QUAL_CODEX_LEN; j++)
			m_model->character.val[i].o0[j] = FQPACK_CONTEXT_INIT;
	}

	return m_model;

}

static inline void model_4state_encode_bits(mixer_t *m_mixer, arithcoder_t *m_coder,
		int16_t *m_predictor0, int16_t *m_predictor1, int16_t *m_predictor2, int16_t *m_predictor3,
		model_params_t *m_params, uint32_t m_rank_bits, uint32_t m_codex_bits)
{
	int16_t probability;

	for (uint32_t bit = 0; bit < m_rank_bits; ++bit)
	{
		probability = mixer_mix4_adapt_bit1(m_mixer,
							*m_predictor0,
							*m_predictor1,
							*m_predictor2,
							*m_predictor3,
							m_params);
		MIXER4_ADJUST_BIT1(	m_predictor0,
							m_predictor1,
							m_predictor2,
							m_predictor3,
							(*m_params));
		ac_encode_bit1(m_coder, probability);

		m_predictor0++;
		m_predictor1++;
		m_predictor2++;
		m_predictor3++;
		m_mixer++;
	}

	if (m_rank_bits < m_codex_bits - 1)
	{
		probability = mixer_mix4_adapt_bit0(m_mixer,
							*m_predictor0,
							*m_predictor1,
							*m_predictor2,
							*m_predictor3,
							m_params);
		MIXER4_ADJUST_BIT0(	m_predictor0,
							m_predictor1,
							m_predictor2,
							m_predictor3,
							(*m_params));
		ac_encode_bit0(m_coder, probability);
	}
}

static inline void model_3state_encode_bits(mixer_t *m_mixer, arithcoder_t *m_coder,
		int16_t *m_predictor0, int16_t *m_predictor1, int16_t *m_predictor2,
		model_params_t *m_params, uint32_t m_rank_bits, uint32_t m_codex_bits)
{
	int16_t probability;

	for (uint32_t bit = 0; bit < m_rank_bits; ++bit)
	{
		probability = mixer_mix3_adapt_bit1(m_mixer,
							*m_predictor0,
							*m_predictor1,
							*m_predictor2,
							m_params);
		MIXER3_ADJUST_BIT1(	m_predictor0,
							m_predictor1,
							m_predictor2,
							(*m_params));
		ac_encode_bit1(m_coder, probability);

		m_predictor0++;
		m_predictor1++;
		m_predictor2++;
		m_mixer++;
	}

	if (m_rank_bits < m_codex_bits - 1)
	{
		probability = mixer_mix3_adapt_bit0(m_mixer,
							*m_predictor0,
							*m_predictor1,
							*m_predictor2,
							m_params);
		MIXER3_ADJUST_BIT0(	m_predictor0,
							m_predictor1,
							m_predictor2,
							(*m_params));
		ac_encode_bit0(m_coder, probability);
	}
}

static inline void model_2state_encode_bits(mixer_t *m_mixer, arithcoder_t *m_coder,
		int16_t *m_predictor0, int16_t *m_predictor1, model_params_t *m_params,
		int_fast32_t m_rank_bits, int_fast32_t m_codex_bits)
{
	int16_t probability;

	for (int_fast32_t bit = 0; bit < m_rank_bits; ++bit)
	{
		probability = mixer_mix2_adapt_bit1(m_mixer, *m_predictor0, *m_predictor1, m_params);
		MIXER2_ADJUST_BIT1(m_predictor0, m_predictor1, (*m_params));
		ac_encode_bit1(m_coder, probability);

		m_predictor1++;
		m_predictor0++;
		m_mixer++;
	}

	if (m_rank_bits < m_codex_bits - 1)
	{
		probability = mixer_mix2_adapt_bit0(m_mixer, *m_predictor0, *m_predictor1, m_params);
		MIXER2_ADJUST_BIT0(m_predictor0, m_predictor1, (*m_params));
		ac_encode_bit0(m_coder, probability);
	}
}

ssize_t model_compress_rle(uint8_t *m_input, uint8_t *m_output, rle_model_t *m_model, size_t m_len)
{
	int16_t		probability;
	int16_t 	*fixed_predictor;
	int16_t 	*context_predictor;
	mixer_t 	*mixer;

    uint32_t 	current;
    uint32_t 	context = 0;
    uint32_t 	next_context = 0;

    uint32_t	context_mask;
    uint32_t 	codex_bits = fqpack_ilog2(m_model->codex_size - 1) + 1;
    uint32_t	prev_bits = 0;
	int32_t 	char_bit_len;
	uint32_t 	context_bit;
	uint32_t 	run_bits;

    arithcoder_t coder;

    uint32_t run_length;

    context_mask = (1 << (m_model->order * codex_bits)) - 1;
    if ((m_model->order * codex_bits) >= 32) context_mask = UINT32_MAX;

    ac_initialize_encoder(&coder, m_output, m_len);

    /**
     * Save the parameters for the mixing model
     */
    for (size_t i = 0; i < 2; i++)
    {
    	for (size_t j = 0; j < 2; j++)
    	{
    		ac_store_halfword(&coder, m_model->char_bits_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, m_model->char_bits_param.threshhold[i][j]);
    		ac_store_halfword(&coder, m_model->char_val_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, m_model->char_val_param.threshhold[i][j]);
    		ac_store_halfword(&coder, m_model->run_bits_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, m_model->run_bits_param.threshhold[i][j]);
    		ac_store_halfword(&coder, m_model->run_len_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, m_model->run_len_param.threshhold[i][j]);
    	}
    	ac_store_halfword(&coder, m_model->char_bits_param.learning_rate[i]);
    	ac_store_halfword(&coder, m_model->char_val_param.learning_rate[i]);
    	ac_store_halfword(&coder, m_model->run_bits_param.learning_rate[i]);
		ac_store_halfword(&coder, m_model->run_len_param.learning_rate[i]);
    }

    current = m_input[0];
    next_context = current;
    ac_store_byte(&coder, m_input[0]);

    for (size_t i = 1; i < m_len;)
    {

    	context = next_context;
		prev_bits = ((prev_bits << codex_bits) | fqpack_lt256[current]) & (FQPACK_MAX_CHAR_BITS - 1);

        current = m_input[i++];
        run_length = 1;
        while ( !(current) && (i < m_len) && (m_input[i] == current))
		{
        	 i++;
        	 run_length++;
		}

		next_context = ((context << codex_bits) | current) & context_mask;
		char_bit_len = fqpack_ilog2(current);

		model_2state_encode_bits(m_model->char_bits_mixer[prev_bits], &coder,
				m_model->character.bits.fixed,
				&m_model->character.bits.context[context << FQPACK_MAX_CHAR_BITS_SHIFT], &m_model->char_bits_param,
				char_bit_len, codex_bits);


		/**
		 * Now encode the actual bit value for the mapped character
		 */

		context_predictor = &m_model->character.val[char_bit_len].context[context << FQPACK_MAX_CONTEXT_SHIFT];
		fixed_predictor = m_model->character.val[char_bit_len].fixed;
		mixer = &m_model->char_val_mixer[char_bit_len];

		context_bit = 1;
		for (int_fast32_t bit = char_bit_len; bit >= 0; --bit)
		{
			uint32_t adjusted_context_bit = context_bit - 1;
			if (current & (1 << bit))
			{
				probability = mixer_mix2_adapt_bit1(mixer,
						fixed_predictor[adjusted_context_bit],
						context_predictor[adjusted_context_bit], &m_model->char_val_param);

				MIXER2_ADJUST_BIT1(
						&fixed_predictor[adjusted_context_bit],
						&context_predictor[adjusted_context_bit], m_model->char_val_param);

				ac_encode_bit1(&coder, probability);

				context_bit += context_bit + 1;
			}
			else
			{
				probability = mixer_mix2_adapt_bit0(mixer,
						fixed_predictor[adjusted_context_bit],
						context_predictor[adjusted_context_bit], &m_model->char_val_param);

				MIXER2_ADJUST_BIT0(
						&fixed_predictor[adjusted_context_bit],
						&context_predictor[adjusted_context_bit], m_model->char_val_param);

				ac_encode_bit0(&coder, probability);

				context_bit += context_bit;
			}
		}


		/**
		 * Run length (RLE0) encoding
		 */

		if (!(current))
		{
	        run_bits = fqpack_ilog2(run_length);
			/**
			 * Encode the number of bits in the run value
			 */
			context_predictor = &m_model->run.bits.context[context << FQPACK_MAX_RUN_BITS_SHIFT];
			fixed_predictor = m_model->run.bits.fixed;
			mixer = m_model->run_bits_mixer;
			for (int32_t bit = 0; bit < run_bits; bit++)
			{
				probability = mixer_mix2_adapt_bit1(mixer,
						*fixed_predictor,
						*context_predictor, &m_model->run_bits_param);

				MIXER2_ADJUST_BIT1(
						fixed_predictor,
						context_predictor, m_model->run_bits_param);

				ac_encode_bit1(&coder, probability);

				context_predictor++;
				fixed_predictor++;
				mixer++;
			}

			probability = mixer_mix2_adapt_bit0(mixer, *fixed_predictor, *context_predictor, &m_model->run_bits_param);
			MIXER2_ADJUST_BIT0(fixed_predictor,	context_predictor, m_model->run_bits_param);
			ac_encode_bit0(&coder, probability);

			/**
			 * Encode the actual run value
			 */

			context_predictor = &m_model->run.length[run_bits].context[context << FQPACK_MAX_RUN_BITS_SHIFT];
			fixed_predictor = m_model->run.length[run_bits].fixed;
			mixer = &m_model->run_length_mixer[run_bits];
			if (run_bits)
			{
				run_bits--;
				context_predictor++;
				fixed_predictor++;
			}
			for (int32_t bit = run_bits; bit >= 0; --bit)
			{
				if (run_length & (1 << bit))
				{
					probability = mixer_mix2_adapt_bit1(mixer,
							*fixed_predictor, *context_predictor, &m_model->run_len_param);

					MIXER2_ADJUST_BIT1(fixed_predictor, context_predictor, m_model->run_len_param);
					ac_encode_bit1(&coder, probability);

					context_predictor++;
					fixed_predictor++;
				}
				else
				{
					probability = mixer_mix2_adapt_bit0(mixer,
							*fixed_predictor, *context_predictor, &m_model->run_len_param);

					MIXER2_ADJUST_BIT0(fixed_predictor, context_predictor, m_model->run_len_param);
					ac_encode_bit0(&coder, probability);

					context_predictor++;
					fixed_predictor++;
				}
			}

	        for (uint32_t run = 1; run < run_length; run++)
	        	context = ((context << codex_bits) | current) & context_mask;
		}

    }

    return ac_finialize_encoder(&coder);
}

static inline void model_compress_large_sequence(sequence_model_t *model, arithcoder_t *coder,
		uint8_t *input, uint8_t *qual_input, size_t i, size_t line_len, int32_t line_bits, uint32_t context_mask)
{
	uint8_t		current;
	uint32_t	context = UINT32_MAX & context_mask;
	uint32_t	o1_context = 0b11;
	uint32_t	prev_bits = 0;
	uint32_t	qual_current = qual_input[i];
	uint8_t		rank_bits;

	int16_t		probability;
    int16_t		*o1_predictor;
    int16_t 	*context_predictor;
    int16_t 	*qual_predictor;
    mixer_t 	*mixer;


    for (size_t pos = 0; pos < line_len; pos++)
    {
		current = input[i++];
		rank_bits = fqpack_ilog2(current);

		qual_current <<= (FQPACK_MAX_QUAL_CODEX_BITS - 1);
		qual_current |= qual_input[(pos+1)<line_len?i:i - 1];
		qual_current &= (FQPACK_MAX_QUAL_CONTEXT - 1);

		/**
		 * Encode the number of bits in the rank.  If the line only contains G,T,C,A, then we output either a 0 or a 1
		 * here.  If the line contains non-standard characters, we do unary encoding of the number of bits.
		 */

		model_3state_encode_bits(model->char_bits_mixer[prev_bits], coder,
				&model->character.bits.o1[o1_context * FQPACK_MAX_SEQ_CONTEXT_BITS],
				&model->character.bits.context[context * FQPACK_MAX_SEQ_CONTEXT_BITS],
				&model->character.bits.qual[qual_current * FQPACK_MAX_SEQ_CONTEXT_BITS],
				&model->char_bits_param, rank_bits, line_bits);
		/**
		 * Now encode the actual bit value for the mapped character.  This will only encode the number of bits specified
		 * from the above encoding
		 */
		context_predictor = &model->character.val[rank_bits].context[context];
		o1_predictor = &model->character.val[rank_bits].o1[o1_context];
		qual_predictor = &model->character.val[rank_bits].qual[qual_current];
		mixer = &model->char_val_mixer[rank_bits];

		if (rank_bits)
		{
			rank_bits--;
		}

		for (int_fast32_t bit = rank_bits; bit >= 0; --bit)
		{
			if (current & (1 << bit))
			{
				probability = mixer_mix3_adapt_bit1(mixer,
					*o1_predictor,
					*context_predictor,
					*qual_predictor, &model->char_val_param);

				ac_encode_bit1(coder, probability);

				MIXER3_ADJUST_BIT1(
					o1_predictor,
					context_predictor,
					qual_predictor, model->char_val_param);
			}
			else
			{
				probability = mixer_mix3_adapt_bit0(mixer,
						*o1_predictor,
						*context_predictor,
						*qual_predictor, &model->char_val_param);

				ac_encode_bit0(coder, probability);

				MIXER3_ADJUST_BIT0(
					o1_predictor,
					context_predictor,
					qual_predictor, model->char_val_param);
			}
		}

		o1_context = current & 0b11;
		context = ((context << 2) | (o1_context)) & context_mask;
		prev_bits = ((prev_bits << 1) | fqpack_lt256[current]) & (FQPACK_MAX_SEQ_MIXER_BITS - 1);

    }
}


ssize_t model_compress_sequence(fqpack_stream_t *m_stream, size_t m_len)
{
    uint8_t 		*input = m_stream->bb_seq->byte_content;
    uint8_t 		*output = m_stream->buffer;
    uint8_t			*qual_input = m_stream->bb_qual->byte_content;

    sequence_model_t 	*model = m_stream->seq_model;

    uint32_t		read_count = 0;
    uint32_t 		line_len = line_len = m_stream->length_vector->word_content[0];

    uint32_t		context_mask;

    uint32_t 		codex_bits = fqpack_ilog2(model->codex_size - 1) + 1;
	int32_t 		line_bits;

    bytemap_t		map;
    arithcoder_t 	coder;

    if ((model->order * 2) >= 32) context_mask = UINT32_MAX;
    else context_mask = (1 << (model->order * 2)) - 1;

    ac_initialize_encoder(&coder, output, m_len);

    if (m_stream->large_codex->bool_content[0]) line_bits = codex_bits;
    else line_bits = 2;

    /**
     * Map our input stream onto the frequency dist
     */
	bytemap_invert(&map, &m_stream->seq_map);
	for (size_t i = 0; i < m_len; i++)
		input[i] = map.map[input[i]];

    /**
     * Save the map-ordered codex at the start of our sequence output
     */
    for (size_t i = 0; i < model->codex_size; i++)
    	ac_store_byte(&coder, m_stream->seq_map.map[i]);
    ac_store_byte(&coder, m_stream->seq_map.map[model->codex_size - 1]);

    for (size_t i = 0; i < 3; i++)
    {
    	for (size_t j = 0; j < 2; j++)
    	{
    		ac_store_halfword(&coder, model->char_bits_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, model->char_bits_param.threshhold[i][j]);
    		ac_store_halfword(&coder, model->char_val_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, model->char_val_param.threshhold[i][j]);
    	}
    	ac_store_halfword(&coder, model->char_bits_param.learning_rate[i]);
    	ac_store_halfword(&coder, model->char_val_param.learning_rate[i]);
    }

    for (size_t i = 0; i < m_len; i += line_len)
    {
		model_compress_large_sequence(model, &coder, input, qual_input, i, line_len, line_bits, context_mask);

		line_len += m_stream->length_vector->word_content[++read_count];

		if (m_stream->large_codex->bool_content[read_count])
			line_bits = codex_bits;
		else line_bits = 2;

    }
    return ac_finialize_encoder(&coder);
}

ssize_t model_compress_quality(fqpack_stream_t *m_stream, size_t m_len)
{
    uint8_t 		*input = m_stream->bb_qual->byte_content;
    uint8_t 		*output = m_stream->buffer;
    quality_model_t *model = m_stream->qual_model;

    uint8_t			max_prev = 0;
    uint8_t			prev[3] = {0};
    uint8_t 		current;
    uint32_t 		context = 0;
    uint32_t		bit_mixer_state;

    int16_t			probability;
    int16_t			*o0_predictor;
    int16_t 		*context_predictor;
    int16_t 		*pos_predictor;
    mixer_t 		*mixer;

	uint32_t 		rank_bits;
	uint32_t 		context_bit;

    uint32_t		read_count = 0;
    uint32_t		read_pos = 0;
    uint32_t		context_pos = 0;
    uint32_t		line_len = m_stream->length_vector->word_content[0];

    uint8_t 		codex_bits = fqpack_ilog2(model->codex_size - 1) + 1;
    uint32_t 		context_mask;

    arithcoder_t 	coder;

    context_mask = (1 << (model->order * codex_bits)) - 1;
    if ((model->order * codex_bits) >= 32) context_mask = UINT32_MAX;

    ac_initialize_encoder(&coder, output, m_len);

    /**
     * Save the codex at the start of our quality output
     */
    for (size_t i = 0; i < model->codex_size; i++)
    	ac_store_byte(&coder, m_stream->qual_map.map[i]);
    ac_store_byte(&coder, m_stream->qual_map.map[model->codex_size - 1]);

    /**
     * Save the parameters for the mixing model
     */
    for (size_t i = 0; i < 3; i++)
    {
    	for (size_t j = 0; j < 2; j++)
    	{
    		ac_store_halfword(&coder, model->char_bits_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, model->char_bits_param.threshhold[i][j]);
    		ac_store_halfword(&coder, model->char_val_param.adaptation_rate[i][j]);
    		ac_store_halfword(&coder, model->char_val_param.threshhold[i][j]);
    	}
    	ac_store_halfword(&coder, model->char_bits_param.learning_rate[i]);
    	ac_store_halfword(&coder, model->char_val_param.learning_rate[i]);
    }

    current = *input;
    context = UINT32_MAX & context_mask;
    ac_store_byte(&coder, current);


    for (size_t i = 1; i < m_len; )
    {
    	uint8_t max_index = (m_stream->qual_map.map[prev[1]] > m_stream->qual_map.map[prev[0]]) ? 1 : 0;

    	if (m_stream->qual_map.map[prev[2]] > m_stream->qual_map.map[prev[max_index]])
    		max_prev = prev[2];
    	else
    		max_prev = prev[max_index];

    	prev[2] = prev[1]; prev[1] = prev[0]; prev[0] = current;
        context = ((max_prev << (2 * codex_bits)) | prev[1] << codex_bits | current) & context_mask;

		if (++read_pos >= line_len)
		{
			line_len += m_stream->length_vector->word_content[++read_count];
			read_pos = 0;
		    context = UINT32_MAX & context_mask;
		    prev[2] = prev[1] = prev[0] = UINT8_MAX & ((1 << codex_bits) - 1);
		}
		context_pos = read_pos & (FQPACK_POS_QUANTA - 1);
        bit_mixer_state = context & (FQPACK_QUAL_BIT_MIXER_LEN - 1);

        current = input[i++];

		rank_bits = fqpack_ilog2(current);

		model_3state_encode_bits(model->char_bits_mixer[bit_mixer_state],
				&coder, &model->character.bits.o0[0],
				&model->character.bits.context[context * codex_bits],
				&model->character.bits.pos[context_pos * codex_bits],
				&model->char_bits_param,
				rank_bits, codex_bits);

		/**
		 * Now encode the actual bit value for the mapped character
		 */

		context_predictor = &model->character.val[rank_bits].context[context * model->codex_size];
		o0_predictor = &model->character.val[rank_bits].o0[0];
		pos_predictor = &model->character.val[rank_bits].pos[context_pos * model->codex_size];
		mixer = &model->char_val_mixer[rank_bits];

		context_bit = 1;
		if (rank_bits)
		{
			rank_bits--;
		}
		for (int32_t bit = rank_bits; bit >= 0; --bit)
		{
			uint32_t adjusted_context_bit = context_bit - 1;
			context_bit <<= 1;

			if (current & (1 << bit))
			{
				context_bit++;

				probability = mixer_mix3_adapt_bit1(mixer,
						o0_predictor[adjusted_context_bit],
						context_predictor[adjusted_context_bit],
						pos_predictor[adjusted_context_bit],
						&model->char_val_param);

				ac_encode_bit1(&coder, probability);

				MIXER3_ADJUST_BIT1(
						&o0_predictor[adjusted_context_bit],
						&context_predictor[adjusted_context_bit],
						&pos_predictor[adjusted_context_bit], model->char_val_param);
			}
			else
			{
				probability = mixer_mix3_adapt_bit0(mixer,
						o0_predictor[adjusted_context_bit],
						context_predictor[adjusted_context_bit],
						pos_predictor[adjusted_context_bit],
						&model->char_val_param);

				ac_encode_bit0(&coder, probability);

				MIXER3_ADJUST_BIT0(
						&o0_predictor[adjusted_context_bit],
						&context_predictor[adjusted_context_bit],
						&pos_predictor[adjusted_context_bit], model->char_val_param);
			}
		}
    }

    return ac_finialize_encoder(&coder);
}

ssize_t model_uncompress_rle(uint8_t *m_input, uint8_t *m_output, rle_model_t *m_model, size_t m_len)
{
	int16_t *fixed_predictor;
	int16_t *context_predictor;
	mixer_t *mixer;

    uint32_t current;
    uint32_t context = 0;
    uint32_t next_context = 0;

    uint32_t	context_mask;
    uint32_t 	codex_bits = fqpack_ilog2(m_model->codex_size - 1) + 1;
    uint32_t	prev_bits = 0;
	int32_t 	rank_bits;
	uint32_t 	context_bit;
	uint32_t 	run_bits;

    uint32_t run_length;

    arithcoder_t coder;

    context_mask = (1 << (m_model->order * codex_bits)) - 1;
    if ((m_model->order * codex_bits) >= 32) context_mask = UINT32_MAX;

    ac_initialize_decoder(&coder, m_input);

    /**
     * Save the parameters for the mixing model
     */
    for (size_t i = 0; i < 2; i++)
    {
    	for (size_t j = 0; j < 2; j++)
    	{
    		m_model->char_bits_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		m_model->char_bits_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    		m_model->char_val_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		m_model->char_val_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    		m_model->run_bits_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		m_model->run_bits_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    		m_model->run_len_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		m_model->run_len_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    	}
    	m_model->char_bits_param.learning_rate[i] = ac_unstore_halfword(&coder);
    	m_model->char_val_param.learning_rate[i] = ac_unstore_halfword(&coder);
    	m_model->run_bits_param.learning_rate[i] = ac_unstore_halfword(&coder);
		m_model->run_len_param.learning_rate[i] = ac_unstore_halfword(&coder);
    }

    current = ac_unstore_byte(&coder);
    *m_output++ = current;
    next_context = current;

    for (size_t i = 1; i < m_len; i+=run_length)
    {

    	context = next_context;
		prev_bits = ((prev_bits << codex_bits) | fqpack_lt256[current]) & (FQPACK_MAX_CHAR_BITS - 1);

		fixed_predictor = m_model->character.bits.fixed;
		context_predictor = &m_model->character.bits.context[context << FQPACK_MAX_CHAR_BITS_SHIFT];
		mixer = m_model->char_bits_mixer[prev_bits];

		rank_bits = 0;
		while (rank_bits < codex_bits - 1)
		{
			if (ac_decode_bit(&coder, mixer_mix2(mixer, *fixed_predictor, *context_predictor)))
			{
				MIXER2_ADJUST_BIT1(fixed_predictor, context_predictor, m_model->char_bits_param);
				mixer_update_weight1(mixer, &m_model->char_bits_param);

				context_predictor++;
				fixed_predictor++;
				mixer++;
				rank_bits++;
			}
			else
			{
				MIXER2_ADJUST_BIT0(fixed_predictor, context_predictor, m_model->char_bits_param);

				mixer_update_weight0(mixer, &m_model->char_bits_param);
				break;
			}
		}

		context_predictor = &m_model->character.val[rank_bits].context[context << FQPACK_MAX_CONTEXT_SHIFT];
		fixed_predictor = m_model->character.val[rank_bits].fixed;
		mixer = &m_model->char_val_mixer[rank_bits];

		current = 0;
		context_bit = 1;
		for (int_fast32_t bit = rank_bits; bit >= 0; --bit)
		{
			uint32_t adjusted_context_bit = context_bit - 1;
			if (ac_decode_bit(&coder, mixer_mix2(mixer, fixed_predictor[adjusted_context_bit], context_predictor[adjusted_context_bit])))
			{
				MIXER2_ADJUST_BIT1(&fixed_predictor[adjusted_context_bit], &context_predictor[adjusted_context_bit], m_model->char_val_param);
				mixer_update_weight1(mixer, &m_model->char_val_param);

				context_bit += context_bit + 1;
				current += current + 1;
			}
			else
			{
				MIXER2_ADJUST_BIT0(&fixed_predictor[adjusted_context_bit], &context_predictor[adjusted_context_bit], m_model->char_val_param);
				mixer_update_weight0(mixer, &m_model->char_val_param);

				context_bit += context_bit;
				current += current;
			}
		}


		next_context = ((next_context << codex_bits) | current) & context_mask;
		run_length = 1;
		run_bits = 0;
		context_bit = 1;

		if (!(current))
		{
			fixed_predictor = m_model->run.bits.fixed;
			context_predictor = &m_model->run.bits.context[context << FQPACK_MAX_RUN_BITS_SHIFT];
			mixer = m_model->run_bits_mixer;


			while (ac_decode_bit(&coder, mixer_mix2(mixer, *fixed_predictor, *context_predictor)))
			{
				MIXER2_ADJUST_BIT1(fixed_predictor, context_predictor, m_model->run_bits_param);

				mixer_update_weight1(mixer, &m_model->run_bits_param);

				context_predictor++;
				fixed_predictor++;
				mixer++;
				run_bits++;
			}

			MIXER2_ADJUST_BIT0(fixed_predictor, context_predictor, m_model->run_bits_param);

			mixer_update_weight0(mixer, &m_model->run_bits_param);


			context_predictor = &m_model->run.length[run_bits].context[context << FQPACK_MAX_RUN_BITS_SHIFT];
			fixed_predictor = m_model->run.length[run_bits].fixed;
			mixer = &m_model->run_length_mixer[run_bits];

			context_bit = 0;
			run_length = 0;
			if (run_bits)
			{
				run_length = 1<<run_bits;
				run_bits--;
				fixed_predictor++;
				context_predictor++;
			}
			for (int_fast32_t bit = run_bits; bit >= 0; --bit)
			{
				if (ac_decode_bit(&coder, mixer_mix2(mixer, fixed_predictor[context_bit], context_predictor[context_bit])))
				{
					MIXER2_ADJUST_BIT1(fixed_predictor, context_predictor, m_model->run_len_param);
					mixer_update_weight1(mixer, &m_model->run_len_param);
					run_length |= (1<<bit);
				}
				else
				{
					MIXER2_ADJUST_BIT0(fixed_predictor, context_predictor, m_model->run_len_param);
					mixer_update_weight0(mixer, &m_model->run_len_param);
				}
				fixed_predictor++;
				context_predictor++;
			}
		}


		for (uint32_t pos = 0; pos < run_length; pos++) *m_output++ = current;
    }
    return coder.in_buf - m_input;

}

ssize_t model_uncompress_sequence(fqpack_stream_t *m_stream)
{
	size_t 			len = m_stream->header->sequence.uncompressed_len;

    uint8_t 		*input = m_stream->buffer;
    uint8_t 		*output = m_stream->bb_seq->byte_content;
    uint8_t			*qual_input = m_stream->bb_qual->byte_content;
    sequence_model_t *model;

    uint32_t		qual_current;
    uint32_t 		current;
    uint32_t 		context = 0;
	uint32_t		o1_context = 0b11;

    uint32_t		read_count = 0;
    uint32_t		read_pos = 0;
    uint32_t 		line_len = line_len = m_stream->length_vector->word_content[0];

    uint32_t		full_shift = m_stream->header->sequence.context_order * 2;
    uint32_t		context_mask;

    uint32_t 		codex_bits;
	int32_t 		rank_bits;
	int32_t 		prev_bits = 0;
	int32_t 		line_bits;

    arithcoder_t 	coder;

	int16_t 		*o1_predictor;
	int16_t 		*context_predictor;
    int16_t 		*qual_predictor;
	mixer_t 		*mixer;

	bytemap_t		map = {{0}, {0}};
	size_t 			codex_size;

    context_mask = (1 << (full_shift)) - 1;
    if ((full_shift) >= 32) context_mask = UINT32_MAX;

    ac_initialize_decoder(&coder, input);

    bb_reset(m_stream->bb_seq);
    bb_ensure_len(m_stream->bb_seq, len);

    /**
     * Read the codex back in for decoding
     */
    map.map[0] = ac_unstore_byte(&coder);
    map.freq[0] = 256;
    for (codex_size = 1; codex_size <= 256; codex_size++)
    {
    	map.map[codex_size] = ac_unstore_byte(&coder);
    	if (map.map[codex_size] == map.map[codex_size-1]) break;
    	map.freq[codex_size] = (256 - codex_size);
    }
	m_stream->seq_model = model_initialize_sequence(m_stream->seq_model, codex_size, m_stream->header->sequence.context_order, m_stream->header->continuing);
	model = m_stream->seq_model;

	codex_bits = fqpack_ilog2(model->codex_size - 1) + 1;
    if (m_stream->large_codex->bool_content[0]) line_bits = codex_bits;
    else line_bits = 2;

    /**
     * Restore the saved mixing parameters for this sequence set
     */
    for (size_t i = 0; i < 3; i++)
    {
    	for (size_t j = 0; j < 2; j++)
    	{
    		model->char_bits_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		model->char_bits_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    		model->char_val_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		model->char_val_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    	}
    	model->char_bits_param.learning_rate[i] = ac_unstore_halfword(&coder);
    	model->char_val_param.learning_rate[i] = ac_unstore_halfword(&coder);
    }

	context = UINT32_MAX & context_mask;
	qual_current = qual_input[0];

    for (size_t i = 0; i < len; i++)
    {
		current = 0;
		read_pos++;

		qual_current <<= (FQPACK_MAX_QUAL_CODEX_BITS - 1);
		qual_current |= qual_input[read_pos<line_len?i+1:i];
		qual_current &= (FQPACK_MAX_QUAL_CONTEXT - 1);

		o1_predictor = &model->character.bits.o1[o1_context * FQPACK_MAX_SEQ_CONTEXT_BITS];
		context_predictor = &model->character.bits.context[context * FQPACK_MAX_SEQ_CONTEXT_BITS];
		qual_predictor = &model->character.bits.qual[qual_current * FQPACK_MAX_SEQ_CONTEXT_BITS];
		mixer = model->char_bits_mixer[prev_bits];

		rank_bits = 0;
		while (rank_bits < line_bits - 1)
		{
			if (ac_decode_bit(&coder, mixer_mix3(mixer, *o1_predictor, *context_predictor, *qual_predictor)))
			{
				MIXER3_ADJUST_BIT1(o1_predictor, context_predictor, qual_predictor, model->char_bits_param);
				mixer_update_weight1(mixer, &model->char_bits_param);

				context_predictor++;
				o1_predictor++;
				qual_predictor++;
				mixer++;

				rank_bits++;
			}
			else
			{
				MIXER3_ADJUST_BIT0(o1_predictor, context_predictor, qual_predictor, model->char_bits_param);
				mixer_update_weight0(mixer, &model->char_bits_param);
				break;
			}
		}

		context_predictor = &model->character.val[rank_bits].context[context];
		o1_predictor = &model->character.val[rank_bits].o1[o1_context];
		qual_predictor = &model->character.val[rank_bits].qual[qual_current];
		mixer = &model->char_val_mixer[rank_bits];

		/**
		 * If rank_bits > 0, then we know that the top-most bit is always set as that defines the bit count,
		 * so we set that in our current val.  If rank_bits = 0, then the top-most bit can be either 0 or 1
		 * so we proceed as normal.
		 */
		if (rank_bits)
		{
			current = rank_bits << 1;
			rank_bits--;
		}
		for (int_fast32_t bit = rank_bits; bit >= 0; --bit)
		{
			if (ac_decode_bit(&coder,
					mixer_mix3(mixer,
							*o1_predictor,
							*context_predictor,
							*qual_predictor)))
			{

				MIXER3_ADJUST_BIT1(o1_predictor,
						context_predictor,
						qual_predictor, model->char_val_param);

				mixer_update_weight1(mixer, &model->char_val_param);
				current |= (1 << bit);
			}
			else
			{
				MIXER3_ADJUST_BIT0(o1_predictor,
						context_predictor,
						qual_predictor, model->char_val_param);

				mixer_update_weight0(mixer, &model->char_val_param);
			}
		}

		*output++ = map.map[current];
		o1_context = current & 0b11;
		context = ((context << 2) | o1_context) & context_mask;
		prev_bits = ((prev_bits << 1) | fqpack_lt256[current]) & (FQPACK_MAX_SEQ_MIXER_BITS - 1);

		if (read_pos == line_len)
		{
			line_len += m_stream->length_vector->word_content[++read_count];
			read_pos = 0;
			context = UINT32_MAX & context_mask;
			o1_context = 0b11;
			prev_bits = 0;
			qual_current = qual_input[i + 1];
		    if (m_stream->large_codex->bool_content[read_count]) line_bits = codex_bits;
		    else line_bits = 2;
		}
    }
    return coder.in_buf - input;
}

ssize_t model_uncompress_quality(fqpack_stream_t *m_stream)
{
	size_t 			len = m_stream->header->quality.uncompressed_len;
    uint8_t 		*input = m_stream->buffer;
    uint8_t 		*output = m_stream->bb_qual->byte_content;
    quality_model_t *model;

    uint8_t			max_prev = 0;
    uint8_t			prev[3] = {0};
    uint32_t 		current;
    uint32_t 		context = 0;

    uint32_t		read_count = 0;
    uint32_t		read_pos = 0;
    uint32_t		context_pos = 0;
    uint32_t		bit_mixer_state;

    uint32_t		codex_size;
    uint8_t 		codex_bits;
    uint32_t 		context_mask;

    int16_t 		*o0_predictor;
	int16_t 		*context_predictor;
	int16_t 		*pos_predictor;
	mixer_t 		*mixer;

	int32_t 		bit_count = 0;
	int32_t 		context_bit = 1;

    uint32_t 		line_len = m_stream->length_vector->word_content[0];
    arithcoder_t 	coder;

    ac_initialize_decoder(&coder, input);

    bb_reset(m_stream->bb_qual);
    bb_ensure_len(m_stream->bb_qual, len);

    /**
     * Read the codex back in for decoding.  After we have the codex, we can re-initialize
     * the model and setup our masking and shift parameters.  We don't know the true freq
     * of the symbols but we do know their relative order, so to maintain that information, we
     * assign a fake, ordered count to the frequency value as well
     */
    memset(&m_stream->qual_map, 0, sizeof(bytemap_t));
    m_stream->qual_map.map[0] = ac_unstore_byte(&coder);
    m_stream->qual_map.freq[0] = 256;
    for (codex_size = 1; codex_size <= 256; codex_size++)
    {
    	m_stream->qual_map.map[codex_size] = ac_unstore_byte(&coder);
    	if (m_stream->qual_map.map[codex_size] == m_stream->qual_map.map[codex_size-1]) break;
    	m_stream->qual_map.freq[codex_size] = (256 - codex_size);
    }
    m_stream->qual_model = model_initialize_quality(m_stream->qual_model, codex_size, m_stream->header->quality.context_order, m_stream->header->continuing);
    model = m_stream->qual_model;

    codex_bits = fqpack_ilog2(model->codex_size - 1) + 1;
    context_mask = (1 << (model->order * codex_bits)) - 1;
    if ((model->order * codex_bits) >= 32) context_mask = UINT32_MAX;

    /**
     * Restore the saved mixing parameters for this quality set
     */
    for (size_t i = 0; i < 3; i++)
    {
    	for (size_t j = 0; j < 2; j++)
    	{
    		model->char_bits_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		model->char_bits_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    		model->char_val_param.adaptation_rate[i][j] = ac_unstore_halfword(&coder);
    		model->char_val_param.threshhold[i][j] = ac_unstore_halfword(&coder);
    	}
    	model->char_bits_param.learning_rate[i] = ac_unstore_halfword(&coder);
    	model->char_val_param.learning_rate[i] = ac_unstore_halfword(&coder);
    }

    current = ac_unstore_byte(&coder);
    context = UINT32_MAX & context_mask;
    *output++ = current;

    for (size_t i = 1; i < len; i++)
    {
    	uint8_t max_index = (m_stream->qual_map.map[prev[1]] > m_stream->qual_map.map[prev[0]]) ? 1 : 0;

    	if (m_stream->qual_map.map[prev[2]] > m_stream->qual_map.map[prev[max_index]])
    		max_prev = prev[2];
    	else
    		max_prev = prev[max_index];

    	prev[2] = prev[1]; prev[1] = prev[0]; prev[0] = current;
        context = ((max_prev << (2 * codex_bits)) | prev[1] << codex_bits | current) & context_mask;

		if (++read_pos >= line_len)
		{
			line_len += m_stream->length_vector->word_content[++read_count];
			read_pos = 0;
		    context = UINT32_MAX & context_mask;
		    prev[2] = prev[1] = prev[0] = UINT8_MAX & ((1 << codex_bits) - 1);
		}
		context_pos = read_pos & (FQPACK_POS_QUANTA - 1);
        bit_mixer_state = context & (FQPACK_QUAL_BIT_MIXER_LEN - 1);

		o0_predictor = &model->character.bits.o0[0];
		pos_predictor = &model->character.bits.pos[context_pos * codex_bits];
		context_predictor = &model->character.bits.context[context * codex_bits];
		mixer = model->char_bits_mixer[bit_mixer_state];

		for (bit_count = 0; bit_count < codex_bits - 1; bit_count++)
		{
			if (ac_decode_bit(&coder, mixer_mix3(mixer,
					*o0_predictor, *context_predictor, *pos_predictor)))
			{
				MIXER3_ADJUST_BIT1(o0_predictor, context_predictor, pos_predictor, model->char_bits_param);
				mixer_update_weight1(mixer, &model->char_bits_param);

				context_predictor++;
				pos_predictor++;
				o0_predictor++;
				mixer++;
			}
			else
			{
				MIXER3_ADJUST_BIT0(o0_predictor, context_predictor, pos_predictor, model->char_bits_param);
				mixer_update_weight0(mixer, &model->char_bits_param);
				break;
			}
		}

		pos_predictor = &model->character.val[bit_count].pos[context_pos * model->codex_size];
		context_predictor = &model->character.val[bit_count].context[context * model->codex_size];
		o0_predictor = &model->character.val[bit_count].o0[0];
		mixer = &model->char_val_mixer[bit_count];

		context_bit = 1;
		current = 0;

		/**
		 * If bit_count > 0, then we know that the top-most bit is always set as that defines the bit count,
		 * so we set that in our current val and update the context_bit to reflect this (otherwise we mix our
		 * context with the rank_bit = 0 case).  If bit_count = 0, then the top-most bit can be either 0 or 1
		 * so we proceed as normal.
		 */
		if (bit_count)
		{
			current = 1 << bit_count;
			bit_count--;
		}
		for (int_fast32_t bit = bit_count; bit >= 0; --bit)
		{
			uint32_t adjusted_context_bit = context_bit - 1;
			context_bit <<= 1;
			if (ac_decode_bit(&coder,
					mixer_mix3(mixer, o0_predictor[adjusted_context_bit],
									context_predictor[adjusted_context_bit],
									pos_predictor[adjusted_context_bit])))
			{
				MIXER3_ADJUST_BIT1(&o0_predictor[adjusted_context_bit],
						&context_predictor[adjusted_context_bit],
						&pos_predictor[adjusted_context_bit], model->char_val_param);
				mixer_update_weight1(mixer, &model->char_val_param);
				context_bit++;
				current |= (1 << bit);
			}
			else
			{
				MIXER3_ADJUST_BIT0(&o0_predictor[adjusted_context_bit],
						&context_predictor[adjusted_context_bit],
						&pos_predictor[adjusted_context_bit], model->char_val_param);
				mixer_update_weight0(mixer, &model->char_val_param);
			}
		}

		*output++ = current;
    }
    return coder.in_buf - input;
}

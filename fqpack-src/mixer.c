/**
 * @file mixer.c
 *
 * @date Dec 22, 2011
 * @author seth
 *
 * Copyright (c) 2011, seth
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

#include <log_tables.h>

#include <mixer.h>

void mixer_init(mixer_t *m_mixer)
{
	m_mixer->weight[0] = 4096 << 5;
	m_mixer->weight[1] = 4096 << 5;
	m_mixer->weight[2] = 4096 << 2;
	m_mixer->weight[3] = 4096 << 2;
}

int16_t mixer_mix4_adapt_bit0(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2, const int16_t m_prob3,
		const model_params_t *m_param)
{
	int16_t stretched_prob0 = fqpack_stretch(m_prob0);
	int16_t stretched_prob1 = fqpack_stretch(m_prob1);
	int16_t stretched_prob2 = fqpack_stretch(m_prob2);
	int16_t stretched_prob3 = fqpack_stretch(m_prob3);

	int16_t probability = fqpack_squash(
			(	stretched_prob0 * m_mixer->weight[0] +
				stretched_prob1 * m_mixer->weight[1] +
				stretched_prob2 * m_mixer->weight[2] +
				stretched_prob2 * m_mixer->weight[3]) >> MIX4_LOG_SHIFT);
	int16_t eps = probability - 4095;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * eps * stretched_prob0) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * eps * stretched_prob1) >> MIX_WGT_SHIFT;
	m_mixer->weight[2] -= (m_param->learning_rate[2] * eps * stretched_prob2) >> MIX_WGT_SHIFT;
	m_mixer->weight[3] -= (m_param->learning_rate[3] * eps * stretched_prob3) >> MIX_WGT_SHIFT;

	return probability;
}

int16_t mixer_mix4_adapt_bit1(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2, const int16_t m_prob3,
		const model_params_t *m_param)
{
	int16_t stretched_prob0 = fqpack_stretch(m_prob0);
	int16_t stretched_prob1 = fqpack_stretch(m_prob1);
	int16_t stretched_prob2 = fqpack_stretch(m_prob2);
	int16_t stretched_prob3 = fqpack_stretch(m_prob3);

	int16_t probability = fqpack_squash(
			(	stretched_prob0 * m_mixer->weight[0] +
				stretched_prob1 * m_mixer->weight[1] +
				stretched_prob2 * m_mixer->weight[2] +
				stretched_prob2 * m_mixer->weight[3]) >> MIX4_LOG_SHIFT);
	int16_t eps = probability - 1;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * eps * stretched_prob0) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * eps * stretched_prob1) >> MIX_WGT_SHIFT;
	m_mixer->weight[2] -= (m_param->learning_rate[2] * eps * stretched_prob2) >> MIX_WGT_SHIFT;
	m_mixer->weight[3] -= (m_param->learning_rate[3] * eps * stretched_prob3) >> MIX_WGT_SHIFT;

	return probability;
}

int16_t mixer_mix3_adapt_bit0(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2,
		const model_params_t *m_param)
{
	int16_t stretched_prob0 = fqpack_stretch(m_prob0);
	int16_t stretched_prob1 = fqpack_stretch(m_prob1);
	int16_t stretched_prob2 = fqpack_stretch(m_prob2);

	int16_t probability = fqpack_squash(
			(	stretched_prob0 * m_mixer->weight[0] +
				stretched_prob1 * m_mixer->weight[1] +
				stretched_prob2 * m_mixer->weight[2]) >> MIX3_LOG_SHIFT);
	int16_t eps = probability - 4095;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * eps * stretched_prob0) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * eps * stretched_prob1) >> MIX_WGT_SHIFT;
	m_mixer->weight[2] -= (m_param->learning_rate[2] * eps * stretched_prob2) >> MIX_WGT_SHIFT;

	return probability;
}

int16_t mixer_mix3_adapt_bit1(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2,
		const model_params_t *m_param)
{
	int16_t stretched_prob0 = fqpack_stretch(m_prob0);
	int16_t stretched_prob1 = fqpack_stretch(m_prob1);
	int16_t stretched_prob2 = fqpack_stretch(m_prob2);

	int16_t probability = fqpack_squash(
			(stretched_prob0 * m_mixer->weight[0] +
			 stretched_prob1 * m_mixer->weight[1] +
			 stretched_prob2 * m_mixer->weight[2]) >> MIX3_LOG_SHIFT);
	int16_t eps = probability - 1;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * eps * stretched_prob0) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * eps * stretched_prob1) >> MIX_WGT_SHIFT;
	m_mixer->weight[2] -= (m_param->learning_rate[2] * eps * stretched_prob2) >> MIX_WGT_SHIFT;

	return probability;
}


int16_t mixer_mix2_adapt_bit0(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1,
		const model_params_t *m_param)
{
	int16_t stretched_prob0 = fqpack_stretch(m_prob0);
	int16_t stretched_prob1 = fqpack_stretch(m_prob1);

	int16_t probability = fqpack_squash(
			(stretched_prob0 * m_mixer->weight[0] +
			 stretched_prob1 * m_mixer->weight[1]) >> MIX2_LOG_SHIFT);
	int16_t eps = probability - 4095;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * eps * stretched_prob0) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * eps * stretched_prob1) >> MIX_WGT_SHIFT;

	return probability;
}

int16_t mixer_mix2_adapt_bit1(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1,
		const model_params_t *m_param)
{
	int16_t stretched_prob0 = fqpack_stretch(m_prob0);
	int16_t stretched_prob1 = fqpack_stretch(m_prob1);

	int16_t probability = fqpack_squash(
			(stretched_prob0 * m_mixer->weight[0] +
			 stretched_prob1 * m_mixer->weight[1]) >> MIX2_LOG_SHIFT);
	int16_t eps = probability - 1;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * eps * stretched_prob0) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * eps * stretched_prob1) >> MIX_WGT_SHIFT;

	return probability;
}


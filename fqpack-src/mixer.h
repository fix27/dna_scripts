/**
 * @file mixer.h
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

#ifndef MIXER_H_
#define MIXER_H_

#include <util.h>
#include <log_tables.h>

#define MIX_LIN_SHIFT (13)
#define MIX4_LOG_SHIFT (20)
#define MIX3_LOG_SHIFT (18)
#define MIX2_LOG_SHIFT (16)
#define MIX_WGT_SHIFT (16)

typedef struct
{
	int16_t threshhold[3][2];
	int16_t adaptation_rate[3][2];
	int16_t learning_rate[3];
} model_params_t;

typedef struct
{
	int16_t	stretched_prob[4] FQPACK_ALIGN;
    int16_t	mix_prob FQPACK_ALIGN;
    int32_t weight[4] FQPACK_ALIGN;
} mixer_t;


void mixer_init(mixer_t *m_mixer);
int16_t mixer_mix4_adapt_bit0(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2, const int16_t m_prob3,
		const model_params_t *m_param) __attribute__ ((hot));
int16_t mixer_mix4_adapt_bit1(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2, const int16_t m_prob3,
		const model_params_t *m_param) __attribute__ ((hot));
int16_t mixer_mix3_adapt_bit0(mixer_t *m_mixer, const int16_t m_prob0,
		const int16_t m_prob1, const int16_t m_prob2,
		const model_params_t *m_param) __attribute__ ((hot));
int16_t mixer_mix3_adapt_bit1(mixer_t *m_mixer, const int16_t m_prob0,
		const int16_t m_prob1, const int16_t m_prob2,
		const model_params_t *m_param) __attribute__ ((hot));
int16_t mixer_mix2_adapt_bit0(mixer_t *m_mixer, const int16_t m_prob0,
		const int16_t m_prob1,
		const model_params_t *m_param) __attribute__ ((hot));
int16_t mixer_mix2_adapt_bit1(mixer_t *m_mixer, const int16_t m_prob0,
		const int16_t m_prob1,
		const model_params_t *m_param) __attribute__ ((hot));

/**
 * Log-domain mixing of 3 bit estimators
 * @param m_mixer Pointer to the mixer
 * @param m_prob0 Probability value (-4095, 4095)
 * @param m_prob1 Probability value (-4095, 4095)
 * @param m_prob2 Probability value (-4095, 4095)
 * @return Approximate log-sum of three weighted probabilities
 */
static inline int16_t mixer_mix3(mixer_t *m_mixer,
		const int16_t m_prob0, const int16_t m_prob1, const int16_t m_prob2)
{
	m_mixer->stretched_prob[0] = fqpack_stretch(m_prob0);
	m_mixer->stretched_prob[1] = fqpack_stretch(m_prob1);
	m_mixer->stretched_prob[2] = fqpack_stretch(m_prob2);

	return m_mixer->mix_prob = fqpack_squash(
			( m_mixer->stretched_prob[0] * m_mixer->weight[0]
			+ m_mixer->stretched_prob[1] * m_mixer->weight[1]
			+ m_mixer->stretched_prob[2] * m_mixer->weight[2]) >> MIX3_LOG_SHIFT);
}

/**
 * Log-domain mixing of 2 bit estimators
 * @param m_mixer Pointer to the mixer
 * @param m_prob0 Probability value (-4095, 4095)
 * @param m_prob1 Probability value (-4095, 4095)
 * @return Approximate log-sum of two weighted probabilities
 */
static inline int16_t mixer_mix2(mixer_t *m_mixer, const int16_t m_prob0, const int16_t m_prob1)
{
	m_mixer->stretched_prob[0] = fqpack_stretch(m_prob0);
	m_mixer->stretched_prob[1] = fqpack_stretch(m_prob1);

	return m_mixer->mix_prob = fqpack_squash(
			( m_mixer->stretched_prob[0] * m_mixer->weight[0]
			+ m_mixer->stretched_prob[1] * m_mixer->weight[1]) >> MIX2_LOG_SHIFT);
}

#define MIXER_ADJUST_BIT0(probability, threshold, adapt)							\
do {																				\
	*(probability) = *(probability) +												\
			(((4096 - *(probability) - (threshold)) * (adapt)) >> MIX_LIN_SHIFT);	\
}while (0)

#define MIXER_ADJUST_BIT1(probability, threshold, adapt)							\
do {																				\
	*(probability) = *(probability) -												\
			(((*(probability) - (threshold)) * (adapt)) >> MIX_LIN_SHIFT);			\
}while (0)

#define MIXER4_ADJUST_BIT0(prob0, prob1, prob2, prob3, param)								\
do {																				\
	MIXER_ADJUST_BIT0(prob0, param.threshhold[0][0], param.adaptation_rate[0][0]); 	\
	MIXER_ADJUST_BIT0(prob1, param.threshhold[1][0], param.adaptation_rate[1][0]); 	\
	MIXER_ADJUST_BIT0(prob2, param.threshhold[2][0], param.adaptation_rate[2][0]); 	\
	MIXER_ADJUST_BIT0(prob3, param.threshhold[3][0], param.adaptation_rate[3][0]); 	\
}while (0)

#define MIXER4_ADJUST_BIT1(prob0, prob1, prob2, prob3, param)								\
do {																				\
	MIXER_ADJUST_BIT1(prob0, param.threshhold[0][1], param.adaptation_rate[0][1]); 	\
	MIXER_ADJUST_BIT1(prob1, param.threshhold[1][1], param.adaptation_rate[1][1]); 	\
	MIXER_ADJUST_BIT1(prob2, param.threshhold[2][1], param.adaptation_rate[2][1]); 	\
	MIXER_ADJUST_BIT1(prob3, param.threshhold[3][1], param.adaptation_rate[3][1]); 	\
}while (0)

#define MIXER3_ADJUST_BIT0(prob0, prob1, prob2, param)								\
do {																				\
	MIXER_ADJUST_BIT0(prob0, param.threshhold[0][0], param.adaptation_rate[0][0]); 	\
	MIXER_ADJUST_BIT0(prob1, param.threshhold[1][0], param.adaptation_rate[1][0]); 	\
	MIXER_ADJUST_BIT0(prob2, param.threshhold[2][0], param.adaptation_rate[2][0]); 	\
}while (0)

#define MIXER3_ADJUST_BIT1(prob0, prob1, prob2, param)								\
do {																				\
	MIXER_ADJUST_BIT1(prob0, param.threshhold[0][1], param.adaptation_rate[0][1]); 	\
	MIXER_ADJUST_BIT1(prob1, param.threshhold[1][1], param.adaptation_rate[1][1]); 	\
	MIXER_ADJUST_BIT1(prob2, param.threshhold[2][1], param.adaptation_rate[2][1]); 	\
}while (0)

#define MIXER2_ADJUST_BIT0(prob0, prob1, param)										\
do {																				\
	MIXER_ADJUST_BIT0(prob0, param.threshhold[0][0], param.adaptation_rate[0][0]); 	\
	MIXER_ADJUST_BIT0(prob1, param.threshhold[1][0], param.adaptation_rate[1][0]); 	\
}while (0)

#define MIXER2_ADJUST_BIT1(prob0, prob1, param)										\
do {																				\
	MIXER_ADJUST_BIT1(prob0, param.threshhold[0][1], param.adaptation_rate[0][1]); 	\
	MIXER_ADJUST_BIT1(prob1, param.threshhold[1][1], param.adaptation_rate[1][1]); 	\
}while (0)

static inline void mixer_update_weight0(mixer_t *m_mixer, const model_params_t *m_param)
{
	const int32_t pred_err = m_mixer->mix_prob - 4095;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * pred_err * m_mixer->stretched_prob[0]) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * pred_err * m_mixer->stretched_prob[1]) >> MIX_WGT_SHIFT;
	m_mixer->weight[2] -= (m_param->learning_rate[2] * pred_err * m_mixer->stretched_prob[2]) >> MIX_WGT_SHIFT;
}

static inline void mixer_update_weight1(mixer_t *m_mixer, const model_params_t *m_param)
{
	const int32_t pred_err = m_mixer->mix_prob - 1;

	m_mixer->weight[0] -= (m_param->learning_rate[0] * pred_err * m_mixer->stretched_prob[0]) >> MIX_WGT_SHIFT;
	m_mixer->weight[1] -= (m_param->learning_rate[1] * pred_err * m_mixer->stretched_prob[1]) >> MIX_WGT_SHIFT;
	m_mixer->weight[2] -= (m_param->learning_rate[2] * pred_err * m_mixer->stretched_prob[2]) >> MIX_WGT_SHIFT;
}

#endif /* MIXER_H_ */

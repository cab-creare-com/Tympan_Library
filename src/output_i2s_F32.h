/* Audio Library for Teensy 3.X
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 *
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards.  Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
  
 /* 
 *  Extended by Chip Audette, OpenAudio, May 2019
 *  Converted to F32 and to variable audio block length
 *	The F32 conversion is under the MIT License.  Use at your own risk.
 */

#ifndef output_i2s_f32_h_
#define output_i2s_f32_h_

#include <Arduino.h>
#include <arm_math.h>
#include "AudioStream_F32.h"
//include "AudioStream.h"
#include "DMAChannel.h"
#include "input_i2s_F32.h" //for AudioInputI2S_F32


class AudioOutputI2S_F32 : public AudioStream_F32
{
//GUI: inputs:2, outputs:0  //this line used for automatic generation of GUI node
public:
	AudioOutputI2S_F32(void) : AudioStream_F32(2, inputQueueArray)	{ 
		audio_block_samples = MAX_AUDIO_BLOCK_SAMPLES_F32; //set size to default
		begin();
	} //uses default AUDIO_SAMPLE_RATE and BLOCK_SIZE_SAMPLES from AudioStream.h
	AudioOutputI2S_F32(const AudioSettings_F32 &settings) : AudioOutputI2S_F32(settings, true) {}
	AudioOutputI2S_F32(const AudioSettings_F32 &settings, bool flag_callBegin) : AudioStream_F32(2, inputQueueArray)
	{ 
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples; //set size to given value
		if (flag_callBegin) begin(); 	
	}
	
	AudioOutputI2S_F32(const AudioSettings_F32 &settings, uint32_t *tx_buff) : AudioOutputI2S_F32(settings, tx_buff, true) { }
	AudioOutputI2S_F32(const AudioSettings_F32 &settings, uint32_t *tx_buff, bool flag_callBegin) : AudioStream_F32(2, inputQueueArray) { 
		sample_rate_Hz = settings.sample_rate_Hz;
		audio_block_samples = settings.audio_block_samples;
		i2s_tx_buffer = tx_buff;
		if (flag_callBegin) begin();  
	}
	
	void update(void) override;
	void begin(void);
	void begin(bool);
	void sub_begin_i32(void);
	void sub_begin_i16(void);
	friend class AudioInputI2S_F32;

	#if defined(__IMXRT1062__)
	friend class AudioInputI2SBase_F32;
	friend class AudioInputI2S_F32;
	friend class AudioInputI2SQuad_F32;
	friend class AudioInputI2SHex_F32;
	friend class AudioOutputI2SQuad_F32;
	//friend class AudioOutputI2SOct;
	//friend class AudioInputI2SOct;
	#endif

	static void scale_f32_to_i16( float32_t *p_f32, float32_t *p_i16, int len) ;
	static void scale_f32_to_i24( float32_t *p_f32, float32_t *p_i16, int len) ;
	static void scale_f32_to_i32( float32_t *p_f32, float32_t *p_i32, int len) ;
	static float setI2SFreq_T3(const float);
	static uint32_t *i2s_tx_buffer;
protected:
	AudioOutputI2S_F32(int dummy): AudioStream_F32(2, inputQueueArray) {} // to be used only inside AudioOutputI2Sslave !!
	static void config_i2s(void);
	static void config_i2s(bool);
	static void config_i2s(float);
	static void config_i2s(bool, float);
	//static void config_i2s_i16(void,float);
	//static void config_i2s_i32(void,float);
	static audio_block_f32_t *block_left_1st;
	static audio_block_f32_t *block_right_1st;
	static bool update_responsibility;
	static DMAChannel dma;
	static void isr_16(void);
	static void isr_32(void);
	static void isr(void);
private:
	static audio_block_f32_t *block_left_2nd;
	static audio_block_f32_t *block_right_2nd;
	static uint16_t block_left_offset;
	static uint16_t block_right_offset;
	audio_block_f32_t *inputQueueArray[2];
	static float sample_rate_Hz;
	static int audio_block_samples;
	volatile uint8_t enabled = 1;
};


class AudioOutputI2Sslave_F32 : public AudioOutputI2S_F32
{
public:
	AudioOutputI2Sslave_F32(void) : AudioOutputI2S_F32(0) { begin(); } ;
	void begin(void);
	friend class AudioInputI2Sslave_F32;
	friend void dma_ch0_isr(void);
protected:
	static void config_i2s(void);
};



#endif

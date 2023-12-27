#ifndef _DRAKON_AUDIO_H
#define _DRAKON_AUDIO_H

#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>

#include <clownresampler/clownresampler.h>

#include "DrakonFileBuffer.h"

typedef struct
{
	SceUID			soundThread;
	SceUID			readThread;
	SceUID			fileHandle;
	uint8_t*		header;
	uint8_t*		buffer;
	uint8_t*		readBuf;
	uint8_t*		fillBuf;
	uint32_t		dataSize;
	uint32_t		offset;
	uint32_t		readEnd;
	uint32_t		endflag;
	uint32_t		memory;
	uint32_t		format;
	DrakonFileBuffer*		fileBuffer;
	void*			handle;
	uint32_t		mode;
	uint32_t		channels;
	uint32_t		samplingRate;
	uint32_t		isLooping;
	ClownResampler_Precomputed resamplerPrecomputation;
	ClownResampler_HighLevel_State resampler;
	int16_t			wavBuff[BUFNUM][USER_GRAIN*STEREO];

} DrakonAudioHandler;

int32_t DrakonInitializeAudio(DrakonAudioHandler* audioHandler);
int32_t DrakonTerminateAudio(DrakonAudioHandler* audioHandler);

int32_t DrakonLoadWavFromMemory(DrakonAudioHandler* audioHandler, void* data, size_t size, SceUInt32 mode);
int32_t DrakonLoadWav(DrakonAudioHandler* audioHandler, const char* filename, SceUInt32 mode, SceUInt32 memory);
int32_t DrakonLoadOgg(DrakonAudioHandler* audioHandler, const char* filename, SceUInt32 mode, uint32_t isLooping);

int32_t DrakonTestOgg(DrakonAudioHandler* audioHandler);

int32_t DrakonPlayAudio(DrakonAudioHandler* audioHandler);
int32_t DrakonStopAudio(DrakonAudioHandler* audioHandler);

int32_t DrakonGetAudioStatus(DrakonAudioHandler* audioHandler);

void DrakonSetVolume(double tVolume);

#endif // _DRAKON_AUDIO_H

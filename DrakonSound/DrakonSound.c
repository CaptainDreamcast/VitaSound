#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/audioout.h>
#include <psp2/io/fcntl.h>

#define CLOWNRESAMPLER_IMPLEMENTATION
#include <DrakonSound/DrakonSound.h>
#include <DrakonSound/DrakonWavFile.h>
#include "include/nogg.h"

static int32_t readWavHeader(DrakonAudioHandler* audioHandler);

static int32_t soundThread(uint32_t args, void *argc);
static int32_t readThread(uint32_t args, void *argc);

static struct {
	double mVolume;
} gDrakonSound;

//ogg callbacks
static int32_t streaming_read(void *opaque, void *buffer, int32_t length)
{
	DrakonAudioHandler* audioHandler = (DrakonAudioHandler*)opaque;
	int32_t ret = (int32_t)sceIoRead(audioHandler->fileHandle, buffer, (size_t)length);
	if(audioHandler->isLooping && ret < length) {
		int32_t missing = length - ret;
		sceIoLseek(audioHandler->fileHandle, 0, SCE_SEEK_SET);
		int32_t ret2 = sceIoRead(audioHandler->fileHandle, buffer + ret, (size_t)missing);
		ret += ret2;
	}
	return ret;
}

static void streaming_close(void *opaque)
{
	DrakonAudioHandler* audioHandler = (DrakonAudioHandler*)opaque;
	sceIoClose(audioHandler->fileHandle);
}

static const vorbis_callbacks_t streaming_callbacks = {
	.read = streaming_read,
	.close = streaming_close,
};

//read wav header
static int32_t readWavHeader(DrakonAudioHandler* audioHandler)
{
	int32_t    ret = 0;
	uint32_t   readSize;
	uint32_t   readHeaderSize;
	uint8_t*   ptr;
	DrakonWaveHeader header;

	/*E reset offset */
	ret = sceIoLseek(audioHandler->fileHandle, 0, SCE_SEEK_SET);

	if (ret < 0)
	{
		//printf("sceIoLseek() 0x%08x\n", ret);
		goto term;
	}

	//E get filesize
	ret = sceIoLseek(audioHandler->fileHandle, 0, SCE_SEEK_END);

	if (ret < 0) 
	{
		//printf("sceIoLseek() 0x%08x\n", ret);
		goto term;
	}

	if (ret >= HEADER_SIZE)
	{
		readHeaderSize = HEADER_SIZE;
	}
	else
	{
		readHeaderSize = audioHandler->dataSize;
	}

	/*E reset offset again */
	ret = sceIoLseek(audioHandler->fileHandle, 0, SCE_SEEK_SET);

	if (ret < 0)
	{
		//printf("sceIoLseek() 0x%08x\n", ret);
		goto term;
	}

	ptr = audioHandler->header;
	readSize = readHeaderSize;

	/*E read wave header */
	while (readSize > 0)
	{
		ret = sceIoRead(audioHandler->fileHandle, ptr, readSize);

		if (ret < 0)
		{
			//printf ("ERROR: sceIoRead () 0x%08x\n", ret);
			goto term;
		}

		readSize -= ret;
		ptr += ret;
	}

	/*E parse wave header (get fmt chunk value) */
	ret = DrakonParseWaveHeader(&header, audioHandler->header, readHeaderSize);

	if (ret < 0)
	{
		//printf("ERROR: _sceParseWaveHeader() %d\n", ret);
		goto term;
	}

	/*E proceed file pointer to data chunk */
	ret = sceIoLseek(audioHandler->fileHandle, header.headerByte, SCE_SEEK_SET);

	if (ret < 0)
	{
		//printf("sceIoLseek() 0x%08x\n", ret);
		goto term;
	}

	audioHandler->dataSize		= header.dataChunkSize;
	audioHandler->channels		= header.nChannels;
	audioHandler->samplingRate	= header.samplingRate;

  term:
	return ret;
}

//read wav header
static int32_t readWavHeaderFromMemory(DrakonAudioHandler* audioHandler, void* tData, size_t tSize)
{
	int32_t    ret = 0;
	uint32_t   readHeaderSize;
	DrakonWaveHeader header;

	if (tSize >= HEADER_SIZE)
	{
		readHeaderSize = HEADER_SIZE;
	}
	else
	{
		readHeaderSize = tSize;
	}

	if(!audioHandler->header)
	{
		//printf("Header buffer not allocated\n");
		return 0;
	}
	memcpy(audioHandler->header, tData, readHeaderSize);
	
	ret = DrakonParseWaveHeader(&header, audioHandler->header, readHeaderSize);

	if (ret < 0)
	{
		//printf("ERROR: _sceParseWaveHeader() %d\n", ret);
		return ret;
	}

	audioHandler->dataSize		= header.dataChunkSize;
	audioHandler->offset		= header.headerByte;
	audioHandler->channels		= header.nChannels;
	audioHandler->samplingRate	= header.samplingRate;
	
	/*
	//printf("size: %d\n", audioHandler->dataSize);
	//printf("offset: %d\n", audioHandler->offset);
	//printf("channels: %d\n", audioHandler->channels);
	//printf("samplingRate: %d\n", audioHandler->samplingRate);
	//printf("bits: %d\n", header.bits);
	*/

	return ret;
}

static int32_t soundThread(uint32_t args, void *argc )
{
	int32_t  ret = 0;
	int32_t  portId;
	uint32_t side;
	int32_t  vol[STEREO];
	int32_t  size;
	int32_t  portType;
	int32_t  samplingRate;
	int32_t  param;
	DrakonAudioHandler* audioHandler;

	//printf ( "# Start-SoundThread\n" );

	audioHandler = *(DrakonAudioHandler**)(argc);

	vol[0] = vol[1] = (int32_t)(SCE_AUDIO_OUT_MAX_VOL * gDrakonSound.mVolume);

	/*E set port type, sampling rate, channels */
	if (audioHandler->mode == AUDIO_OUT_MAIN)
	{
		portType     = SCE_AUDIO_OUT_PORT_TYPE_MAIN;
		samplingRate = 48000;

		if (audioHandler->channels == 2) 
		{
			param = SCE_AUDIO_OUT_MODE_STEREO;
		} 
		else
		{
			param = SCE_AUDIO_OUT_MODE_MONO;
		}

	} 
	else
	{
		portType     = SCE_AUDIO_OUT_PORT_TYPE_BGM;
		samplingRate = audioHandler->samplingRate;

		if (audioHandler->channels == 2)
		{
			param = SCE_AUDIO_OUT_MODE_STEREO;
		} 
		else
		{
			param = SCE_AUDIO_OUT_MODE_MONO;
		}
	}

	/*E open port */
	portId = sceAudioOutOpenPort(portType, USER_GRAIN, samplingRate, param);

	if (portId < 0)
	{
		//printf("Error: sceAudioOutOpenPort() 0x%08x\n",portId);
		goto term;
	}

	/*E set Volume */
	sceAudioOutSetVolume(portId, (SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH), vol);

	side = 0;

	/*E wait till buffer become full */
	for (;;) 
	{
		size = DrakonGetBufferCurrentCount(audioHandler->fileBuffer);
		if (size >= FILEBUF_SIZE || audioHandler->readEnd || audioHandler->endflag)
		{
			break;
		}

		sceKernelDelayThread ( 1000 );
	}

	for (;;)
	{
		if (audioHandler->endflag || (DrakonGetBufferCurrentCount(audioHandler->fileBuffer) < USER_GRAIN * audioHandler->channels * sizeof(short) && audioHandler->readEnd)) {
			break;
		}

		/*E audio output */
		if (DrakonGetBufferCurrentCount(audioHandler->fileBuffer) >= USER_GRAIN * audioHandler->channels * sizeof(short))
		{
			DrakonGetBufferData(audioHandler->fileBuffer, (SceUChar8*)(audioHandler->wavBuff[side]), USER_GRAIN * audioHandler->channels * sizeof(short));
			ret = sceAudioOutOutput(portId, audioHandler->wavBuff[side]);
			if(ret < 0)
			{
				//printf ("Error: sceAudioOutOutput() 0x%08x\n",ret);
				break;
			}
			side ^= 0x01;
		} 
		else
		{
			/*E buffer underflow!! */
			//printf ("currentCount = %u\n", DrakonGetBufferCurrentCount(audioHandler->fileBuffer));
			sceKernelDelayThread(1000);
		}
	}

	sceAudioOutOutput(portId, NULL);

	/*E terminate function */
	ret = sceAudioOutReleasePort(portId);
	if(ret < 0)
	{
		//printf("Error: sceAudioOutReleasePort() 0x%08x\n",ret);
		goto term;
	}

term:
	//printf("# End-SoundThread\n");
	audioHandler->endflag = AUDIO_STATUS_END;
	ret = sceKernelExitDeleteThread(0);
	return ret;
}

typedef struct {
	DrakonAudioHandler* audioHandler;
	uint32_t bufferSpaceRemaining;
	uint32_t bufferPosition;
	uint32_t inputBufferSize;
	uint32_t actualReadBytes;
} ResamplerCallbackData;

static size_t ResamplerInputCallback(const void *user_data, short *buffer, size_t total_frames)
{
	ResamplerCallbackData* callbackData = (ResamplerCallbackData*)user_data;

	//printf("Input: Total frames %d vs space left %d\n", (int)total_frames, (int)callbackData->bufferSpaceRemaining);
	size_t frameSize = callbackData->audioHandler->channels * sizeof(uint16_t);
	size_t ret = sceIoRead(callbackData->audioHandler->fileHandle, buffer, total_frames * frameSize);
	callbackData->actualReadBytes += ret;
	size_t frames = ret / frameSize;
	return frames;
}

static char ResamplerOutputCallback(const void *user_data, const long *frame, unsigned int total_samples)
{
	ResamplerCallbackData* const callbackData = (ResamplerCallbackData*)user_data;

	unsigned int i;

	//printf("Output: Total samples %d vs remaining %d\n", (int)total_samples, (int)callbackData->bufferSpaceRemaining);

	/* Output the frame. */
	for (i = 0; i < total_samples; ++i)
	{
		long sample;

		sample = frame[i];
		//printf("sample %d\n", (int)sample);
		/* Clamp the sample to 16-bit. */
		if (sample > 0x7FFF)
			sample = 0x7FFF;
		else if (sample < -0x7FFF)
			sample = -0x7FFF;


		/* Push the sample to the output buffer. */
		*((int16_t*)(&callbackData->audioHandler->readBuf[callbackData->bufferPosition])) = (int16_t)sample;
		callbackData->bufferPosition+=2;
		callbackData->bufferSpaceRemaining-=2;
	}

	/* Signal whether there is more room in the output buffer. */
	return callbackData->bufferSpaceRemaining != 0;
}

static size_t ResamplerInputCallbackMemory(const void *user_data, short *buffer, size_t total_frames)
{
	ResamplerCallbackData* callbackData = (ResamplerCallbackData*)user_data;

	//printf("Input: Total frames %d vs space left %d\n", (int)total_frames, (int)callbackData->bufferSpaceRemaining);
	size_t frameSize = callbackData->audioHandler->channels * sizeof(uint16_t);
	size_t maxRead = total_frames * frameSize;
	size_t bytesLeft = callbackData->inputBufferSize - callbackData->bufferPosition;
	size_t ret = (maxRead < bytesLeft) ? maxRead : bytesLeft;
	memcpy(buffer, callbackData->audioHandler->fillBuf + callbackData->bufferPosition, ret);
	callbackData->bufferPosition += ret;
	callbackData->actualReadBytes += ret;
	size_t frames = ret / frameSize;
	return frames;
}

static char ResamplerOutputCallbackMemory(const void *user_data, const long *frame, unsigned int total_samples)
{
	ResamplerCallbackData* const callbackData = (ResamplerCallbackData*)user_data;

	unsigned int i;

	//printf("Output: Total samples %d vs remaining %d\n", (int)total_samples, (int)callbackData->bufferSpaceRemaining);

	/* Output the frame. */
	for (i = 0; i < total_samples; ++i)
	{
		long sample;

		sample = frame[i];
		//printf("sample %d\n", (int)sample);
		/* Clamp the sample to 16-bit. */
		if (sample > 0x7FFF)
			sample = 0x7FFF;
		else if (sample < -0x7FFF)
			sample = -0x7FFF;


		DrakonSetBufferData(callbackData->audioHandler->fileBuffer, &sample, sizeof(int16_t));					
		callbackData->bufferSpaceRemaining-=2;
	}

	/* Signal whether there is more room in the output buffer. */
	return callbackData->bufferSpaceRemaining != 0;
}

static int32_t readThread(uint32_t args, void *argc)
{
	int32_t  ret = 0;
	uint32_t readSize = 0;
	uint32_t capacity;
	DrakonAudioHandler* audioHandler;

	//printf ( "# Start-ReadThread\n" );

	audioHandler = *(DrakonAudioHandler**)(argc);

	readSize = audioHandler->dataSize;

	if (audioHandler->format == 0)
	{
		if (audioHandler->mode == AUDIO_OUT_MAIN && audioHandler->samplingRate != 48000)
		{
			//printf("resampling from %d to %d for mode %d with channel count %d\n", audioHandler->samplingRate, 48000, audioHandler->mode, audioHandler->channels);
			ClownResampler_Precompute(&audioHandler->resamplerPrecomputation);

			/* Create a resampler that converts from the sample rate of the MP3 to the sample rate of the playback device. */
			/* The low-pass filter is set to 44100Hz since that should allow all human-perceivable frequencies through. */
			ClownResampler_HighLevel_Init(&audioHandler->resampler, audioHandler->channels, audioHandler->samplingRate, 48000, 44100);
		}
		
		if (audioHandler->memory == 1)
		{
			int32_t  readPosition = 0;

			//read wave data from memory
			while (readSize > 0 && audioHandler->endflag != AUDIO_STATUS_END)
			{
				capacity = DrakonGetBufferCapacity(audioHandler->fileBuffer);

				if (capacity > 0)
				{
					if (capacity > READBUF_SIZE)
					{
						capacity = READBUF_SIZE;
					}

					if (capacity > readSize)
					{
						capacity = readSize;
					}

					if (capacity < 0)
					{
						//printf("ERROR: sceIoRead () 0x%08x\n", capacity);
						goto term;
					}

					if (audioHandler->mode == AUDIO_OUT_MAIN && audioHandler->samplingRate != 48000)
					{
						ResamplerCallbackData callbackData;
						callbackData.audioHandler = audioHandler;
						callbackData.bufferSpaceRemaining = capacity;
						callbackData.bufferPosition = readPosition;
						callbackData.inputBufferSize = audioHandler->dataSize;
						callbackData.actualReadBytes = 0;
						ClownResampler_HighLevel_Resample(&audioHandler->resampler, &audioHandler->resamplerPrecomputation, ResamplerInputCallbackMemory, ResamplerOutputCallbackMemory, &callbackData);
						readPosition += callbackData.actualReadBytes;
						readSize -= callbackData.actualReadBytes;
					}
					else {
						DrakonSetBufferData(audioHandler->fileBuffer, audioHandler->fillBuf + readPosition, capacity);					
						readSize -= capacity;
						readPosition += capacity;
					}
				}

				sceKernelDelayThread(1000);
			}
		}
		else
		{
			/*E read wave data */
			while (readSize > 0 && audioHandler->endflag != AUDIO_STATUS_END)
			{
				capacity = DrakonGetBufferCapacity(audioHandler->fileBuffer);

				if (capacity > 0)
				{
					if (capacity > READBUF_SIZE)
					{
						capacity = READBUF_SIZE;
					}

					if (capacity > readSize)
					{
						capacity = readSize;
					}

					int actualInputReadSize = 0;
					if (audioHandler->mode == AUDIO_OUT_MAIN && audioHandler->samplingRate != 48000)
					{
						ResamplerCallbackData callbackData;
						callbackData.audioHandler = audioHandler;
						callbackData.bufferSpaceRemaining = capacity;
						callbackData.bufferPosition = 0;
						callbackData.inputBufferSize = 0;
						callbackData.actualReadBytes = 0;
						ClownResampler_HighLevel_Resample(&audioHandler->resampler, &audioHandler->resamplerPrecomputation, ResamplerInputCallback, ResamplerOutputCallback, &callbackData);
						ret = capacity - callbackData.bufferSpaceRemaining;
						actualInputReadSize = callbackData.actualReadBytes;
					}
					else
					{
						actualInputReadSize = ret = sceIoRead(audioHandler->fileHandle, audioHandler->readBuf, capacity);
					}

					if (ret < 0)
					{
						//printf("ERROR: sceIoRead () 0x%08x\n", ret);
						goto term;
					}

					DrakonSetBufferData(audioHandler->fileBuffer, audioHandler->readBuf, ret);

					readSize -= actualInputReadSize;
				}

				sceKernelDelayThread(1000);
			}
		}
	}
	else if (audioHandler->format == 1)
	{
		int count = 1;

		while (count > 0 && audioHandler->endflag != AUDIO_STATUS_END)
		{
			capacity = DrakonGetBufferCapacity(audioHandler->fileBuffer);

			if (capacity > 0)
			{
				if (capacity > READBUF_SIZE)
				{
					capacity = READBUF_SIZE;
				}

				//printf("capacity:%d\n", capacity);

				int buffSize = capacity;
				vorbis_error_t error;
				int16_t* buf = malloc(sizeof(int16_t) * buffSize);
				int buffer_len = buffSize / 4;

				if (buffer_len > 0)
				{
					count = vorbis_read_int16((vorbis_t *)audioHandler->handle, buf, buffer_len, &error);

					if (count < 0)
					{
						//printf("ERROR: sceIoRead () 0x%08x\n", ret);
						goto term;
					}

					if (error == VORBIS_ERROR_DECODE_RECOVERED)
					{
						if(!audioHandler->isLooping) // This might happen with some samples during a loop
						{
							//printf("Warning: possible corruption at sample \n");
							goto term;
						}
					}
					else if (error && error != VORBIS_ERROR_STREAM_END)
					{
						if (error == VORBIS_ERROR_DECODE_FAILED)
						{
							//printf("Decode failed at sample d\n");
						}
						else if (error == VORBIS_ERROR_INSUFFICIENT_RESOURCES)
						{
							//printf("Out of memory at sample d\n");
						}
						else
						{
							//printf("Unexpected libnogg error %d at sample \n", error);
						}

						goto term;
					}

					/*memcpy(audioHandler->readBuf, buf, count * 4);
					free(buf);

					DrakonSetBufferData(audioHandler->fileBuffer, audioHandler->readBuf, count * 4);*/

					DrakonSetBufferData(audioHandler->fileBuffer, buf, count * 4);
					free(buf);
				}					
			}

			sceKernelDelayThread(1000);
		}		
	}
	

term:
	//printf("# End-ReadThread\n");
	audioHandler->readEnd = AUDIO_READFLAG_READEND;
	ret = sceKernelExitDeleteThread(0);
	return ret;
}

int32_t DrakonTestOgg(DrakonAudioHandler* audioHandler)
{
	int count = 1;
	int readData = 0;

	while (count > 0)
	{
		//capacity = GetBufferCapacity(audioHandler->fileBuffer);
		int capacity = READBUF_SIZE;

		int buffSize = capacity / 2;
		vorbis_error_t error;

		int16_t* buf = malloc(sizeof(int16_t) * buffSize);
		int buffer_len = buffSize / 4;

		count = vorbis_read_int16((vorbis_t *)audioHandler->handle, buf, buffer_len, &error);
		readData += count;

		free(buf);
	}

	//printf("Samples read %d\n", readData);

	return 0;
}

int32_t DrakonInitializeAudio(DrakonAudioHandler* audioHandler)
{
	int32_t ret = 0;

	memset(audioHandler, 0, sizeof(DrakonAudioHandler));

	audioHandler->fileHandle = -1;

	audioHandler->buffer = (uint8_t*)malloc(FILEBUF_SIZE);

	if (audioHandler->buffer == NULL) 
	{
		//printf("ERROR: malloc buffer\n");
		ret = AUDIO_ERROR_OUT_OF_MEMORY;
		goto error;
	}

	audioHandler->header = (uint8_t*)malloc(HEADER_SIZE);

	if (audioHandler->header == NULL)
	{
		//printf("ERROR: malloc header\n");
		ret = AUDIO_ERROR_OUT_OF_MEMORY;
		goto error;
	}
	
	audioHandler->readBuf = (uint8_t*)malloc(READBUF_SIZE);

	if (audioHandler->readBuf == NULL)
	{
		//printf("ERROR: malloc readBuf\n");
		ret = AUDIO_ERROR_OUT_OF_MEMORY;
		goto error;
	}

	audioHandler->fileBuffer = (DrakonFileBuffer*)malloc(sizeof(DrakonFileBuffer));

	if(audioHandler->fileBuffer== NULL)
	{
		//printf("ERROR: malloc fileBuffer\n");
		ret = AUDIO_ERROR_OUT_OF_MEMORY;
		goto error;
	}

	ret = DrakonCreateBuffer(audioHandler->fileBuffer, audioHandler->buffer, FILEBUF_SIZE);

	if (ret < 0) 
	{
		//printf("ERROR: CreateBuffer () 0x%08x\n", ret);
		goto error;
	}

	/*E create sound thread */
	audioHandler->soundThread = sceKernelCreateThread(
							SOUND_THREAD_NAME,
							soundThread,
							SCE_KERNEL_DEFAULT_PRIORITY_USER,
							SOUND_THREAD_STACK_SIZE,
							0,
							SCE_KERNEL_CPU_MASK_USER_ALL,
							0);

	if (audioHandler->soundThread < 0)
	{
		//printf("Error: sceKernelCreateThread 0x%08x\n", audioHandler->soundThread);
		ret = audioHandler->soundThread;
		goto error;
	}

	/*E create read thread */
	audioHandler->readThread = sceKernelCreateThread(
							READ_THREAD_NAME,
							readThread,
							SCE_KERNEL_DEFAULT_PRIORITY_USER + 1,
							READ_THREAD_STACK_SIZE,
							0,
							SCE_KERNEL_CPU_MASK_USER_ALL,
							0);

	if (audioHandler->readThread < 0)
	{
		//printf("Error: sceKernelCreateThread 0x%08x\n", audioHandler->readThread);
		ret = audioHandler->readThread;
		goto error;
	}

	goto term;

error:

	if (audioHandler->readThread > 0)
	{
		sceKernelDeleteThread(audioHandler->readThread);
		audioHandler->readThread = 0;
	}

	if (audioHandler->soundThread > 0) 
	{
		sceKernelDeleteThread (audioHandler->soundThread);
		audioHandler->soundThread = 0;
	}

	if ( audioHandler->fileBuffer )
	{
		DrakonDeleteBuffer(audioHandler->fileBuffer);
		free(audioHandler->fileBuffer);
		audioHandler->fileBuffer = NULL;
	}
	
	if ( audioHandler->fillBuf)
	{
		free ( audioHandler->fillBuf );
		audioHandler->fillBuf = NULL;
	}

	if (audioHandler->readBuf)
	{
		free(audioHandler->readBuf);
		audioHandler->readBuf = NULL;
	}

	if (audioHandler->buffer)
	{
		free(audioHandler->buffer);
		audioHandler->buffer = NULL;
	}

	if (audioHandler->header)
	{
		free(audioHandler->header);
		audioHandler->header = NULL;
	}

term:
	return ret;
}


int32_t DrakonTerminateAudio(DrakonAudioHandler* audioHandler)
{
	audioHandler->endflag = AUDIO_STATUS_END;

	if ( audioHandler->readThread > 0 ) 
	{
		sceKernelWaitThreadEnd ( audioHandler->readThread, NULL, 0 );
	}

	if ( audioHandler->soundThread > 0 )
	{
		sceKernelWaitThreadEnd ( audioHandler->soundThread, NULL, 0 );
	}

	if ( audioHandler->readThread > 0 )
	{
		sceKernelDeleteThread ( audioHandler->readThread );
		audioHandler->readThread = 0;
	}

	if ( audioHandler->soundThread > 0 ) 
	{
		sceKernelDeleteThread ( audioHandler->soundThread );
		audioHandler->soundThread = 0;
	}

	if ( audioHandler->fileHandle >= 0 ) 
	{
		sceIoClose ( audioHandler->fileHandle );
		audioHandler->fileHandle = -1;
	}
	
	if ( audioHandler->fillBuf )
	{
		free ( audioHandler->fillBuf );
		audioHandler->fillBuf = NULL;
	}

	if ( audioHandler->fileBuffer )
	{
		DrakonDeleteBuffer ( audioHandler->fileBuffer );
		free ( audioHandler->fileBuffer );
		audioHandler->fileBuffer = NULL;
	}

	if ( audioHandler->readBuf )
	{
		free ( audioHandler->readBuf );
		audioHandler->readBuf = NULL;
	}

	if ( audioHandler->buffer ) 
	{
		free ( audioHandler->buffer );
		audioHandler->buffer = NULL;
	}

	if ( audioHandler->header )
	{
		free ( audioHandler->header );
		audioHandler->header = NULL;
	}

	return 0;
}

int32_t DrakonLoadOgg(DrakonAudioHandler* audioHandler, const char* filename, SceUInt32 mode, uint32_t isLooping)
{
	int32_t ret = 0;

	vorbis_t *handle = NULL;
	vorbis_error_t error;
	const unsigned int options = 0;

	audioHandler->mode = mode;
	audioHandler->memory = 0;

	audioHandler->format = 1; //ogg

	audioHandler->endflag = 0;
	audioHandler->readEnd = 0;

	audioHandler->isLooping = isLooping;

	audioHandler->fileHandle = sceIoOpen(filename, SCE_O_RDONLY, 0);

	if (audioHandler->fileHandle < 0)
	{
		//printf("sceIoOpen() 0x%08x\n", audioHandler->fileHandle);
		ret = audioHandler->fileHandle;
		goto error;
	}

	handle = vorbis_open_callbacks(streaming_callbacks, audioHandler, options, &error);

	if (!handle)
	{
		if (error == VORBIS_ERROR_INSUFFICIENT_RESOURCES)
		{
			//printf("LoadOgg Out of memory\n");
		}
		else if (error == VORBIS_ERROR_STREAM_INVALID)
		{
			//printf("LoadOgg Invalid stream format\n");
		}
		else if (error == VORBIS_ERROR_STREAM_END)
		{
			//printf("LoadOgg Unexpected EOF\n");
		}
		else if (error == VORBIS_ERROR_DECODE_SETUP_FAILED)
		{
			//printf("LoadOgg Failed to initialize decoder\n");
		}
		else
		{
			//printf("LoadOgg Unexpected libnogg error %d\n", error);
		}

		ret = -1;
		goto error;
	}

	audioHandler->handle = handle;

	//read header
	const int channels = vorbis_channels(handle);
	const uint32_t rate = vorbis_rate(handle);

	audioHandler->channels = channels;
	audioHandler->samplingRate = rate;

	goto term;

error:
	audioHandler->endflag = AUDIO_STATUS_END;

	if (audioHandler->readThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->readThread, NULL, 0);
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->soundThread, NULL, 0);
	}

	if (audioHandler->readThread > 0)
	{
		sceKernelDeleteThread(audioHandler->readThread);
		audioHandler->readThread = 0;
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelDeleteThread(audioHandler->soundThread);
		audioHandler->soundThread = 0;
	}

	if (audioHandler->fileHandle >= 0)
	{
		sceIoClose(audioHandler->fileHandle);
		audioHandler->fileHandle = -1;
	}

term:

	return ret;
}

int32_t DrakonLoadWavFromMemory(DrakonAudioHandler* audioHandler, void* data, size_t size, SceUInt32 mode) {
	int32_t ret = 0;

	audioHandler->mode = mode;
	audioHandler->memory = 1;

	audioHandler->endflag = 0;
	audioHandler->readEnd = 0;

	audioHandler->format = 0;//wav

	ret = readWavHeaderFromMemory(audioHandler, data, size);

	if (ret < 0)
	{
		//printf("ERROR: readHeader() 0x%08x\n", ret);
		goto term;
	}

	if (audioHandler->memory == 1)
	{
		uint32_t dataSize = audioHandler->dataSize;

		//alloc data buffer
		audioHandler->fillBuf = (uint8_t*)malloc(dataSize);

		if (audioHandler->fillBuf == NULL)
		{
			//printf("ERROR: malloc fillBuf\n");
			ret = AUDIO_ERROR_OUT_OF_MEMORY;
			goto error;
		}

		memcpy(audioHandler->fillBuf, data+audioHandler->offset, dataSize);
	}

	goto term;

error:
	audioHandler->endflag = AUDIO_STATUS_END;

	if (audioHandler->readThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->readThread, NULL, 0);
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->soundThread, NULL, 0);
	}

	if (audioHandler->readThread > 0)
	{
		sceKernelDeleteThread(audioHandler->readThread);
		audioHandler->readThread = 0;
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelDeleteThread(audioHandler->soundThread);
		audioHandler->soundThread = 0;
	}

	if (audioHandler->fileHandle >= 0)
	{
		sceIoClose(audioHandler->fileHandle);
		audioHandler->fileHandle = -1;
	}

term:

	return ret;
}

int32_t DrakonLoadWav(DrakonAudioHandler* audioHandler, const char* filename, SceUInt32 mode, SceUInt32 memory)
{
	int32_t ret = 0;

	audioHandler->mode = mode;
	audioHandler->memory = memory;

	audioHandler->endflag = 0;
	audioHandler->readEnd = 0;

	audioHandler->format = 0;//wav

	audioHandler->fileHandle = sceIoOpen(filename, SCE_O_RDONLY, 0);

	if (audioHandler->fileHandle < 0)
	{
		//printf("sceIoOpen() 0x%08x\n", audioHandler->fileHandle);
		ret = audioHandler->fileHandle;
		goto error;
	}

	ret = readWavHeader(audioHandler);

	if (ret < 0)
	{
		//printf("ERROR: readHeader() 0x%08x\n", ret);
		goto term;
	}

	if (audioHandler->memory == 1)
	{
		//load data from wave file
		uint32_t dataSize = audioHandler->dataSize;

		//alloc data buffer
		audioHandler->fillBuf = (uint8_t*)malloc(dataSize);

		if (audioHandler->fillBuf == NULL)
		{
			//printf("ERROR: malloc fillBuf\n");
			ret = AUDIO_ERROR_OUT_OF_MEMORY;
			goto error;
		}

		ret = sceIoRead(audioHandler->fileHandle, audioHandler->fillBuf, dataSize);

		if (ret < 0)
		{
			//printf("ERROR: sceIoRead () 0x%08x\n", ret);
			goto term;
		}
	}

	goto term;

error:
	audioHandler->endflag = AUDIO_STATUS_END;

	if (audioHandler->readThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->readThread, NULL, 0);
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->soundThread, NULL, 0);
	}

	if (audioHandler->readThread > 0)
	{
		sceKernelDeleteThread(audioHandler->readThread);
		audioHandler->readThread = 0;
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelDeleteThread(audioHandler->soundThread);
		audioHandler->soundThread = 0;
	}

	if (audioHandler->fileHandle >= 0)
	{
		sceIoClose(audioHandler->fileHandle);
		audioHandler->fileHandle = -1;
	}

term:

	return ret;
}

int32_t DrakonPlayAudio(DrakonAudioHandler* audioHandler)
{
	int32_t ret = 0;

	ret = sceKernelStartThread(audioHandler->readThread, sizeof(audioHandler), &audioHandler);

	if (ret < 0)
	{
		//printf("Error: [readThread]sceKernelStartThread 0x%08x\n", ret);
		goto term;
	}

	ret = sceKernelStartThread(audioHandler->soundThread, sizeof(audioHandler), &audioHandler);

	if (ret < 0)
	{
		//printf("Error: [soundThread]sceKernelStartThread 0x%08x\n", ret);
		goto term;
	}

	goto term;

error:
	audioHandler->endflag = AUDIO_STATUS_END;

	if (audioHandler->readThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->readThread, NULL, 0);
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelWaitThreadEnd(audioHandler->soundThread, NULL, 0);
	}

	if (audioHandler->readThread > 0)
	{
		sceKernelDeleteThread(audioHandler->readThread);
		audioHandler->readThread = 0;
	}

	if (audioHandler->soundThread > 0)
	{
		sceKernelDeleteThread(audioHandler->soundThread);
		audioHandler->soundThread = 0;
	}

	if (audioHandler->fileHandle >= 0)
	{
		sceIoClose(audioHandler->fileHandle);
		audioHandler->fileHandle = -1;
	}

term:

	return ret;
}

int32_t DrakonStopAudio(DrakonAudioHandler* audioHandler)
{
	audioHandler->endflag = AUDIO_STATUS_END;

	if (audioHandler->readThread > 0) {
		sceKernelWaitThreadEnd(audioHandler->readThread, NULL, NULL);
	}

	if ( audioHandler->soundThread > 0 ) {
		sceKernelWaitThreadEnd(audioHandler->soundThread, NULL, NULL);
	}

	if (audioHandler->readThread > 0) {
		sceKernelDeleteThread(audioHandler->readThread);
		audioHandler->readThread = 0;
	}

	if (audioHandler->soundThread > 0) {
		sceKernelDeleteThread(audioHandler->soundThread);
		audioHandler->soundThread = 0;
	}

	if (audioHandler->fileHandle >= 0 ) {
		sceIoClose(audioHandler->fileHandle);
		audioHandler->fileHandle = -1;
	}

	return 0;
}

int32_t DrakonGetAudioStatus(DrakonAudioHandler* audioHandler)
{
	return audioHandler->endflag;
}

void DrakonSetVolume(double tVolume) {
	gDrakonSound.mVolume = tVolume;
}
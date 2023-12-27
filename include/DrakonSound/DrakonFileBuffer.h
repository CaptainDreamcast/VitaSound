#ifndef DrakonFileBuffer_H
#define DrakonFileBuffer_H

#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>

#include "DrakonMissingDefinitions.h"

typedef struct DrakonFileBuffer
{
	uint32_t	initFlag;
	SceUID		lock;
	SceKernelMutexOptParam optParam;
	uint8_t*	buf;
	uint32_t	write;
	uint32_t	read;
	uint32_t	count;
	uint32_t	size;
} DrakonFileBuffer;

int32_t DrakonCreateBuffer(DrakonFileBuffer* fileBuffer, uint8_t* buf, uint32_t size);
int32_t DrakonDeleteBuffer(DrakonFileBuffer* fileBuffer);
int32_t DrakonResetBuffer(DrakonFileBuffer* fileBuffer);
int32_t DrakonSetBufferData(DrakonFileBuffer* fileBuffer, uint8_t* data, uint32_t setSize);
int32_t DrakonGetBufferData(DrakonFileBuffer* fileBuffer, uint8_t* data, uint32_t getSize);
int32_t DrakonGetBufferCapacity(DrakonFileBuffer* fileBuffer);
int32_t DrakonGetBufferCurrentCount(DrakonFileBuffer* fileBuffer);

#endif

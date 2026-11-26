// Test program for the Cemu web port: CPU, FPU, threads, OS calls. No graphics.
// Prints lines starting with "HB:" through OSReport; the expected output is deterministic.
#include <coreinit/debug.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <coreinit/memdefaultheap.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static uint32_t crc32(const uint8_t* data, uint32_t len)
{
	uint32_t crc = 0xFFFFFFFF;
	for (uint32_t i = 0; i < len; i++)
	{
		crc ^= data[i];
		for (int k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
	}
	return ~crc;
}

static uint32_t integerWork(uint32_t iterations)
{
	uint32_t x = 0x12345678, acc = 0;
	for (uint32_t i = 0; i < iterations; i++)
	{
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		acc += (x % 1000) * (i & 7) + (x >> 3) / ((i & 15) + 1);
		acc = (acc << 3) | (acc >> 29);
	}
	return acc;
}

static uint64_t floatWork(uint32_t iterations)
{
	double d = 0.5;
	float f = 1.25f;
	for (uint32_t i = 0; i < iterations; i++)
	{
		d = d * 1.0000001 + sqrt((double)(i & 255)) - sin(d) * 0.001;
		f = f * 0.999f + (float)(i & 31) * 0.0625f;
	}
	uint64_t bits;
	memcpy(&bits, &d, sizeof(bits));
	uint32_t fbits;
	memcpy(&fbits, &f, sizeof(fbits));
	return bits ^ ((uint64_t)fbits << 7);
}

static volatile uint32_t sharedCounter = 0;

static int workerMain(int argc, void* argv)
{
	uint32_t r = integerWork(200000 + (uint32_t)argc * 1000);
	for (int i = 0; i < 1000; i++)
	{
		sharedCounter++;
		if ((i & 63) == 0)
			OSYieldThread();
	}
	return (int)(r & 0x7FFFFFFF);
}

int main(int argc, char** argv)
{
	OSReport("HB: hello from the Wii U test program\n");

	static uint8_t buffer[64 * 1024];
	for (uint32_t i = 0; i < sizeof(buffer); i++)
		buffer[i] = (uint8_t)(i * 31 + (i >> 8));
	OSReport("HB: crc32=%08x\n", crc32(buffer, sizeof(buffer)));

	OSTime t0 = OSGetTime();
	uint32_t iw = integerWork(2000000);
	OSTime t1 = OSGetTime();
	OSReport("HB: int=%08x\n", iw);
	uint64_t fw = floatWork(500000);
	OSTime t2 = OSGetTime();
	OSReport("HB: float=%08x%08x\n", (uint32_t)(fw >> 32), (uint32_t)fw);

	// two threads that run concurrently with the main thread
	OSThread* threads[2];
	int results[2];
	for (int i = 0; i < 2; i++)
	{
		const uint32_t stackSize = 64 * 1024;
		threads[i] = (OSThread*)MEMAllocFromDefaultHeapEx(sizeof(OSThread), 8);
		uint8_t* stack = (uint8_t*)MEMAllocFromDefaultHeapEx(stackSize, 16);
		memset(threads[i], 0, sizeof(OSThread));
		OSCreateThread(threads[i], workerMain, i + 1, NULL, stack + stackSize, stackSize, 16, OS_THREAD_ATTRIB_AFFINITY_ANY);
		OSResumeThread(threads[i]);
	}
	for (int i = 0; i < 2; i++)
		OSJoinThread(threads[i], &results[i]);
	OSReport("HB: threads=%08x,%08x counter=%u\n", results[0], results[1], (unsigned)sharedCounter);

	char text[64];
	snprintf(text, sizeof(text), "%.6f", 3.14159265358979 * 2.0);
	OSReport("HB: printf=%s\n", text);

	OSReport("HB: ticks int=%u float=%u (not deterministic)\n", (unsigned)OSTicksToMilliseconds(t1 - t0), (unsigned)OSTicksToMilliseconds(t2 - t1));
	OSReport("HB: done\n");
	return 0;
}

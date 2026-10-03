#include "stdafx.h"

#include "XSPI.h"
#include <stdio.h>

bool XNANDWaitReady(unsigned int timeout)
{
	do {
		if (!(XSPIReadBYTE_(0x04) & 0x01))
			return true;
	} while (timeout--);

	return false;
}

unsigned int XNANDGetStatus()
{
	return XSPIReadWORD_(0x04);
}

void XNANDClearStatus()
{
	unsigned char tmp[4] = { 0,2,0,0 };

	XSPIRead_sync(4, tmp);	
	XSPIWrite_sync(4, tmp);	

}

unsigned int XNANDReadStart(unsigned int block)
{
	unsigned int res;
	unsigned int tries = 0x1000;

	XNANDClearStatus();

	XSPIBatchBegin();	

	XSPIQueueWriteDWORD(0x0C, block << 9);
	XSPIQueueWriteBYTE(0x08, 0x03);

	XSPIBatchSend();

	if (!XNANDWaitReady(0x1000))
		return 0x8011;

	res = 0;

	XSPIBatchBegin();
	XSPIQueueWrite0(0x0C);
	XSPIBatchSend();

	return res;
}

void XNANDReadProcess(unsigned char * pData, unsigned char Words ) {
	unsigned int len = Words;

	XSPIBatchBegin();
	while (Words--) {
		XSPIQueueWrite0(0x08);
		XSPIQueueRead(0x10);
	}

	XSPIBatchReceive(pData, len * 4);
}

unsigned int XNANDErase(unsigned int block)
{
	unsigned char tmp[4];

	XNANDClearStatus();

	XSPIRead_sync(0, tmp);	
	tmp[0] |= 0x08;
	XSPIWrite_sync(0, tmp);

	XSPIBatchBegin();	

	XSPIQueueWriteDWORD(0x0C, block << 9);
	XSPIQueueWriteBYTE(0x08, 0xAA);
	XSPIQueueWriteBYTE(0x08, 0x55);
	XSPIQueueWriteBYTE(0x08, 0x5);

	XSPIBatchSend();

	if (!XNANDWaitReady(0x1000))
		return 0x8003;

	return XNANDGetStatus();	
}

void XNANDWriteStart()
{
	XSPIBatchBegin();
	XSPIQueueWrite0(0x0C);
	XSPIBatchSend();
}

void XNANDWriteProcess(unsigned char Buffer[], unsigned char Words) 
{
	XSPIBatchBegin();	
	while (Words--) {
		XSPIWrite_(0x10, Buffer);
		XSPIQueueWriteBYTE(0x08, 0x01);
		Buffer += 4;
	}
	XSPIBatchSend();
}

unsigned int XNANDWriteExecute(unsigned int block) 
{
	unsigned int tries = 0x1000;

	XSPIBatchBegin();	
	XSPIQueueWriteDWORD(0x0C, block << 9);
	XSPIQueueWriteBYTE(0x08, 0x55);
	XSPIQueueWriteBYTE(0x08, 0xAA);
	XSPIQueueWriteBYTE(0x08, 0x4);
	XSPIBatchSend();

	if (!XNANDWaitReady(0x1000))
		return 0x8021;

	return XNANDGetStatus();
}

// ---------------------------------------------------------------------------
// Fast batched read path (optimization)
// ---------------------------------------------------------------------------

// Read nPages consecutive physical pages in a single USB write + single USB read.
// Mirrors XNANDReadStart()+XNANDReadProcess() per page, but: status is cleared once
// for the whole batch, the busy-poll is replaced by an in-stream delay, and all the
// page commands/reads are queued before a single XSPIBatchReceive().
//   wordsPerPage : bytes-per-page / 4 (132 for 0x210 incl. spare, 128 for 0x200 raw)
//   delayBytes   : page-load delay via XSPIQueueDelay()
// Keep nPages * wordsPerPage * 4 below the FT2232H RX FIFO (~4 KB).
void XNANDReadBatch(unsigned char* pData, unsigned int startBlock,
                    unsigned int nPages, unsigned int wordsPerPage,
                    unsigned int delayBytes)
{
	XNANDClearStatus();          // once per batch, not once per page

	XSPIBatchBegin();

	for (unsigned int p = 0; p < nPages; p++) {
		unsigned int block = startBlock + p;

		// start read of one physical page into the SFC page buffer
		XSPIQueueWriteDWORD(0x0C, block << 9);   // set address (queue-only)
		XSPIQueueWriteBYTE(0x08, 0x03);          // PHY_PAGE_TO_BUF (queue-only)
		XSPIQueueDelay(delayBytes);              // wait tR in-stream, no USB round-trip
		XSPIQueueWrite0(0x0C);                   // reset page-buffer pointer

		// drain the page buffer
		for (unsigned int w = 0; w < wordsPerPage; w++) {
			XSPIQueueWrite0(0x08);               // PAGE_BUF_TO_REG: advance one word
			XSPIQueueRead(0x10);                 // queue read of the data register
		}
	}

	XSPIBatchReceive(pData, nPages * wordsPerPage * 4);
}


#ifndef WRAPPER_H
#define WRAPPER_H

#include "ftd2xx.h"

bool spi_init( void );
void spi_setGPIO( bool GPIO1, bool GPIO2 );
void spi_SetCS( bool ChipSelect );

void AddByteToOutputBuffer( BYTE DataByte, bool bClearOutputBuffer );
void ClearOutputBuffer( void );
void SendBytesToDevice( void );
void DisableSPIChip( void );
void EnableSPIChip( void );
void SetAnswerFast( void );
void AddWriteOutBuffer( DWORD dwNumControlBitsToWrite, unsigned char pWriteControlBuffer[] );
void AddReadOutBuffer( DWORD dwNumDataBitsToRead );
void GetDataFromDevice(unsigned int dwNumDataBitsToRead, unsigned char ReadDataBuffer[] );
void spi_QueueClockDelay( unsigned int numBytes );
void closeDevice();

// Host-side MPSSE command staging buffer. Enlarged from the original 65535 so a
// whole NAND_READ_BATCH_PAGES batch of read commands fits in one queue (a 0x210
// page costs ~4.9 KB of MPSSE commands, so 512 KB holds ~100 pages). The D2XX
// driver background-drains the chip RX FIFO, so batch size is bounded by this
// buffer, not the ~4 KB chip FIFO.
#define OUTPUT_BUFFER_SIZE (512 * 1024)
extern BYTE byOutputBuffer[OUTPUT_BUFFER_SIZE];
extern DWORD dwNumBytesToSend;
#endif

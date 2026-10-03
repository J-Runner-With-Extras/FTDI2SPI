/*! 
*
*/

#ifndef XSPI_H
#define XSPI_H

extern void XSPIInit(void);
extern void XSPIClose(void);

extern void XSPIEnterFlashMode(void);
extern void XSPILeaveFlashMode(void);
extern void XSPIPowerUp();
extern void XSPIShutdown();

// Transaction & Batching Lifecycle
extern void XSPIBatchBegin(void);
extern void XSPIBatchSend(void);
extern void XSPIBatchReceive(unsigned char* outBuf, unsigned int numBytes);
extern void XSPIQueueDelay(unsigned int numBytes);

// Queueable Register Operations (within an active batch)
extern void XSPIQueueWriteDWORD(unsigned char reg, unsigned int data);
extern void XSPIQueueWriteBYTE(unsigned char reg, unsigned char data);
extern void XSPIQueueWrite0(unsigned char reg);
extern void XSPIQueueRead(unsigned char reg);

// Bulk Block Operations
extern void XSPIReadBlock(unsigned char reg, unsigned char* buf, int numWords);
extern void XSPIWriteBlock(unsigned char reg, const unsigned char* buf, int numWords);

// Legacy / Direct SPI Operations
extern void XSPIRead_sync(unsigned char reg, unsigned char Data[]);
extern void XSPIRead_(unsigned char reg, unsigned char Data[]);
extern void XSPIWrite_(unsigned char reg, unsigned char data[] );
extern void XSPIWrite_sync(unsigned char reg, unsigned char data[] );

extern unsigned int XSPIReadWORD_(unsigned char reg);
extern unsigned char XSPIReadBYTE_(unsigned char reg);
extern void XSPIWrite0_(unsigned char reg);
extern void XSPIWriteBYTE_(unsigned char reg, unsigned char d);
extern void XSPIWriteWORD_(unsigned char reg, unsigned int data);
extern unsigned int XSPIReadDWORD_(unsigned char reg);
extern void XSPIReadBlock_(unsigned char reg, unsigned char* buf, int bytes);
extern void XSPIQueueBytes_(unsigned char reg, unsigned int Data);

extern void XSPIWriteBlock_(unsigned char reg, unsigned char* data);
#endif
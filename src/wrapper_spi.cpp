#include "stdafx.h"
#include <stdlib.h>

#include "ftd2xx.h"
#include <stdio.h>
#include "wrapper_spi.h"

#define MAX_NUM_BYTES_USB_WRITE 16384

#define MAX_READ_DATA_WORDS_BUFFER_SIZE 65536    // 64k bytes
#define MAX_FREQ_CLOCK_DIVISOR			0

#define CHIP_SELECT_PIN '\x08'

const BYTE SET_LOW_BYTE_DATA_BITS_CMD = '\x80';
const BYTE SET_HIGH_BYTE_DATA_BITS_CMD = '\x82';
const BYTE SEND_ANSWER_BACK_IMMEDIATELY_CMD = '\x87';

const BYTE CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD = '\x19';
const BYTE CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD = '\x1B';
const BYTE CLK_DATA_BYTES_IN_ON_NEG_CLK_LSB_FIRST_CMD = '\x2D';
const BYTE CLK_DATA_BITS_IN_ON_NEG_CLK_LSB_FIRST_CMD = '\x2F';


typedef WORD ReadDataWordBuffer[MAX_READ_DATA_WORDS_BUFFER_SIZE];
typedef ReadDataWordBuffer *PReadDataWordBuffer;

FT_HANDLE ftHandle = NULL;

BYTE byOutputBuffer[OUTPUT_BUFFER_SIZE];
BYTE dwLowPinsValue = 0;
DWORD dwNumBytesToSend = 0; // Index to the output buffer
DWORD dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
DWORD dwNumBytesToRead = 0; // Number of bytes available to read

bool spi_init( void )
{
  FT_STATUS ftStatus;
  DWORD numDevs = 0;
  char targetDesc[64] = {0};

  // Enumerate all connected FTDI devices
  ftStatus = FT_CreateDeviceInfoList(&numDevs);
  if (ftStatus != FT_OK || numDevs == 0)
    return false;

  FT_DEVICE_LIST_INFO_NODE* devInfo =
    (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE) * numDevs);
  if (!devInfo)
    return false;

  bool found = false;
  ftStatus = FT_GetDeviceInfoList(devInfo, &numDevs);
  if (ftStatus == FT_OK) {
    for (DWORD i = 0; i < numDevs; i++) {
      const char* desc = devInfo[i].Description;
      size_t len = strlen(desc);
      // Select channel B: description ends with " B"
      if (len >= 2 && desc[len - 2] == ' ' && desc[len - 1] == 'B') {
        strncpy_s(targetDesc, sizeof(targetDesc), desc, _TRUNCATE);
        found = true;
        break;
      }
    }
  }
  free(devInfo);

  if (!found)
    return false;

  // Open the channel B device by description
  ftStatus = FT_OpenEx((PVOID)targetDesc, FT_OPEN_BY_DESCRIPTION, &ftHandle);
  if (ftStatus != FT_OK)
    return false;

  // Reset to serial mode, then switch to MPSSE
  FT_SetBitMode(ftHandle, 0x00, 0x00);  // Reset
  Sleep(50);
  FT_SetBitMode(ftHandle, 0x00, 0x02);  // MPSSE mode
  Sleep(50);

  // Flush buffers and configure USB transfer sizes
  FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);
  FT_SetUSBParameters(ftHandle, 64 * 1024, 64 * 1024);
  FT_SetLatencyTimer(ftHandle, 1);

  BYTE cmd[8];
  DWORD bytesWritten;

  // Disable loopback (MPSSE command 0x85)
  cmd[0] = 0x85;
  FT_Write(ftHandle, cmd, 1, &bytesWritten);

  // Enable divide-by-5 clocking (mirrors SPI_TurnOnDivideByFiveClockingHiSpeedDevice)
  // 60 MHz / 5 / ((divisor + 1) * 2) = 6 MHz at divisor=0
  cmd[0] = 0x8B;
  FT_Write(ftHandle, cmd, 1, &bytesWritten);

  // Set clock divisor = MAX_FREQ_CLOCK_DIVISOR (0)
  cmd[0] = 0x86;
  cmd[1] = (MAX_FREQ_CLOCK_DIVISOR) & 0xFF;
  cmd[2] = ((MAX_FREQ_CLOCK_DIVISOR) >> 8) & 0xFF;
  FT_Write(ftHandle, cmd, 3, &bytesWritten);

  // Disable divide-by-5 clocking (mirrors SPI_TurnOffDivideByFiveClockingHiSpeedDevice)
  // 60 MHz / ((divisor + 1) * 2) = 30 MHz at divisor=0
  cmd[0] = 0x8A;
  FT_Write(ftHandle, cmd, 1, &bytesWritten);

  // Set clock divisor again with divide-by-5 off
  cmd[0] = 0x86;
  cmd[1] = (MAX_FREQ_CLOCK_DIVISOR) & 0xFF;
  cmd[2] = ((MAX_FREQ_CLOCK_DIVISOR) >> 8) & 0xFF;
  FT_Write(ftHandle, cmd, 3, &bytesWritten);

  // Configure low byte (ADBUS) pins:
  //   ADBUS0 = SK  (output)
  //   ADBUS1 = DO  (output)
  //   ADBUS2 = DI  (input)
  //   ADBUS3 = CS  (output, deasserted high)
  //   ADBUS4-7 = GPIOL1-4 (output, low)
  // Direction byte: 0xFB = 1111_1011 (all output except DI/ADBUS2)
  dwLowPinsValue = 0x08;  // CS deasserted (high), SK/DO/GPIOL1-4 low
  cmd[0] = SET_LOW_BYTE_DATA_BITS_CMD;  // 0x80
  cmd[1] = dwLowPinsValue;
  cmd[2] = 0xFB;
  FT_Write(ftHandle, cmd, 3, &bytesWritten);

  // Configure high byte (ACBUS) pins:
  //   ACBUS0 (Pin1) = output, low
  //   ACBUS1 (Pin2) = output, high
  //   ACBUS2-7    = input, low
  // Direction byte: 0x03 (ACBUS0+ACBUS1 output), value: 0x02 (ACBUS1 high)
  cmd[0] = SET_HIGH_BYTE_DATA_BITS_CMD;  // 0x82
  cmd[1] = 0x02;
  cmd[2] = 0x03;
  FT_Write(ftHandle, cmd, 3, &bytesWritten);

  return (ftHandle != NULL);
}

void SendBytesToDevice( void )
{
  FT_STATUS Status = FT_OK;
  DWORD dwNumDataBytesToSend = 0;
  DWORD dwNumBytesSent = 0;
  DWORD dwTotalNumBytesSent = 0;

  if (dwNumBytesToSend > MAX_NUM_BYTES_USB_WRITE)
  {
    do
    {
      // 25/08/05 - Can only use 4096 byte block as Windows 2000 Professional does not allow you to alter the USB buffer size
      // 25/08/05 - Windows 2000 Professional always sets the USB buffer size to 4K ie 4096
      if ((dwTotalNumBytesSent + MAX_NUM_BYTES_USB_WRITE) <= dwNumBytesToSend)
        dwNumDataBytesToSend = MAX_NUM_BYTES_USB_WRITE;
      else
        dwNumDataBytesToSend = (dwNumBytesToSend - dwTotalNumBytesSent);

      // This function sends data to a FT2232C dual type device. The dwNumBytesToSend variable specifies the number of
      // bytes in the output buffer to be sent to a FT2232C dual type device. The dwNumBytesSent variable contains
      // the actual number of bytes sent to a FT2232C dual type device.
      Status = FT_Write(ftHandle, &byOutputBuffer[dwTotalNumBytesSent], dwNumDataBytesToSend, &dwNumBytesSent);

      dwTotalNumBytesSent = dwTotalNumBytesSent + dwNumBytesSent;
    }
    while ((dwTotalNumBytesSent < dwNumBytesToSend) && (Status == FT_OK)); 
  }
  else
  {
    // This function sends data to a FT2232C dual type device. The dwNumBytesToSend variable specifies the number of
    // bytes in the output buffer to be sent to a FT2232C dual type device. The dwNumBytesSent variable contains
    // the actual number of bytes sent to a FT2232C dual type device.

    Status = FT_Write(ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
  }

  dwNumBytesToSend = 0;

}

void ClearOutputBuffer(void)
{
  dwNumBytesToSend = 0;
}

void AddByteToOutputBuffer( BYTE DataByte, bool bClearOutputBuffer )
{
	if( bClearOutputBuffer )
		dwNumBytesToSend = 0;

	// Guard the (previously unchecked) staging buffer. With a correctly sized
	// OUTPUT_BUFFER_SIZE this never trips; it turns a batch sized too large for
	// the buffer into a dropped byte instead of a heap overrun.
	if( dwNumBytesToSend < OUTPUT_BUFFER_SIZE )
		byOutputBuffer[dwNumBytesToSend++] = DataByte;
}

void SetAnswerFast( void )
{
	AddByteToOutputBuffer( SEND_ANSWER_BACK_IMMEDIATELY_CMD, false );	
}

void GetDataFromDevice(unsigned int dwNumBytesToRead, unsigned char ReadDataBuffer[] )
{
	DWORD dwNumBytesRead = 0;
//	DWORD dwNumBytesToRead = dwNumDataBitsToRead/8;	
	DWORD dwBytesReadIndex = 0;

	int try_count = 10;
	
	do {
		FT_Read(ftHandle, &ReadDataBuffer[dwBytesReadIndex], dwNumBytesToRead, &dwNumBytesRead);
		dwBytesReadIndex += dwNumBytesRead;
		dwNumBytesToRead -= dwNumBytesRead;
	} while( dwNumBytesToRead > 0 && try_count-- > 0 );

	if( try_count <= 0 )
		throw "ERROR: NO DATA FROM DEVICE";
	
	
}
void DisableSPIChip( void )
{
	AddByteToOutputBuffer(SET_LOW_BYTE_DATA_BITS_CMD, false);
	dwLowPinsValue = (dwLowPinsValue | CHIP_SELECT_PIN); // set CS to high
	// set SK, DO, CS and GPIOL1-4 as output, set D1 as input
	AddByteToOutputBuffer(dwLowPinsValue, FALSE);
	AddByteToOutputBuffer('\xFB', false);
}

void EnableSPIChip( void )
{
	AddByteToOutputBuffer(SET_LOW_BYTE_DATA_BITS_CMD, false);
	dwLowPinsValue = (dwLowPinsValue & ~CHIP_SELECT_PIN); // set CS to low
	// set SK, DO, CS and GPIOL1-4 as output, set D1 as input
	AddByteToOutputBuffer(dwLowPinsValue, FALSE);
	AddByteToOutputBuffer('\xFB', false);
}

void AddWriteOutBuffer( DWORD dwNumControlBitsToWrite, unsigned char pWriteControlBuffer[] )
{
  DWORD dwModNumControlBitsToWrite = 0;
  DWORD dwControlBufferIndex = 0;
  DWORD dwNumControlBytes = 0;
  DWORD dwNumRemainingControlBits = 0;
  DWORD dwModNumDataBitsToWrite = 0;
  DWORD dwDataBufferIndex = 0;
  DWORD dwNumDataBytes = 0;
  DWORD dwNumRemainingDataBits = 0;

  // kra - 040608, added test for number of control bits to write, because for SPI only, the number of control
  // bits to write can be 0 on some SPI devices, before a read operation is performed
  if (dwNumControlBitsToWrite > 1)
  {
    // adjust for bit count of 1 less than no of bits
    dwModNumControlBitsToWrite = (dwNumControlBitsToWrite - 1);

    // Number of control bytes is greater than 0, only if the minimum number of control bits is 8
    dwNumControlBytes = (dwModNumControlBitsToWrite / 8);

    if (dwNumControlBytes > 0)
    {
      // Number of whole bytes
      dwNumControlBytes = (dwNumControlBytes - 1);

      // clk data bytes out
      AddByteToOutputBuffer(CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
      AddByteToOutputBuffer((dwNumControlBytes & '\xFF'), FALSE);
      AddByteToOutputBuffer(((dwNumControlBytes / 256) & '\xFF'), FALSE);

      // now add the data bytes to go out
      do
      {
        AddByteToOutputBuffer( pWriteControlBuffer[dwControlBufferIndex], FALSE);
        dwControlBufferIndex = (dwControlBufferIndex + 1);
      }
      while (dwControlBufferIndex < (dwNumControlBytes + 1));
    }

    dwNumRemainingControlBits = (dwModNumControlBitsToWrite % 8);

    // do remaining bits
    if (dwNumRemainingControlBits > 0)
    {
      // clk data bits out
      //*lpdwDataWriteBytesCommand = CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD;
      //*lpdwDataWriteBitsCommand = CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD;
      AddByteToOutputBuffer(CLK_DATA_BITS_OUT_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
      AddByteToOutputBuffer((dwNumRemainingControlBits & '\xFF'), FALSE);
      AddByteToOutputBuffer( pWriteControlBuffer[dwControlBufferIndex], FALSE);
    }
  }
}


void AddReadOutBuffer( DWORD dwNumDataBitsToRead )
{
  DWORD dwModNumBitsToRead = 0;
  DWORD dwNumDataBytes = 0;
  DWORD dwNumRemainingDataBits = 0;

  // adjust for bit count of 1 less than no of bits
  dwModNumBitsToRead = (dwNumDataBitsToRead - 1);

  dwNumDataBytes = (dwModNumBitsToRead / 8);

  if (dwNumDataBytes > 0)
  {
    // Number of whole bytes
    dwNumDataBytes = (dwNumDataBytes - 1);

    // clk data bytes out
    AddByteToOutputBuffer(CLK_DATA_BYTES_IN_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
    AddByteToOutputBuffer((dwNumDataBytes & '\xFF'), FALSE);
    AddByteToOutputBuffer(((dwNumDataBytes / 256) & '\xFF'), FALSE);
  }

  // number of remaining bits
  dwNumRemainingDataBits = (dwModNumBitsToRead % 8);

  if (dwNumRemainingDataBits > 0)
  {
    // clk data bits out
    AddByteToOutputBuffer(CLK_DATA_BITS_IN_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
    AddByteToOutputBuffer((dwNumRemainingDataBits & '\xFF'), FALSE);
  }
}

void spi_SetCS( bool ChipSelect )
{
	dwNumBytesToSend = 0; // Index to the output buffer
	dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	dwNumBytesToRead = 0; // Number of bytes available to read

	byOutputBuffer[dwNumBytesToSend++] = 0x80;
	dwLowPinsValue &= ~0x08;
	dwLowPinsValue |= ChipSelect ? 0x08 : 0x00;
	byOutputBuffer[dwNumBytesToSend++] = dwLowPinsValue;
	byOutputBuffer[dwNumBytesToSend++] = 0x3E; // byDirection
	FT_STATUS ftStatus = FT_Write( ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
//	if(ftStatus == FT_OK)
//		while(dwNumBytesSent != dwNumBytesToSend)
//			printf("Sending byte %d\n", dwNumBytesSent);

	dwNumBytesToSend = 0;
	dwNumBytesToRead = 0;	
}

void spi_setGPIO( bool XXLo, bool EJLo )
{
	dwNumBytesToSend = 0; // Index to the output buffer
	dwNumBytesSent = 0; // Count of actual bytes sent - used with FT_Write
	dwNumBytesToRead = 0; // Number of bytes available to read

	byOutputBuffer[dwNumBytesToSend++] = 0x80;
	dwLowPinsValue &= ~0x30;
	dwLowPinsValue |= (XXLo ? 0x10 : 0x00) | (EJLo ? 0x20 : 0x00);
	byOutputBuffer[dwNumBytesToSend++] = dwLowPinsValue; 
	byOutputBuffer[dwNumBytesToSend++] = 0x3E; // byDirection
	FT_STATUS ftStatus = FT_Write( ftHandle, byOutputBuffer, dwNumBytesToSend, &dwNumBytesSent);
//	if(ftStatus == FT_OK)
//		while(dwNumBytesSent != dwNumBytesToSend)
//			printf("Sending byte %d\n", dwNumBytesSent);

	dwNumBytesToSend = 0;
	dwNumBytesToRead = 0;	
}

void spi_QueueClockDelay( unsigned int numBytes )
{
	if (numBytes == 0)
		return;

	unsigned int n = numBytes - 1;
	AddByteToOutputBuffer(CLK_DATA_BYTES_OUT_ON_NEG_CLK_LSB_FIRST_CMD, FALSE);
	AddByteToOutputBuffer((BYTE)(n & 0xFF), FALSE);
	AddByteToOutputBuffer((BYTE)((n >> 8) & 0xFF), FALSE);
	for (unsigned int i = 0; i < numBytes; i++)
		AddByteToOutputBuffer((BYTE)0x00, FALSE);
}

void closeDevice() {
    if (ftHandle != NULL) {
        // Deassert CS, then reset from MPSSE back to serial mode
        FT_SetBitMode(ftHandle, 0x00, 0x00);

        // Flush any remaining data in the USB buffers
        FT_Purge(ftHandle, FT_PURGE_RX | FT_PURGE_TX);

        FT_Close(ftHandle);
        ftHandle = NULL;
    }
}

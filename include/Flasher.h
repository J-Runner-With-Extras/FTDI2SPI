/*!
*
*/

#ifndef FLASHER_H
#define FLASHER_H

#include <stdint.h>
#include "sfc.h"
extern bool g_is_emmc;
void FlashReadStatus(unsigned char Status[]);
void FlashDataInit(unsigned char FlashConfig[]);
void FlashDataDeInit(void);
void PowerUp(void);
void Shutdown(void);
void Update(void);
void FlashDataErase( unsigned int reg );
void FlashDataRead( unsigned char * pData, unsigned int Arg, unsigned int Words );
void FlashDataWrite( unsigned char Data[], unsigned int Arg, unsigned int Words );
void fixSpare_ECC(unsigned char *blockdata, uint32_t block_num );
void fixSB(unsigned char *blockdata);

// ---------------------------------------------------------------------------
// Fast batched NAND read (optimization)
// Reads NAND_READ_BATCH_PAGES physical pages per USB round-trip instead of one
// page per ~3 round-trips. Comment out NAND_FAST_READ to restore the original
// per-page read path byte-for-byte.
//
//   NAND_READ_BATCH_PAGES : pages per USB transaction. The D2XX driver continuously
//                           drains the chip RX FIFO (MPSSE fills it at ~1 MB/s, the
//                           driver empties it far faster), so this is bounded by the
//                           host command buffer (OUTPUT_BUFFER_SIZE), not the ~4 KB
//                           chip FIFO. Bigger = fewer per-batch stalls. 32 lands
//                           ~20-22 s; try 64 for ~18-19 s. Drop to 7 if a setup ever
//                           shows instability. 0x210-page commands ~= 4.9 KB each, so
//                           keep nPages * 4.9 KB below OUTPUT_BUFFER_SIZE (512 KB).
//   NAND_READ_DELAY_BYTES : SPI clocks (in bytes) inserted after each page-read
//                           command to allow the NAND read time (tR), replacing
//                           the per-page busy-bit poll. ~375 bytes ~= 100 us at
//                           30 MHz, already less than the old poll's round-trip
//                           wait. Tune down toward the real tR once a CRC-verified
//                           dump confirms integrity.
// ---------------------------------------------------------------------------
#define NAND_FAST_READ
#define NAND_READ_BATCH_PAGES   32
#define NAND_READ_DELAY_BYTES   375

void FlashDataReadBatch(unsigned char* pData, unsigned int Block, unsigned int nPages, unsigned int bytesPerPage);

#endif
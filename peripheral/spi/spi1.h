/* 
 * File:   spi1.h
 * Author: thaus
 *
 * Created on 2014. sijecanj 06, 13:30
 */

#ifndef SPI1_H
#define	SPI1_H

#include <p33Fxxxx.h>
#include <Generic.h>
#include "../gpio/digitalIO.h"

#define chipSelect(slave) digitalLow(slave)
#define chipDeselect(slave) digitalHigh(slave)
UINT8 spi1Init(UINT8 mode, UINT8 int_en);
UINT8 spi1Write(UINT16 data);
UINT8 spi1TransferWord(UINT16 out, UINT16 *in);
UINT8 spi1TransferBuff(UINT16 *buff, UINT16 len);
UINT8 spi1TxBuffFull();

#endif	/* SPI1_H */

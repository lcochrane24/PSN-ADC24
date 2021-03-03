/* sdrint.c */

// Host Xmit Data Interrupt
void __attribute__((__interrupt__, no_auto_psv)) _U1TXInterrupt(void)
{
	if( txOutCount )  {
		U1TXREG = *txOutPtr++;
		--txOutCount;
	}
	else if( gpsOutSendLen ) {
		txOutCount = gpsOutSendLen;
		txOutPtr = gpsOutSendPtr;
		gpsOutSendLen = 0;
		U1TXREG = *txOutPtr++;
		--txOutCount;
	}
	else
	    IEC0bits.U1TXIE = 0;
	IFS0bits.U1TXIF = 0;
}

// GPS Xmit Data Interrupt
void __attribute__((__interrupt__, no_auto_psv)) _U2TXInterrupt(void)
{
	if( tx2OutCount )  {
		U2TXREG = *tx2OutPtr++;
		--tx2OutCount;
	}
	else
	    IEC1bits.U2TXIE = 0;
	IFS1bits.U2TXIF = 0;
}

// Host Receive Data Interrupt
void __attribute__((__interrupt__, no_auto_psv)) _U1RXInterrupt(void)
{
	BYTE d, err = 0;
					
	if( U1STAbits.OERR )  {
		U1STAbits.OERR = 0;
		err = 1;
	}
		
	while( U1STAbits.URXDA )  {
		if( U1STAbits.OERR )  {
			U1STAbits.OERR = 0;
			err = 1;
		}
		if( U1STAbits.FERR != 0 )  {
			d = U1RXREG;
			err = 1;
			continue;
		}
		
		d = U1RXREG;
		if( !hdrState )  {
			if( d == 0x02 )  {
				inPtrQue = inBuffQueue;
				commTimer = 0;
				inSum = 0;
				hdrState = 1;
			}
			continue;
		}
		if( hdrState == 1 )  {
			inSum ^= d;
			*inPtrQue++ = d;
			if( d == 'C' )
				inPacketLen = inDataLen = sizeof( Sdr24ConfigInfo ) + 2;
			else if( d == 'T' || d == 'D' )  {
				inPacketLen = inDataLen = sizeof( TimeInfo ) + 2;
				timeStamp = timeTick;
			}
			else if( d == 'd' )
				inPacketLen = inDataLen = sizeof( TimeDiffInfo ) + 2;
			else if( d == 'V' )
				inPacketLen = inDataLen = sizeof( VcoInfo ) + 2;
			else
				inPacketLen = inDataLen = 2;
			hdrState = 2;
			continue;
		}
		if( hdrState >= 2 )  {
			if( inDataLen >= 3 )
				inSum ^= d;
			*inPtrQue++ = d;
			--inDataLen;
			if( !inDataLen )  {
				hdrState = 0;
				if( d != 0x03 )  {
					strcpy( logStr, "Bad Incoming Packet");
					sendLog = 1;
					continue;
				}
				if( inSum == inBuffQueue[ inPacketLen-1 ] )  {
					memcpy( inBuffer, inBuffQueue, inPacketLen );
					newCommandFlag = TRUE;
					checkErr = 0;
				}
				else  {
					checkErr = 1;
					err = 1;
				}
			}
		}
	}
		
	if( err )  {
		strcpy( logStr, "Err in Rvr Int");
		sendLog = 1;
	}
	
	IFS0bits.U1RXIF = 0;
}

// GPS Receive Data Interrupt
void __attribute__((__interrupt__, no_auto_psv)) _U2RXInterrupt(void)
{
	int d;
	
	if( U2STAbits.OERR )
		U2STAbits.OERR = 0;
					
	while( U2STAbits.URXDA )  {
		if( U2STAbits.OERR )
			U2STAbits.OERR = 0;

		if( U2STAbits.FERR != 0 )  {
			d = U2RXREG;
			continue;
		}
		
		gpsInQueue[ gpsQueueInCount ] = U2RXREG;
		if( ++gpsQueueInCount >= GPS_IN_QUEUE )
			gpsQueueInCount = 0;
	}
	
	IFS1bits.U2RXIF = 0;
}
			
// Timer Interrupt
void __attribute__((__interrupt__, no_auto_psv)) _T1Interrupt(void)
{
//	TEST_PIN = 1;
	
	if( ++timeTick >= TICKS_PER_DAY )	// test for new day
		timeTick = 0;
	
	if( ledPPS )  {
		--secTick;
		if( !secTick )  {
			secTick = secTickValue;
			LED_PIN = 1;
		}
		else if(secTick < secOffValue )
			LED_PIN = 0;
	}
	
	if( timeRefType )  {
		if( ppsHiToLowFlag )  {
			if( ppsLowHiFlag && PPS_PIN )  {
				ppsTick = timeTick;
				ppsLowHiFlag = 0;
				gpsInCount = 0;
				needTimeFlag = 1;
			}	
			else if( !PPS_PIN )  {
				ppsLowHiFlag = 1;
			}
		}
		else  {
			if( ppsLowHiFlag && !PPS_PIN )  {
				ppsTick = timeTick;
				ppsLowHiFlag = 0;
				gpsInCount = 0;
				needTimeFlag = 1;
			}	
			else if( PPS_PIN )  {
				ppsLowHiFlag = 1;
			}
		}
	}
 
	--vcoTime;
	if( !vcoTime )  {
		if( vcoPinOn )  {
			vcoPinOn = 0;
			if( newVcoOffTime )  {
				vcoTime = vcoOffTime = newVcoOffTime;
				newVcoOffTime = 0;
			}
			else  {
				vcoTime = vcoOffTime;
			}
		}
		else  {
			vcoPinOn = 1;
			if( newVcoOnTime )  {
				vcoTime = vcoOnTime = newVcoOnTime;
				newVcoOnTime = 0;
			}
			else  {
				vcoTime = vcoOnTime;
			}
		}
		VCO_OUT = vcoPinOn;
	}

	++gpsTimer;
		
//	TEST_PIN = 0;
	IFS0bits.T1IF = 0;		/* clear interrupt flag */
}

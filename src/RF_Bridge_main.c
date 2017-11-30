//=========================================================
// src/RF_Bridge_2_main.c: generated by Hardware Configurator
//
// This file will be updated when saving a document.
// leave the sections inside the "$[...]" comment tags alone
// or they will be overwritten!!
//=========================================================

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <SI_EFM8BB1_Register_Enums.h>                  // SFR declarations
#include "Globals.h"
#include "InitDevice.h"
#include "uart_0.h"
#include "pca_0.h"
#include "uart.h"
#include "RF_Handling.h"
// $[Generated Includes]
// [Generated Includes]$

uart_state_t uart_state = IDLE;
uart_command_t uart_command = NONE;
bool Sniffing = false;

//-----------------------------------------------------------------------------
// SiLabs_Startup() Routine
// ----------------------------------------------------------------------------
// This function is called immediately after reset, before the initialization
// code is run in SILABS_STARTUP.A51 (which runs before main() ). This is a
// useful place to disable the watchdog timer, which is enable by default
// and may trigger before main() in some instances.
//-----------------------------------------------------------------------------
void SiLabs_Startup (void)
{

}

//-----------------------------------------------------------------------------
// main() Routine
// ----------------------------------------------------------------------------
int main (void)
{
	// Call hardware initialization routine
	enter_DefaultMode_from_RESET();

	// enter default state
	LED = LED_OFF;
	BUZZER = BUZZER_OFF;

	T_DATA = 1;

	// enable UART
	UART0_init(UART0_RX_ENABLE, UART0_WIDTH_8, UART0_MULTIPROC_DISABLE);

	// start sniffing if enabled by default
	if (Sniffing)
		PCA0_DoSniffing();
	else
		PCA0_StopSniffing();

	// enable global interrupts
	IE_EA = 1;

	while (1)
	{
		/*------------------------------------------
		 * check if something got received by UART
		 ------------------------------------------*/
		unsigned int rxdata;
		uint8_t len;
		uint8_t position;

		rxdata = uart_getc();

		if (rxdata != UART_NO_DATA)
		{
			// state machine for UART
			switch(uart_state)
			{
				// check if UART_SYNC_INIT got received
				case IDLE:
					if ((rxdata & 0xFF) == UART_SYNC_INIT)
						uart_state = SYNC_INIT;
					break;

				// sync byte got received, read command
				case SYNC_INIT:
					uart_command = rxdata & 0xFF;
					uart_state = SYNC_FINISH;

					// check if some data needs to be received
					switch(uart_command)
					{
						case LEARNING:
							Timer_3_Timeout = 50000;
							BUZZER = BUZZER_ON;
							// start 5�s timer
							TMR3CN0 |= TMR3CN0_TR3__RUN;
							// wait until timer has finished
							while((TMR3CN0 & TMR3CN0_TR3__BMASK) == TMR3CN0_TR3__RUN);
							BUZZER = BUZZER_OFF;
							break;
						case SNIFFING_ON:
							PCA0_DoSniffing();
							break;
						case SNIFFING_OFF:
							PCA0_StopSniffing();
							sniffing_is_on = false;
							break;
						case TRANSMIT_DATA:
							uart_state = RECEIVE_LEN;
							break;


						// unknown command
						default:
							uart_command = NONE;
							uart_state = IDLE;
							break;
					}
					break;

				// Receiving UART data len
				case RECEIVE_LEN:
					position = 0;
					len = rxdata & 0xFF;
					if (len > 0)
						uart_state = RECEIVING;
					else
						uart_state = SYNC_FINISH;
					break;

				// Receiving UART data
				case RECEIVING:
					RF_DATA[position] = rxdata & 0xFF;
					position++;

					if (position == len)
						uart_state = SYNC_FINISH;
					break;

				// wait and check for UART_SYNC_END
				case SYNC_FINISH:
					if ((rxdata & 0xFF) == UART_SYNC_END)
					{
						uart_state = IDLE;
						// send acknowledge
						uart_put_command(COMMAND_AK);
					}
					break;
			}
		}

		/*------------------------------------------
		 * check command byte
		 ------------------------------------------*/
		switch(uart_command)
		{
			case SNIFFING_ON:
				// check if a RF signal got decoded
				if ((RF_DATA_STATUS & RF_DATA_RECEIVED_MASK) != 0)
				{
					uint8_t used_protocol = RF_DATA_STATUS & 0x7F;
					uart_put_RF_Data(SNIFFING_ON, used_protocol);

					// clear RF status
					RF_DATA_STATUS = 0;
				}
				break;

			// transmit data on RF
			// byte 0:		Protocol identifier
			// byte 1..N:	data to be transmitted
			case TRANSMIT_DATA:
				// only do the job if all data got received by UART
				if (uart_state != IDLE)
					break;

				if (sniffing_is_on)
					PCA0_StopSniffing();

				protocol_index = PCA0_DoTransmit(RF_DATA[0]);

				if (protocol_index != 0xFF)
				{
					actual_bit_of_byte = 0x08;
					actual_bit_of_byte--;
					actual_byte = 1;
					actual_bit = 1;

					if(((RF_DATA[actual_byte] >> actual_bit_of_byte) & 0x01) == 0x01)
					{
						// bit 1
						PCA0_writeChannel(PCA0_CHAN0, DUTY_CYCLE_HIGH << 8);
					}
					else
					{
						// bit 0
						PCA0_writeChannel(PCA0_CHAN0, DUTY_CYLCE_LOW << 8);
					}

					SendRF_SYNC(protocol_index);
					PCA0CN0_CR = PCA0CN0_CR__RUN;
				}

				uart_command = NONE;
				break;
		}
	}
}

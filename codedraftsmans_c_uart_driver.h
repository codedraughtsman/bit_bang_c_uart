#ifndef __CODEDRAFTSMANS_C_UART_DRIVER_H__
#define __CODEDRAFTSMANS_C_UART_DRIVER_H__

#include<stdint.h>

struct uart_dev {
	int32_t countTillNextReadOfRX;
	int (*read_rx_pin) (void);
	//set_tx_pin
	uint32_t oversamplingRate;
	uint16_t rx_current_frame;
	uint32_t rx_current_frame_index;
	uint8_t rx_is_setup;
	uint8_t rx_max_frame_index;

	uint8_t rx_buffer[200];
	uint32_t rx_buffer_current_index;
	uint32_t rx_pin_has_been_consecutively_high_for_the_last_n_samples;

	uint32_t numberOfStopBits;
	uint32_t hasParityBit;

};

struct uart_dev create_uart(uint32_t oversamplingRate, int (*read_rx_pin) (void));
uint32_t rxFrameSize(struct uart_dev *dev);

#endif

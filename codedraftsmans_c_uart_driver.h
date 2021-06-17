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

	void (*receivedCharHandler) (uint8_t);
	uint32_t rx_pin_has_been_consecutively_high_for_the_last_n_samples;

	uint32_t number_of_stop_bits;
	uint32_t hasParityBit;

};

struct uart_dev uart_rx_init(uint32_t oversamplingRate, int (*read_rx_pin) (void), void (*receivedCharHandler) (uint8_t));
uint32_t uart_rx_frame_size(struct uart_dev *dev);

#endif

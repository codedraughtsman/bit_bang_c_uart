#ifndef __CODEDRAFTSMANS_C_UART_DRIVER_H__
#define __CODEDRAFTSMANS_C_UART_DRIVER_H__

#include<stdint.h>

struct uart_rx_dev {
	int32_t count_till_next_read;
	int (*read_rx_pin) (void);
	//set_tx_pin
	uint32_t oversampling_rate;
	uint16_t rx_current_frame;
	uint32_t rx_current_frame_index;
	uint8_t rx_is_setup;

	void (*received_data_handler) (uint8_t, int32_t);
	uint32_t rx_pin_has_been_high_for;

	uint32_t number_of_stop_bits;
	uint32_t has_parity_bit;

};

void uart_rx_init(struct uart_rx_dev *dev, uint32_t oversampling_rate, uint32_t (*read_rx_pin) (void), void (*received_data_handler) (uint8_t, int32_t));
uint32_t uart_rx_frame_size(struct uart_rx_dev *dev);
int32_t rxInterruptHandler(struct uart_rx_dev *dev);

#endif

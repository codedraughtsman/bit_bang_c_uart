#include "codedraftsmans_c_uart_driver.h"
#include <stdio.h>

void rxInterruptHandler(struct uart_rx_dev *dev) ;

struct uart_rx_dev uart_rx_init(uint32_t oversampling_rate, int (*read_rx_pin) (void), void (*received_data_handler) (uint8_t)){
	struct uart_rx_dev dev;
	dev.count_till_next_read = 0;

	dev.rx_is_setup = 0;
	//last pin needs to be set to 1 before it can be used for the
	dev.rx_pin_has_been_high_for = 0;

	dev.rx_current_frame = 0;
	dev.rx_current_frame_index = 0;
	dev.rx_max_frame_index = 10; //todo calculate this based off of settings passed in by the user.

	dev.oversampling_rate = oversampling_rate;

	dev.read_rx_pin = read_rx_pin;

	dev.number_of_stop_bits = 1;
	dev.has_parity_bit= 0;
	dev.received_data_handler= received_data_handler;


	return dev;
}

uint32_t uart_rx_frame_size(struct uart_rx_dev *dev){
	return dev->number_of_stop_bits + 8 + 1 + dev->has_parity_bit;
	//stop bits + data bits + start bits + parityBit.
}

void add_bit_to_rx_frame_buffer(struct uart_rx_dev *dev, uint32_t bit){
	dev->rx_current_frame |= bit << dev->rx_current_frame_index;
	dev->rx_current_frame_index ++;
}

uint32_t rx_frame_buffer_is_complete(struct uart_rx_dev *dev){
	return dev->rx_current_frame_index >= dev->number_of_stop_bits + dev->has_parity_bit + 8+ 1;
}

void rxSyncTiming(struct uart_rx_dev *dev, uint32_t pin_val){

	if (!dev->rx_is_setup && pin_val == 0 && dev->rx_pin_has_been_high_for > 0){
		//the signal has started.

		//Todo really need to check that this is the start of the signal.
		//it could be part way through a sent frame, which will cause all kinds of errors.


		//this value will cause the interrupt handler to read the pin on the next interrupt.
		dev->count_till_next_read = (dev->oversampling_rate+1) /2;

		//don't do the setup again.
		dev->rx_is_setup = 1;

	}

	if (pin_val){
		dev->rx_pin_has_been_high_for ++;
	} else {
		dev->rx_pin_has_been_high_for = 0;
	}


	if (dev->rx_pin_has_been_high_for/dev->oversampling_rate
			>= dev->rx_max_frame_index){
		//we have had enough time pass with no data being sent to know that the next time the rx pin goes low it is the start bit.
		dev->rx_is_setup = 0;
	}
}

uint8_t getRxFrameBufferData(struct uart_rx_dev *dev){
	//todo does it needs the 0xff? it is probably already being converted/truncated to 8 bits by
	//the return type. There is no harm in leaving in the 0xff for now.
	return (dev->rx_current_frame >> 1) & 0xff;
}

void reset_rx_frame_buffer(struct uart_rx_dev *dev){
	dev->rx_current_frame = 0;
	dev->rx_current_frame_index =0;
}

void rxInterruptHandler(struct uart_rx_dev *dev) {

	//first read the pin, then pass it to all the functions that need it.
	//this is to prevent the pin's value from possibly changing as this function
	//is running.
	uint32_t pin_val = dev->read_rx_pin();
	//we want the user to only use the first bit.
	//if any of the other bits are set then we need to throw an error
	//to let the user know to fix their code.
	if (pin_val > 1){
		printf("Error: pin val is more than a single bit\n");
		//print an error message.
		//then throw an error to make it obvious.
		return;
	}


	rxSyncTiming(dev, pin_val);


	dev->count_till_next_read --;
	if (!dev->rx_is_setup || dev->count_till_next_read > 0) {
		return;
	}

	dev->count_till_next_read = dev->oversampling_rate;
	//add bit to message buffer
	add_bit_to_rx_frame_buffer(dev, pin_val);

	if (!rx_frame_buffer_is_complete(dev)){
		//still need more bits before this frame is completed.
		return;
	}

	//frame is completed, call the handler function
	if (dev->received_data_handler == 0) {
		//the callback function is not set. Do nothing
		//todo Alert the user.

	} else {
		dev->received_data_handler(getRxFrameBufferData(dev));
	}
	reset_rx_frame_buffer(dev);

}

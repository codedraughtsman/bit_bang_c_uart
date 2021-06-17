#include "codedraftsmans_c_uart_driver.h"
#include <stdio.h>

void rxInterruptHandler(struct uart_dev *dev) ;

struct uart_dev create_uart(uint32_t oversamplingRate, int (*read_rx_pin) (void), void (*receivedCharHandler) (uint8_t)){
	struct uart_dev dev;
	dev.countTillNextReadOfRX = 0;

	dev.rx_is_setup = 0;
	//last pin needs to be set to 1 before it can be used for the
	dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples = 0;

	dev.rx_current_frame = 0;
	dev.rx_current_frame_index = 0;
	dev.rx_max_frame_index = 10; //todo calculate this based off of settings passed in by the user.

	dev.oversamplingRate = oversamplingRate;

	dev.read_rx_pin = read_rx_pin;

	dev.numberOfStopBits = 1;
	dev.hasParityBit= 0;
	dev.receivedCharHandler= receivedCharHandler;


	return dev;
}

uint32_t rxFrameSize(struct uart_dev *dev){
	return dev->numberOfStopBits + 8 + 1 + dev->hasParityBit;
	//stop bits + data bits + start bits + parityBit.
}

void pollingInterruptHandler(struct uart_dev *dev) {
	//first read the incoming data.
	rxInterruptHandler(dev);
}

void addBitToRxFrameBuffer(struct uart_dev *dev, uint32_t bit){
	dev->rx_current_frame |= bit << dev->rx_current_frame_index;
	dev->rx_current_frame_index ++;
}

uint32_t RxFrameBufferIsComplete(struct uart_dev *dev){
	return dev->rx_current_frame_index >= dev->numberOfStopBits + dev->hasParityBit + 8+ 1;
}

void rxSyncTiming(struct uart_dev *dev, uint32_t pin_val){



	if (!dev->rx_is_setup && pin_val == 0 && dev->rx_pin_has_been_consecutively_high_for_the_last_n_samples > 0){
		//the signal has started.

		//Todo really need to check that this is the start of the signal.
		//it could be part way through a sent frame, which will cause all kinds of errors.


		//this value will cause the interrupt handler to read the pin on the next interrupt.
		dev->countTillNextReadOfRX = (dev->oversamplingRate+1) /2;

		//don't do the setup again.
		dev->rx_is_setup = 1;

	}

	if (pin_val){
		dev->rx_pin_has_been_consecutively_high_for_the_last_n_samples ++;
	} else {
		dev->rx_pin_has_been_consecutively_high_for_the_last_n_samples = 0;
	}


	if (dev->rx_pin_has_been_consecutively_high_for_the_last_n_samples/dev->oversamplingRate
			>= dev->rx_max_frame_index){
		//we have had enough time pass with no data being sent to know that the next time the rx pin goes low it is the start bit.
		dev->rx_is_setup = 0;
	}
}

uint8_t getRxFrameBufferData(struct uart_dev *dev){
	//todo does it needs the 0xff? it is probably already being converted/truncated to 8 bits by
	//the return type. There is no harm in leaving in the 0xff for now.
	return (dev->rx_current_frame >> 1) & 0xff;
}

void resetRxFrameBuffer(struct uart_dev *dev){
	dev->rx_current_frame = 0;
	dev->rx_current_frame_index =0;
}

void rxInterruptHandler(struct uart_dev *dev) {

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


	dev->countTillNextReadOfRX --;
//	printf("dev->countTillNextReadOfRX is %d, dev->rx_is_setup is %d, pin val %d\n",dev->countTillNextReadOfRX,dev->rx_is_setup,pin_val);
	if (!dev->rx_is_setup || dev->countTillNextReadOfRX > 0) {
		return;
	}
//	printf("now adding bit %d to frame\n", pin_val);
	dev->countTillNextReadOfRX = dev->oversamplingRate;
	//add bit to message buffer
	addBitToRxFrameBuffer(dev, pin_val);

	if (!RxFrameBufferIsComplete(dev)){
		//still need more bits before this frame is completed.
		return;
	}

	//frame is completed, call the handler function
	if (dev->receivedCharHandler == 0) {
		//the function is not set. Do nothing and alert the user.
		printf("the receivedCharHandler is null\n");
		return;
	}
	dev->receivedCharHandler(getRxFrameBufferData(dev));
	resetRxFrameBuffer(dev);


	//todo add a call back function that gets called every time a new byte is received?


}

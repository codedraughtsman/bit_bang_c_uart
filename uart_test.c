//include the .c file to get access to all the private functions.
#include "codedraftsmans_c_uart_driver.h"
#include "codedraftsmans_c_uart_driver.c"
#include <string.h>
#include <stdio.h>

#define PASS 0
#define FAIL 1

#define ASSERT_EQUAL(a, b, message) \
	if (a != b) {\
		printf("\nASSERT_EQUALS failed for %s\n",message);\
		return FAIL; \
	}

uint8_t mockedPinData[500] ={0};
uint32_t mockedPinBitsWritten =0;
uint32_t mockedPinBitsRead =0;
uint32_t mockedPinOversamplingCounter = 0;

uint32_t mockedPinCurrentFrame = 0;
uint32_t mockedPinCurrentFrameIndex = 0;
uint32_t mockedPinIndexOfByteBeingSent =0;

uint8_t rx_buffer[500];
uint32_t rx_buffer_index =0;

struct uart_rx_dev dev;



uint32_t make_frame(struct uart_rx_dev *dev, uint8_t data){
	uint32_t frame = 0;
	//the start bit is already set to zero.
	frame |= ((uint32_t) data << 1);

	//todo set parityBit.
	//set stop bits.
	for (int i=0; i< dev->number_of_stop_bits; i++){
		frame |= (1 << (9+i));
	}


	return frame;
}


void received_data_handler(uint8_t val){
	rx_buffer[rx_buffer_index++] = val;
}
void clear_rx_buffer(){
	memset(rx_buffer, 0, 500);
	rx_buffer_index = 0;
}



void mockedPinSetString(struct uart_rx_dev *dev, char *str){
	strcpy(mockedPinData, str);
	mockedPinIndexOfByteBeingSent=0;
	mockedPinCurrentFrame = make_frame(dev, mockedPinData[mockedPinIndexOfByteBeingSent]);
	mockedPinCurrentFrameIndex =0;
	mockedPinOversamplingCounter = dev->oversampling_rate;
}

int mockedPin_data(){
	if (mockedPinCurrentFrameIndex >= uart_rx_frame_size(&dev)) {
		//need to load the next byte into the frame buffer.
		mockedPinIndexOfByteBeingSent ++;
		mockedPinCurrentFrame = make_frame(&dev, mockedPinData[mockedPinIndexOfByteBeingSent]);
		mockedPinCurrentFrameIndex =0;
	}
	uint8_t val = (mockedPinCurrentFrame >> mockedPinCurrentFrameIndex)& 0x01;
	mockedPinOversamplingCounter --;
	if (mockedPinOversamplingCounter <=0){
		mockedPinCurrentFrameIndex ++;
		mockedPinOversamplingCounter = dev.oversampling_rate;
	}
	return val;
}

int mockedPin_high(){
	return 1;
}


struct uart_rx_dev commonUart(){
	return uart_rx_init(3, mockedPin_data, received_data_handler);
}


int test_add_bit_to_rx_frame_buffer(){
	dev = commonUart();

	//check that the dev has been initialized properly.
	uint32_t bits[] = {1,1,1,1,1,0,0,1,1,0};
	ASSERT_EQUAL(dev.rx_current_frame, 0, "rx_current_frame ==0");
	for (int i=0; i<10; i++){
		ASSERT_EQUAL(dev.rx_current_frame_index, i, "rx_current_frame_index==0");
		add_bit_to_rx_frame_buffer(&dev, bits[i]);
	}

	ASSERT_EQUAL(dev.rx_current_frame, 0x19f, "rx_current_frame ==0xf91");


	return PASS;

}

int test_rxSyncTiming(){
	dev = commonUart();
	ASSERT_EQUAL(dev.rx_pin_has_been_high_for, 0, "dev.rx_pin_has_been_high_for ==0")
	ASSERT_EQUAL(dev.rx_is_setup, 0, "dev.rex_is_setup == 0");

	rxSyncTiming(&dev, 1);
	ASSERT_EQUAL(dev.rx_pin_has_been_high_for, 1, "dev.rx_pin_has_been_high_for == 1")
	ASSERT_EQUAL(dev.rx_is_setup, 0, "dev.rex_is_setup == 0");

	//start the timing pulse.
	rxSyncTiming(&dev, 0);
	ASSERT_EQUAL(dev.rx_pin_has_been_high_for, 0, "dev.rx_pin_has_been_high_for == 0")
	ASSERT_EQUAL(dev.rx_is_setup, 1, "dev.rex_is_setup == 1");

	//a long period without any data. this should cause the UART to reset its timing.
	for (int i=0; i<100; i++) {
		rxSyncTiming(&dev, 1);
	}
	ASSERT_EQUAL(dev.rx_is_setup, 0, "dev.rex_is_setup == 0");

	//start the timing pulse.
	rxSyncTiming(&dev, 0);
	ASSERT_EQUAL(dev.rx_pin_has_been_high_for, 0, "dev.rx_pin_has_been_high_for == 0")
	ASSERT_EQUAL(dev.rx_is_setup, 1, "dev.rex_is_setup == 1");


	return PASS;
}

int test_getRxFrameBufferData(){
	dev = commonUart();
	dev.rx_current_frame = 0x2aa;
	uint8_t data = getRxFrameBufferData(&dev);

	ASSERT_EQUAL(data, 0x55, "data == 0x55");
	return PASS;
}

int test_reset_rx_frame_buffer(){
	dev = commonUart();
	//just using random values, the index does not match with the number of bits written to the
	//current_frame.
	dev.rx_current_frame = 0x2aa;
	dev.rx_current_frame_index = 5;

	ASSERT_EQUAL(dev.rx_current_frame, 0x2aa, "dev.rx_current_frame == 0x2aa");
	ASSERT_EQUAL(dev.rx_current_frame_index, 5, "dev.rx_current_frame_index == 5");
	reset_rx_frame_buffer(&dev);
	ASSERT_EQUAL(dev.rx_current_frame, 0, "dev.rx_current_frame == 0");
	ASSERT_EQUAL(dev.rx_current_frame_index, 0, "dev.rx_current_frame_index == 0");

	return PASS;
}




int test_rx_frame_buffer_is_complete(){
	dev = commonUart();
	uint8_t startData = 0x5a;

	for (int i=0; i<uart_rx_frame_size(&dev); i++) {
		ASSERT_EQUAL(rx_frame_buffer_is_complete(&dev), 0, "rx_frame_buffer_is_complete(&dev) == 0");
		if (i>=1 && i <9){

			add_bit_to_rx_frame_buffer(&dev, (startData >>(i-1)) & 0x01);
		} else {
			//we don't care about these bits.
			add_bit_to_rx_frame_buffer(&dev, 1);
		}

	}
	ASSERT_EQUAL(rx_frame_buffer_is_complete(&dev), 1, "rx_frame_buffer_is_complete(&dev) == 1");

	uint8_t data = getRxFrameBufferData(&dev);
	ASSERT_EQUAL(data, startData, "data == startData");

	return PASS;
}

int test_rxInterruptHandler(){
	dev = commonUart();

	//high bits to setup the syncing of the frames.
	dev.read_rx_pin = mockedPin_high;
	for (int i=0; i<12; i++) {
		rxInterruptHandler(&dev);
	}

	dev.read_rx_pin = mockedPin_data;
	int8_t *startData = "hello world";

	mockedPinSetString(&dev, startData);


	for (int i=0; i<dev.oversampling_rate * uart_rx_frame_size(&dev)* (strlen(startData)+3); i++) {
		rxInterruptHandler(&dev);
	}

	if (strcmp(rx_buffer, startData)!=0){
		//strings don't match.
		printf("test_rxInterruptHandler, the strings don't match\n");
		printf("original string '%s', received string '%s'\n",startData, rx_buffer);
		return FAIL;

	}


	return PASS;
}
int set_rx_current_frame_data(struct uart_rx_dev *dev, uint8_t newData){
	dev->rx_current_frame = ((uint32_t)newData) << 1;
	ASSERT_EQUAL((dev->rx_current_frame >>1), newData, "set_rx_current_frame_data: dev->rx_current_frame == newData");
}

//int test_moveRxFrameDataToBuffer(){
//	dev = commonUart();
//
//	//todo do something with this return code if the setting fails.
//	set_rx_current_frame_data(&dev, 'a');
//
//	ASSERT_EQUAL(rx_buffer[0], 0, "rx_buffer[0] == 0");
//	moveRxFrameDataToBuffer(&dev);
//	ASSERT_EQUAL(rx_buffer[0], 'a', "rx_buffer[0] == a");
//	set_rx_current_frame_data(&dev, 'b');
//	moveRxFrameDataToBuffer(&dev);
//
//	//check to see if it has changed the first item in the buffer.
//	ASSERT_EQUAL(rx_buffer[0], 'a', "rx_buffer[0] == a");
//	ASSERT_EQUAL(rx_buffer[1], 'b', "rx_buffer[1] == b");
//
//	return PASS;
//}

struct test_struct {
	uint8_t *name;
	int (*test_fn) (void);
};

int main(){
	uint32_t numberOfTestsFailed = 0;
	struct test_struct tests[] ={
			{"test_add_bit_to_rx_frame_buffer", test_add_bit_to_rx_frame_buffer},
			{"test_rxSyncTiming", test_rxSyncTiming},
			{"test_getRxFrameBufferData", test_getRxFrameBufferData},
			{"test_reset_rx_frame_buffer", test_reset_rx_frame_buffer},
			{"test_rx_frame_buffer_is_complete", test_rx_frame_buffer_is_complete},
			{"test_rxInterruptHandler", test_rxInterruptHandler},
//			{"test_moveRxFrameDataToBuffer", test_moveRxFrameDataToBuffer},
			{"\0", NULL}
	};
	for (int i=0; tests[i].name[0] != 0; i ++){
		struct test_struct test = tests[i];

		int ret = test.test_fn();
		if (ret == FAIL){
			printf("test %s failed\n", test.name);
			numberOfTestsFailed++;
		}
	}

	if (numberOfTestsFailed > 0){
		printf("%d tests failed\n",numberOfTestsFailed);
	} else {
		printf("all tests passed\n");
	}
	return 0;
}

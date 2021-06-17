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

struct uart_dev dev;



uint32_t makeFrame(struct uart_dev *dev, uint8_t data){
	uint32_t frame = 0;
	//the start bit is already set to zero.
	frame |= ((uint32_t) data << 1);

	//todo set parityBit.
	//set stop bits.
	for (int i=0; i< dev->numberOfStopBits; i++){
		frame |= (1 << (9+i));
	}


	return frame;
}


void receivedCharHandler(uint8_t val){
	rx_buffer[rx_buffer_index++] = val;
}
void clear_rx_buffer(){
	memset(rx_buffer, 0, 500);
	rx_buffer_index = 0;
}



void mockedPinSetString(struct uart_dev *dev, char *str){
	strcpy(mockedPinData, str);
	mockedPinIndexOfByteBeingSent=0;
	mockedPinCurrentFrame = makeFrame(dev, mockedPinData[mockedPinIndexOfByteBeingSent]);
	mockedPinCurrentFrameIndex =0;
	mockedPinOversamplingCounter = dev->oversamplingRate;
}

int mockedPin_data(){
	if (mockedPinCurrentFrameIndex >= rxFrameSize(&dev)) {
		//need to load the next byte into the frame buffer.
		mockedPinIndexOfByteBeingSent ++;
		mockedPinCurrentFrame = makeFrame(&dev, mockedPinData[mockedPinIndexOfByteBeingSent]);
		mockedPinCurrentFrameIndex =0;
	}
	uint8_t val = (mockedPinCurrentFrame >> mockedPinCurrentFrameIndex)& 0x01;
	mockedPinOversamplingCounter --;
	if (mockedPinOversamplingCounter <=0){
		mockedPinCurrentFrameIndex ++;
		mockedPinOversamplingCounter = dev.oversamplingRate;
	}
	return val;
}

int mockedPin_high(){
	return 1;
}


struct uart_dev commonUart(){
	return create_uart(3, mockedPin_data, receivedCharHandler);
}


int test_addBitToRxFrameBuffer(){
	dev = commonUart();

	//check that the dev has been initialized properly.
	uint32_t bits[] = {1,1,1,1,1,0,0,1,1,0};
	ASSERT_EQUAL(dev.rx_current_frame, 0, "rx_current_frame ==0");
	for (int i=0; i<10; i++){
		ASSERT_EQUAL(dev.rx_current_frame_index, i, "rx_current_frame_index==0");
		addBitToRxFrameBuffer(&dev, bits[i]);
	}

	ASSERT_EQUAL(dev.rx_current_frame, 0x19f, "rx_current_frame ==0xf91");


	return PASS;

}

int test_rxSyncTiming(){
	dev = commonUart();
	ASSERT_EQUAL(dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples, 0, "dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples ==0")
	ASSERT_EQUAL(dev.rx_is_setup, 0, "dev.rex_is_setup == 0");

	rxSyncTiming(&dev, 1);
	ASSERT_EQUAL(dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples, 1, "dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples == 1")
	ASSERT_EQUAL(dev.rx_is_setup, 0, "dev.rex_is_setup == 0");

	//start the timing pulse.
	rxSyncTiming(&dev, 0);
	ASSERT_EQUAL(dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples, 0, "dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples == 0")
	ASSERT_EQUAL(dev.rx_is_setup, 1, "dev.rex_is_setup == 1");

	//a long period without any data. this should cause the UART to reset its timing.
	for (int i=0; i<100; i++) {
		rxSyncTiming(&dev, 1);
	}
	ASSERT_EQUAL(dev.rx_is_setup, 0, "dev.rex_is_setup == 0");

	//start the timing pulse.
	rxSyncTiming(&dev, 0);
	ASSERT_EQUAL(dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples, 0, "dev.rx_pin_has_been_consecutively_high_for_the_last_n_samples == 0")
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

int test_resetRxFrameBuffer(){
	dev = commonUart();
	//just using random values, the index does not match with the number of bits written to the
	//current_frame.
	dev.rx_current_frame = 0x2aa;
	dev.rx_current_frame_index = 5;

	ASSERT_EQUAL(dev.rx_current_frame, 0x2aa, "dev.rx_current_frame == 0x2aa");
	ASSERT_EQUAL(dev.rx_current_frame_index, 5, "dev.rx_current_frame_index == 5");
	resetRxFrameBuffer(&dev);
	ASSERT_EQUAL(dev.rx_current_frame, 0, "dev.rx_current_frame == 0");
	ASSERT_EQUAL(dev.rx_current_frame_index, 0, "dev.rx_current_frame_index == 0");

	return PASS;
}




int test_RxFrameBufferIsComplete(){
	dev = commonUart();
	uint8_t startData = 0x5a;

	for (int i=0; i<rxFrameSize(&dev); i++) {
		ASSERT_EQUAL(RxFrameBufferIsComplete(&dev), 0, "RxFrameBufferIsComplete(&dev) == 0");
		if (i>=1 && i <9){

			addBitToRxFrameBuffer(&dev, (startData >>(i-1)) & 0x01);
		} else {
			//we don't care about these bits.
			addBitToRxFrameBuffer(&dev, 1);
		}

	}
	ASSERT_EQUAL(RxFrameBufferIsComplete(&dev), 1, "RxFrameBufferIsComplete(&dev) == 1");

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


	for (int i=0; i<dev.oversamplingRate * rxFrameSize(&dev)* (strlen(startData)+3); i++) {
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
int set_rx_current_frame_data(struct uart_dev *dev, uint8_t newData){
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
			{"test_addBitToRxFrameBuffer", test_addBitToRxFrameBuffer},
			{"test_rxSyncTiming", test_rxSyncTiming},
			{"test_getRxFrameBufferData", test_getRxFrameBufferData},
			{"test_resetRxFrameBuffer", test_resetRxFrameBuffer},
			{"test_RxFrameBufferIsComplete", test_RxFrameBufferIsComplete},
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

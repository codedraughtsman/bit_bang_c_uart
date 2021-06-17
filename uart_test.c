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

uint32_t mockedPinData[500] ={0};
uint32_t mockedPinBitsWritten =0;
uint32_t mockedPinBitsRead =0;
uint32_t mockedPinOversamplingCounter =0;
uint32_t mockedPinOversampling = 2;

int mockedPinClearData(){
	memset(mockedPinData, 0 ,4*500);
	mockedPinBitsWritten = 0;
	mockedPinBitsRead = 0;
	mockedPinOversamplingCounter =0;

}


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




void mockedPinAddData(struct uart_dev *dev, uint32_t data, uint32_t nBits){
	uint32_t *ptr = mockedPinData + (mockedPinBitsWritten / 32);
	uint32_t offset = mockedPinBitsWritten %32;

	*ptr |= (data << offset);
	//handle when the mocked pin needs to write over two uint32_t.
	//i.e. the data is written to the start of the first uint32_t and the
	//beginning of the second one.
	if (offset + nBits >=32) {
		uint32_t bitsStillNeedToBeWritten = nBits + offset - 32;
		uint32_t bitsThatHaveBeenWritten = nBits - bitsStillNeedToBeWritten;
		mockedPinBitsWritten += bitsThatHaveBeenWritten;
		mockedPinAddData(dev, (data >> bitsThatHaveBeenWritten), bitsStillNeedToBeWritten);
	} else {
		mockedPinBitsWritten += nBits;
	}
}

void mockedPinAddString(struct uart_dev *dev, char *str){
	for (int i=0; str[i] != '\0'; i ++){
		uint32_t frame = makeFrame(dev, str[i]);
		mockedPinAddData(dev, frame, rxFrameSize(dev));
	}
	uint32_t frame = makeFrame(dev, '\0');
	mockedPinAddData(dev, frame, rxFrameSize(dev));
}

int mockedPin(){
	//todo check to see that the index has not gone out of the data array.
	uint32_t *ptr = mockedPinData + (mockedPinBitsRead / 32);
	uint32_t offset = mockedPinBitsRead %32;
	uint8_t val = (*ptr >> offset) & 0x01;

	mockedPinOversamplingCounter ++;
	if (mockedPinOversamplingCounter >= mockedPinOversampling){
		mockedPinBitsRead ++;
		mockedPinOversamplingCounter = 0;
	}
	return val;
}


struct uart_dev commonUart(){
	return create_uart(2, mockedPin);
}

int test_addDataToMockedPin(){
	struct uart_dev dev = commonUart();
	mockedPinClearData();
	uint8_t *str = "hello";
	mockedPinAddString(&dev, str);

	for (int j=0; str[j] != '\0'; j++){
		//chomp start bit
		mockedPin();
		for (int i=0; i<8; i++){
			uint8_t mockedValue =0;
			for (int x=0;x<8;x++){
				mockedValue |= (mockedPin() << x);
			}
			ASSERT_EQUAL(mockedValue, str[j], "test_addDataToMockedPin: data from mocked pin does not match expected data\n")
		}
		for (int i=0; i< dev.numberOfStopBits + dev.hasParityBit; i ++){
			//chomp stop bits and parity bits
			mockedPin();
		}
	}
}

int test_addBitToRxFrameBuffer(){
	struct uart_dev dev = commonUart();

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
	struct uart_dev dev = commonUart();
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
	struct uart_dev dev = commonUart();
	dev.rx_current_frame = 0x2aa;
	uint8_t data = getRxFrameBufferData(&dev);

	ASSERT_EQUAL(data, 0x55, "data == 0x55");
	return PASS;
}

int test_resetRxFrameBuffer(){
	struct uart_dev dev = commonUart();
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
	struct uart_dev dev = commonUart();
	uint8_t startData = 0x5a;

	for (int i=0; i<9; i++) {
		ASSERT_EQUAL(RxFrameBufferIsComplete(&dev), 0, "RxFrameBufferIsComplete(&dev) == 0");
		if (i>=1 && i <=8){

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
	struct uart_dev dev = commonUart();
	int8_t *startData = "hello world";
	mockedPinClearData();
	//mockedPinAddData(&dev, 0xffff, 32);
	mockedPinAddString(&dev, startData);
	for (int i=0;i<mockedPinBitsWritten/32;i++){
		for (int j=0;j<32;j++){
			uint32_t byte =mockedPinData[i];
			uint32_t bit = (byte >> j) &0x01;
			printf("%d",bit);
		}
		printf(" ");
	}
	printf("\n");

	for (int i=0; i<500; i++) {
		rxInterruptHandler(&dev);
	}

	if (strcmp(dev.rx_buffer, startData)!=0){
		//strings don't match.
		printf("test_rxInterruptHandler, the strings don't match\n");
		printf("original string '%s', received string '%s'\n",startData, dev.rx_buffer);
		return FAIL;

	}


	return PASS;
}
int set_rx_current_frame_data(struct uart_dev *dev, uint8_t newData){
	dev->rx_current_frame = ((uint32_t)newData) << 1;
	ASSERT_EQUAL((dev->rx_current_frame >>1), newData, "set_rx_current_frame_data: dev->rx_current_frame == newData");
}

int test_moveRxFrameDataToBuffer(){
	struct uart_dev dev = commonUart();

	//todo do something with this return code if the setting fails.
	set_rx_current_frame_data(&dev, 'a');

	ASSERT_EQUAL(dev.rx_buffer[0], 0, "dev.rx_buffer[0] == 0");
	moveRxFrameDataToBuffer(&dev);
	ASSERT_EQUAL(dev.rx_buffer[0], 'a', "dev.rx_buffer[0] == a");
	set_rx_current_frame_data(&dev, 'b');
	moveRxFrameDataToBuffer(&dev);

	//check to see if it has changed the first item in the buffer.
	ASSERT_EQUAL(dev.rx_buffer[0], 'a', "dev.rx_buffer[0] == a");
	ASSERT_EQUAL(dev.rx_buffer[1], 'b', "dev.rx_buffer[1] == b");

	return PASS;
}

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
			{"test_moveRxFrameDataToBuffer", test_moveRxFrameDataToBuffer},
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

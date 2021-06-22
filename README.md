# Codedraughtsman's bit-bang uart written in C

This is a generic, platform inderpendent, bit-bang uart driver. 
Currently, only the uart reciever is implemented and the uart transmitter will hopefully be added soon.

## how it works internally

## what you have to do to get it to work.

include the and build the files

```C
struct uart_rx_dev rx_dev;

/* this is called when a whole byte has been recieved.
 * it is up to you, the user, to check that the parity bit is correct for the recieved byte
 * and to process/store the recieved byte.
 */
void uart_rx_received_data(uint8_t byte, int32_t parity_bit){
  //check process and store the byte.
}

/* This is a wrapper function that allows the uart to read the pin that it is 
* watching without having to worry about the underlying architecture.
*/
uint32_t read_uart_rx_pin(void){
  //An example pin read could look like this STM32 example.
	//return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);
  return pin_value;
}

/*
* This is the timer interrupt that is called 3 times per bit that is sent.
* i.e. 3 times the baud rate.
*/
void timer_interrupt_function(TIM_HandleTypeDef* htim)
{
    //call the uart's interupt handler.
    rx_interrupt_handler(&rx_dev);
}

int main(){
  //setup timers, interrupts and i/o stuff first.
  
  //Set up the uart. The '3' tells the uart that it is oversampling the incoming signal by 3.
  uart_rx_init(&rx_dev, 3, read_uart_rx_pin, uart_rx_received_data);
}
```


## stm32 projects using this library.

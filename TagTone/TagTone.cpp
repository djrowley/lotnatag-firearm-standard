/* $Id: Tone.cpp 119 2010-07-17 18:56:36Z bhagman@roguerobotics.com $

  A Tone Generator Library - modified for laser tag pulse

  Written by Brett Hagman
  http://www.roguerobotics.com/
  bhagman@roguerobotics.com
  
  Modified by D Rowley, May 2015

    This library is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*************************************************/

#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <Arduino.h>
#include <pins_arduino.h>
#include "TagTone.h"

// timerx_toggle_count:
//  > 0 - duration specified
//  = 0 - stopped
//  < 0 - infinitely (until stop() method called, or new play() called)

volatile int32_t timer1_toggle_count;
volatile uint8_t *timer1_pin_port;
volatile uint8_t timer1_pin_mask;

volatile int32_t timer2_toggle_count;
volatile int32_t timer2_signal_toggle_count;
int32_t signal_cycle_count = 0;
volatile uint8_t signal_state = 1;

volatile uint8_t *timer2_pin_port;
volatile uint8_t timer2_pin_mask;

#define CARRIER_FREQUENCY 57600
#define SIGNAL_FREQUENCY 1800

#define AVAILABLE_TONE_PINS 3


void TagTone::beginSignal(uint8_t tonePin)
{
	_pin = tonePin;
	_timer = 1;
	
    // 16 bit timer
    TCCR1A = 0;
    TCCR1B = 0;
    bitWrite(TCCR1B, WGM12, 1);
    bitWrite(TCCR1B, CS10, 1);
    timer1_pin_port = portOutputRegister(digitalPinToPort(_pin));
    timer1_pin_mask = digitalPinToBitMask(_pin);
}

void TagTone::beginCarrier(uint8_t tonePin)
{
	_pin = tonePin;
	_timer = 2;
	
    // 8 bit timer
    TCCR2A = 0;
    TCCR2B = 0;
    bitWrite(TCCR2A, WGM21, 1);
    bitWrite(TCCR2B, CS20, 1);
    timer2_pin_port = portOutputRegister(digitalPinToPort(_pin));
    timer2_pin_mask = digitalPinToBitMask(_pin);
	
	int duration = 50; //50 milliseconds
	uint8_t prescalarbits = 0b001;
	int32_t signal_toggle_count = 0;
	

    // Note: may need prescaler reimplemented if using 16Mhz
    ocr = F_CPU / (CARRIER_FREQUENCY * 2) - 1;
    prescalarbits = 0b001;  
    

    TCCR2B = (TCCR2B & 0b11111000) | prescalarbits;
   
    toggle_count = 2 * CARRIER_FREQUENCY * duration / 1000;	// how many times the carrier toggles before pulse ends
	
	// want to AND with 1800 Hz signal
	
	// as duration now set to 50: toggle_count for carrier = 5760
	// equivalent toggle count for signal = 180
	signal_toggle_count = 2* SIGNAL_FREQUENCY * duration / 1000;
	
	// want to flip signal state every 32 carrier pulses
	signal_cycle_count = 32; //toggle_count / signal_toggle_count;
}



// duration (in milliseconds).

void TagTone::playCarrier()
{

    OCR2A = ocr;
	timer2_signal_toggle_count = signal_cycle_count;
    timer2_toggle_count = toggle_count;
	signal_state = true;
    bitWrite(TIMSK2, OCIE2A, 1);
  
}

void TagTone::playSignal(uint32_t duration)
{

	uint8_t prescalarbits = 0b001;
	int32_t toggle_count = 0;
	uint32_t ocr = 0;

    // two choices for the 16 bit timers: ck/1 or ck/64
    ocr = F_CPU / SIGNAL_FREQUENCY / 2 - 1;
    prescalarbits = 0b001;

    TCCR1B = (TCCR1B & 0b11111000) | prescalarbits;

    toggle_count = 2 * SIGNAL_FREQUENCY * duration / 1000;
    

    // Set the OCR for the given timer,
    // set the toggle count,
    // then turn on the interrupts

    OCR1A = ocr;
    timer1_toggle_count = toggle_count;
    bitWrite(TIMSK1, OCIE1A, 1);

}


void TagTone::stop()
{
  switch (_timer)
  {
    case 1:
      TIMSK1 &= ~(1 << OCIE1A);
      break;
    case 2:
      TIMSK2 &= ~(1 << OCIE2A);
      break;
  }

  digitalWrite(_pin, 0);
}


bool TagTone::isPlaying(void)
{
  bool returnvalue = false;
  
  switch (_timer)
  {
    case 1:
      returnvalue = (TIMSK1 & (1 << OCIE1A));
      break;
    case 2:
      returnvalue = (TIMSK2 & (1 << OCIE2A));
      break;

  }
  return returnvalue;
}



ISR(TIMER1_COMPA_vect)
{
  if (timer1_toggle_count != 0)
  {
    // toggle the pin
    *timer1_pin_port ^= timer1_pin_mask;

    if (timer1_toggle_count > 0)
      timer1_toggle_count--;
  }
  else
  {
    TIMSK1 &= ~(1 << OCIE1A);                 // disable the interrupt
    *timer1_pin_port &= ~(timer1_pin_mask);   // keep pin low after stop
  }
}

// working stable pulse
/*ISR(TIMER2_COMPA_vect)
{
  if (timer2_toggle_count != 0)
  {
    // toggle the pin
    *timer2_pin_port ^= timer2_pin_mask;

    if (timer2_toggle_count > 0)
      timer2_toggle_count--;
  }
  else
  {
    TIMSK2 &= ~(1 << OCIE2A);                 // disable the interrupt
    *timer2_pin_port &= ~(timer2_pin_mask);   // keep pin low after stop
  }
}*/

ISR(TIMER2_COMPA_vect)
{
  if (timer2_signal_toggle_count != 0)
  {
    // toggle the pin
	if(signal_state == 1)
		*timer2_pin_port ^= timer2_pin_mask;

    if (timer2_toggle_count > 0)
      timer2_toggle_count--; 
	else
	{
		TIMSK2 &= ~(1 << OCIE2A);                 // disable the interrupt
		*timer2_pin_port &= ~(timer2_pin_mask);   // keep pin low after stop
	}
	 timer2_signal_toggle_count --; 
  }
  else
  {
	timer2_signal_toggle_count = signal_cycle_count;
	timer2_toggle_count --;
	if(signal_state == 1)
	{
		signal_state = 0;
		*timer2_pin_port &= ~(timer2_pin_mask);
		//*timer2_pin_port ^= timer2_pin_mask;
	}
	else 
	{
		signal_state = 1;
		*timer2_pin_port ^= timer2_pin_mask;
	}
  }
}


/*ISR(TIMER2_COMPA_vect)
{
	//if(signal_state == 0b1111)
		*timer2_pin_port ^= timer2_pin_mask;
	//else
	//	*timer2_pin_port ^= timer2_pin_mask;

	if (timer2_signal_toggle_count > 0)
	{
		if (timer2_toggle_count > 0)
			timer2_toggle_count--;
		else
		{
			TIMSK2 &= ~(1 << OCIE2A);                 // disable the interrupt
			*timer2_pin_port &= ~(timer2_pin_mask);   // keep pin low after stop
		}
		timer2_signal_toggle_count --;
	}
	else
	{
		timer2_signal_toggle_count = signal_cycle_count;
		timer2_toggle_count --;
		//signal_state ^= signal_state;
	}

}*/


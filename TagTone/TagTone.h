/* $Id: Tone.h 113 2010-06-16 20:16:29Z bhagman@roguerobotics.com $

  A Tone Generator Library (modified for creating a specific laser tag pulse)

  Written by Brett Hagman
  http://www.roguerobotics.com/
  bhagman@roguerobotics.com
  
  Modified by D Rowley - May 2015

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

#ifndef _TagTone_h
#define _TagTone_h

#include <stdint.h>


/*************************************************
* Definitions
*************************************************/

class TagTone
{
  public:
    void beginCarrier(uint8_t tonePin);
	void beginSignal(uint8_t tonePin);
    bool isPlaying();
    void playCarrier();
	void playSignal(uint32_t duration = 0);
    void stop();

  private:
    uint8_t _pin;
    int8_t _timer;
	int32_t toggle_count = 0;
	uint32_t ocr = 0;
};

#endif

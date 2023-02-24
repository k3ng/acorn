/*



  Standalone compile with:

    gcc -g -o avr serial.c debug.c avr.c -pthread



*/




#include "avr.h"
#include "debug.h"
#include "serial.h"





// ---------------------------------------------------------------------------------------

void initialize_avr_bus(){


  /*

     We call it the AVR Bus as AVR chips can be daisy-chained like so;

                        +-------------+       +-------------+       +-------------+
              TX]------>|RX  AVR 1  TX|------>|RX  AVR 2  TX|------>|RX  AVR 3  TX|-----+
     Pi ttyS0           +-------------+       +-------------+       +-------------+     |
              RX]<----------------------------------------------------------------------+


     If the first AVR receives a command it doesn't recognize, it simply sends it out its TX port
     for the next AVR to evaluate and act upon if it recognizes the command.  Note that this
     makes it required that commands are not duplicated between AVR units on the bus.

     The poll (p) command has special behavior as each AVR on the bus will respond to it.  An AVR 
     will append its identification and software version and the strong out its TX port to the next
     AVR to do the same.  So, for example, three AVRs like above would do something like this:

     pCW_Si5351_unit:202301161347$
     pCW_Si5351_unit:202301161347$auto_tuner_unit:202303261500$
     pCW_Si5351_unit:202301161347$auto_tuner_unit:202303261500$LCD_manager:202411091000$

     This allows the Pi to poll and discover whatever AVR units are on the bus.


  */

  //zzzzzzz
  


}

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------

#if !defined(COMPILING_EVERYTHING)


  void main(){




  }

#endif //!defined(COMPILING_EVERYTHING)
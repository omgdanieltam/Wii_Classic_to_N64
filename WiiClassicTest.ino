/*
* By Daniel Tam (omgdanieltam@gmail.com)
============ NOTES ============
The idea for this project was brought out by Moltov, a Zelda OoT speedrunner.
Since he used a classic controller pro (referred to as CCP from this point) to run OoT, he couldn't have used a N64 to run the game.
This is written so that the CCP can be read and outputted as a N64 controller.
This was written and tested using Osepp's Uno board (based off of Arduino Uno using the same ATmega358 chip).
Final product will be used on the same board.

Much of the code is copied and pasted with only slight variations in code.
===============================

============ WIRING ============
How to correctly hook the wires up
CCP SCL -> Analog 5
CCP SDA -> Analog 4
CCP VSS -> 3.3V
CCP GRD -> Ground
N64 3.3V -> None
N64 Data -> Digital 8
N64 GRD -> Ground
================================

============ MAPPING ============
How keys are mapped from the CCP to N64 in format CCP : N64
A : A
B : B
X : C-Right
Y : C-Left
R : R
L : L
RZ/LZ : C-Down
+ : Start
Right Stick Up : C-Up
Right Stick Down: C-Down
Right Stick Left : C-Left
Right Stick Right: C-Right
---- All other keys are left unmapped as this was intended to be used with Zelda OoT ----
================================

============ SOURCES ===========
(Álvaro García) https://github.com/maxpowel/Wii-N64-Controller  -- Wii remote to N64 using Arduino
(Andrew Brown) https://github.com/brownan/Gamecube-N64-Controller -- Gamecube controller to N64 using Arduino
(Tim Hirzel) http://playground.arduino.cc/Main/WiiClassicController -- Wii Classic Controller to Arduino
================================
*/
#include "Wire.h"
#include "WiiClassic.h"
#include "crc_table.h"

#define N64_PIN 8
#define N64_HIGH DDRB &= ~0x01
#define N64_LOW DDRB |= 0x01
#define N64_QUERY (PINB & 0x01)

char n64_raw_dump[281]; // maximum recv is 1+2+32 bytes + 1 bit
// n64_raw_dump does /not/ include the command byte. That gets pushed into
// n64_command:
unsigned char n64_command;

// bytes to send to the 64
// maximum we'll need to send is 33, 32 for a read request and 1 CRC byte
unsigned char n64_buffer[33];

void get_n64_command();

// Classic controller library
WiiClassic wiiClassy = WiiClassic();

void setup() 
{
  // Setup the classic controller
  Wire.begin();
  Serial.begin(9600);
  wiiClassy.begin();
  wiiClassy.update();
  
  // Communication with the N64 on this pin
  digitalWrite(N64_PIN, LOW);
  pinMode(N64_PIN, INPUT);  
}

// completly copied and pasted to not mess up timings
void n64_send(unsigned char *buffer, char length, bool wide_stop)
{
    asm volatile (";Starting N64 Send Routine");
    // Send these bytes
    char bits;
    
    bool bit;

    // This routine is very carefully timed by examining the assembly output.
    // Do not change any statements, it could throw the timings off
    //
    // We get 16 cycles per microsecond, which should be plenty, but we need to
    // be conservative. Most assembly ops take 1 cycle, but a few take 2
    //
    // I use manually constructed for-loops out of gotos so I have more control
    // over the outputted assembly. I can insert nops where it was impossible
    // with a for loop
    
    asm volatile (";Starting outer for loop");
outer_loop:
    {
        asm volatile (";Starting inner for loop");
        bits=8;
inner_loop:
        {
            // Starting a bit, set the line low
            asm volatile (";Setting line to low");
            N64_LOW; // 1 op, 2 cycles

            asm volatile (";branching");
            if (*buffer >> 7) {
                asm volatile (";Bit is a 1");
                // 1 bit
                // remain low for 1us, then go high for 3us
                // nop block 1
                asm volatile ("nop\nnop\nnop\nnop\nnop\n");
                
                asm volatile (";Setting line to high");
                N64_HIGH;

                // nop block 2
                // we'll wait only 2us to sync up with both conditions
                // at the bottom of the if statement
                asm volatile ("nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              );

            } else {
                asm volatile (";Bit is a 0");
                // 0 bit
                // remain low for 3us, then go high for 1us
                // nop block 3
                asm volatile ("nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\nnop\n"  
                              "nop\n");

                asm volatile (";Setting line to high");
                N64_HIGH;

                // wait for 1us
                asm volatile ("; end of conditional branch, need to wait 1us more before next bit");
                
            }
            // end of the if, the line is high and needs to remain
            // high for exactly 16 more cycles, regardless of the previous
            // branch path

            asm volatile (";finishing inner loop body");
            --bits;
            if (bits != 0) {
                // nop block 4
                // this block is why a for loop was impossible
                asm volatile ("nop\nnop\nnop\nnop\nnop\n"  
                              "nop\nnop\nnop\nnop\n");
                // rotate bits
                asm volatile (";rotating out bits");
                *buffer <<= 1;

                goto inner_loop;
            } // fall out of inner loop
        }
        asm volatile (";continuing outer loop");
        // In this case: the inner loop exits and the outer loop iterates,
        // there are /exactly/ 16 cycles taken up by the necessary operations.
        // So no nops are needed here (that was lucky!)
        --length;
        if (length != 0) {
            ++buffer;
            goto outer_loop;
        } // fall out of outer loop
    }

    // send a single stop (1) bit
    // nop block 5
    asm volatile ("nop\nnop\nnop\nnop\n");
    N64_LOW;
    // wait 1 us, 16 cycles, then raise the line 
    // take another 3 off for the wide_stop check
    // 16-2-3=11
    // nop block 6
    asm volatile ("nop\nnop\nnop\nnop\nnop\n"
                  "nop\nnop\nnop\nnop\nnop\n"  
                  "nop\n");
    if (wide_stop) {
        asm volatile (";another 1us for extra wide stop bit\n"
                      "nop\nnop\nnop\nnop\nnop\n"
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\n");
    }

    N64_HIGH;
}

boolean lightStatus;
int read_buffer[4];

void cc_to_n64()
{
  wiiClassy.update();
  
    // First byte
    if (wiiClassy.aPressed()) 
    {
      // a button
       n64_buffer[0] |= 0x80;
    }
    else if (wiiClassy.bPressed()) 
    {
      // b button
      n64_buffer[0] |= 0x40;
    }
    else if (wiiClassy.leftShoulderPressed()) 
    {
      // z button for n64 or left shoulder for classic
       n64_buffer[0] |= 0x20;
    }
    else if (wiiClassy.startPressed()) 
    {
      // start
       n64_buffer[0] |= 0x10;
    }
    
    // Second Byte
    if (wiiClassy.rightShoulderPressed()) 
    {
      // r button for n64 or right shoulder for classic
       n64_buffer[1] |= 0x10;
    }
    else if (wiiClassy.xPressed()) 
    {
      // c-right for n64 or x for classic
      n64_buffer[1] |= 0x01;
    }
    else if (wiiClassy.yPressed()) 
    {
      // c-left for n64 or y for classic
      n64_buffer[1] |= 0x02;
    }
    else if(wiiClassy.rzPressed())
    {
      // c-down for n64 or rz for classic
      n64_buffer[1] |= 0x04;
    }
    else if(wiiClassy.lzPressed())
    {
      // c-down for n64 or lz for classic
      n64_buffer[1] |= 0x04;
    }
    else if(wiiClassy.rightStickX() > 21)
    {
      // c-right for n64 or right stick right for classic
      n64_buffer[1] |= 0x01;
    }
    else if(wiiClassy.rightStickX() < 11)
    {
      // c-left for n64 or right stick left for classic
       n64_buffer[1] |= 0x02;
    }
    else if(wiiClassy.rightStickY() < 11)
    {
      // c-down for n64 or right stick down for classic
      n64_buffer[1] |= 0x04;
    }
    else if(wiiClassy.rightStickY() > 21)
    {
      // c-up for n64 or right stick down for classic
      n64_buffer[1] |= 0x08;
    }
    
    // third and fourth byte (control stick)
    // the classic controller pro reads the values of the stick as default of (32:x and 30:y [by my tests, I'm assuming it's 0 to 64 with 32 as center])
    // the n64 takes in a signed int from -80 to 80 as a default of 0
    // will subtract the value by 32 (default classic) and multiply by 4 giving us the proper value
    n64_buffer[2] = ((wiiClassy.leftStickX()- 32) * 2.5);
    n64_buffer[3] = ((wiiClassy.leftStickY()- 32) * 2.5);
    
    // second method to test is to set the values of the stick from 128 as the center
    /*
    n64_buffer[2] = (wiiClassy.leftStickX() * 4);
    n64_buffer[3] = (wiiClassy.leftStickY() * 4);
    
    */
}

// copied and pasted the second half as the first is merely assign button presses to bytes
void loop() 
{
  unsigned char data, addr;
  memset(n64_buffer, 0, sizeof(n64_buffer));

    cc_to_n64();
    
    // COPIED AND PASTED FROM THIS POINT
  
    // Wait for incomming 64 command
    // this will block until the N64 sends us a command
    noInterrupts();
    get_n64_command();

    // 0x00 is identify command
    // 0x01 is status
    // 0x02 is read
    // 0x03 is write
    //Serial.println(n64_command, HEX);
   
    switch (n64_command)
    {
        case 0x00:
        case 0xFF:
            // identify
            // mutilate the n64_buffer array with our status
            // we return 0x050001 to indicate we have a rumble pack
            // or 0x050002 to indicate the expansion slot is empty
            //
            // 0xFF I've seen sent from Mario 64 and Shadows of the Empire.
            // I don't know why it's different, but the controllers seem to
            // send a set of status bytes afterwards the same as 0x00, and
            // it won't work without it.
            n64_buffer[0] = 0x05;
            n64_buffer[1] = 0x00;
            n64_buffer[2] = 0x02;

            n64_send(n64_buffer, 3, 0);

            //Serial.println("It was 0x00: an identify command");
            break;
        case 0x01:
            // Send to n64 the received data
            
            n64_send(n64_buffer, 4, 0);
            //Serial.println("It was 0x01: the query command");
            break;
        case 0x02:
            // A read. If the address is 0x8000, return 32 bytes of 0x80 bytes,
            // and a CRC byte.  this tells the system our attached controller
            // pack is a rumble pack

            // Assume it's a read for 0x8000, which is the only thing it should
            // be requesting anyways
            memset(n64_buffer, 0x80, 32);
            n64_buffer[32] = 0xB8; // CRC

            n64_send(n64_buffer, 33, 1);

            //Serial.println("It was 0x02: the read command");
            break;
        case 0x03:
            // A write. we at least need to respond with a single CRC byte.  If
            // the write was to address 0xC000 and the data was 0x01, turn on
            // rumble! All other write addresses are ignored. (but we still
            // need to return a CRC)

            // decode the first data byte (fourth overall byte), bits indexed
            // at 24 through 31
            data = 0;
            data |= (n64_raw_dump[16] != 0) << 7;
            data |= (n64_raw_dump[17] != 0) << 6;
            data |= (n64_raw_dump[18] != 0) << 5;
            data |= (n64_raw_dump[19] != 0) << 4;
            data |= (n64_raw_dump[20] != 0) << 3;
            data |= (n64_raw_dump[21] != 0) << 2;
            data |= (n64_raw_dump[22] != 0) << 1;
            data |= (n64_raw_dump[23] != 0);

            // get crc byte, invert it, as per the protocol for
            // having a memory card attached
            n64_buffer[0] = crc_repeating_table[data] ^ 0xFF;

            // send it
            n64_send(n64_buffer, 1, 1);

            // end of time critical code
            // was the address the rumble latch at 0xC000?
            // decode the first half of the address, bits
            // 8 through 15
            addr = 0;
            addr |= (n64_raw_dump[0] != 0) << 7;
            addr |= (n64_raw_dump[1] != 0) << 6;
            addr |= (n64_raw_dump[2] != 0) << 5;
            addr |= (n64_raw_dump[3] != 0) << 4;
            addr |= (n64_raw_dump[4] != 0) << 3;
            addr |= (n64_raw_dump[5] != 0) << 2;
            addr |= (n64_raw_dump[6] != 0) << 1;
            addr |= (n64_raw_dump[7] != 0);

            if (addr == 0xC0) {
            //    rumble = (data != 0);
            }

           // Serial.println("It was 0x03: the write command");
            //Serial.print("Addr was 0x");
            //Serial.print(addr, HEX);
            //Serial.print(" and data was 0x");
            //Serial.println(data, HEX);
            break;

    }

    interrupts();

    // DEBUG: print it
    //print_gc_status();
  
}

/**
  * Waits for an incomming signal on the N64 pin and reads the command,
  * and if necessary, any trailing bytes.
  * 0x00 is an identify request
  * 0x01 is a status request
  * 0x02 is a controller pack read
  * 0x03 is a controller pack write
  *
  * for 0x02 and 0x03, additional data is passed in after the command byte,
  * which is also read by this function.
  *
  * All data is raw dumped to the n64_raw_dump array, 1 bit per byte, except
  * for the command byte, which is placed all packed into n64_command
  */
void get_n64_command()
{
  
    int bitcount;
    char *bitbin = n64_raw_dump;
    int idle_wait;

func_top:
    n64_command = 0;

    bitcount = 8;

    // wait to make sure the line is idle before
    // we begin listening
    for (idle_wait=32; idle_wait>0; --idle_wait) {
        if (!N64_QUERY) {
            idle_wait = 32;
        }
    }

read_loop:
        // wait for the line to go low
        while (N64_QUERY){}

        // wait approx 2us and poll the line
        asm volatile (
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                );
        if (N64_QUERY)
            n64_command |= 0x01;

        --bitcount;
        if (bitcount == 0)
            goto read_more;

        n64_command <<= 1;

        // wait for line to go high again
        // I don't want this to execute if the loop is exiting, so
        // I couldn't use a traditional for-loop
        while (!N64_QUERY) {}
        goto read_loop;

read_more:
        switch (n64_command)
        {
            case (0x03):
                // write command
                // we expect a 2 byte address and 32 bytes of data
                bitcount = 272 + 1; // 34 bytes * 8 bits per byte
                //Serial.println("command is 0x03, write");
                break;
            case (0x02):
                // read command 0x02
                // we expect a 2 byte address
                bitcount = 16 + 1;
                //Serial.println("command is 0x02, read");
                break;
            case (0x00):
            case (0x01):
            default:
                // get the last (stop) bit
                bitcount = 1;
                break;
            //default:
            //    Serial.println(n64_command, HEX);
            //    goto func_top;
        }

        // make sure the line is high. Hopefully we didn't already
        // miss the high-to-low transition
        while (!N64_QUERY) {}
read_loop2:
        // wait for the line to go low
        while (N64_QUERY){}

        // wait approx 2us and poll the line
        asm volatile (
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                      "nop\nnop\nnop\nnop\nnop\n"  
                );
        *bitbin = N64_QUERY;
        ++bitbin;
        --bitcount;
        if (bitcount == 0)
            return;

        // wait for line to go high again
        while (!N64_QUERY) {}
        goto read_loop2;
}

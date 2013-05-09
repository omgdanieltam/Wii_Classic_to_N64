WiiClassicTest
==============
Classic Controller Pro to N64 Adapter  
by: Daniel Tam

INTRODUCTION
===============================
The idea for this project was brought out by Moltov, a Legend of Zelda: Ocarina of Time (OoT) speedrunner.
He plays OoT on the Wii instead of the original system, a Nintendo 64. Since he uses the classic controller pro (CCP) to play, he cannot play on the Nintendo 64, since it uses a different controller. A basic adapter cannot be made since it uses two different protocols.
This is written so that the CCP can be read and outputted as a N64 controller.
This was written and tested using Osepp's Uno board (based off of Arduino Uno using the same ATmega358 chip).
Final product will be used on the same board.
Much of the code is copied and pasted with only slight variations in code.
This so far is known to be the first adpater to do this conversion as there are no adapters on the market.

WIRING
===============================

How to correctly hook the wires up
CCP SCL -> Analog 5

CCP SDA -> Analog 4

CCP VSS -> 3.3V

CCP GRD -> Ground

N64 3.3V -> None

N64 Data -> Digital 8

N64 GRD -> Ground

MAPING
============
How keys are mapped from the CCP to N64 in format CCP : N64

A : A

B : B

X : C-Right

Y : C-Left

R : R

L : Z

RZ/LZ : C-Down

PLUS : Start

Right Stick Up : C-Up

Right Stick Down: C-Down

Right Stick Left : C-Left

Right Stick Right: C-Right

---- All other keys are left unmapped as this was intended to be used with Zelda OoT ----

(Keys missing: MINUS, D-PAD, HOME)
SOURCES
================================
(Álvaro García) https://github.com/maxpowel/Wii-N64-Controller  -- Wii remote to N64 using Arduino

(Andrew Brown) https://github.com/brownan/Gamecube-N64-Controller -- Gamecube controller to N64 using Arduino

(Tim Hirzel) http://playground.arduino.cc/Main/WiiClassicController -- Wii Classic Controller to Arduino


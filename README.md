# Pocket Tag Girls' Night
Tiny laser tag for Elab girls' night!

# Assembly instructions

### 1. LED + resistor for LOGO
Components: 
- white LED for D3
- 120Ω for R5

Note the direction of the LED
Light should point DOWN

<img src="media/step1.png" alt="Step 1: solder D3, R5" width="500">


### 2. LEDs + resistors for shoulder pads
Components:
- red LEDs for D1 and D2
- 120Ω for R3 and R4

Note the direction of the LEDs
Light should point DOWN

<img src="media/step2.png" alt="Step 2: solder D1, R3, D2 and R4" width="500">

### Test time!

Apply 3.3V between VCC and GND on the test points. The LED under the logo should now light up.

<img src="media/test1-logo.png" alt="Test logo" width="500">

Apply 3.3V between PC2 and GND on the test points. The LEDs on the shoulder pads should now light up.

<img src="media/test2-shoulder-leds.png" alt="Test shoulder LEDs" width="500">

### 3. Buzzer with components
Components:
- Buzzer for BZ1
- 2302 NMOS for Q1
- 1Ω for R6
- 1uF for C5

Note the direction of the buzzer.

<img src="media/step3.png" alt="Step 3: solder BZ1, R6, C5 and Q1" width="500">



### 4. Power and battery protection

Components: 
- 1µF for C1
- XB3303A for U3
- 120Ω for R1
- Switch for SW1
- Charging connector for CHRG


<img src="media/step4.png" alt="Step 4: solder C1, U3, R1, SW1 and CHRG" width="500">

### Test time!

With the board powered, short the test points VCC and GND using tweezers or other conducting material. The LED should turn off within a second and the board should then stay off until power cycled. 
<img src="media/test3-battery-protection.png" alt="Test battery protection: short VCC and GND" width="500">


### 5. Microcontroller and decoupling capacitors
Components:
- 1µF for C3
- 22µF for C4
- CH32V003 for U1

<img src="media/step5.png" alt="Step 5: solder C3, C4 and U1">


### 6. Transmitter
Components:
- 2302 for Q2
- 120Ω for R2


<img src="media/step7.png" alt="Step 6: solder Q2 and R2">


### 7. Receiver

Components:
- 1µF for C2
- VS838 for U2

The receiver is mounted on the other side of the PCB, otherwise no one can hit you and that is cheating. Insert the legs and bend the receiver to fit in the square on the PCB before soldering.

<img src="media/step6.png" alt="Step 7: solder C2 and U2 on the back side">


### 8. Battery and cable
Solder the black side of the battery to BAT- and the red side to BAT+. Solder the cable to the pads as follows:
* BLACK to LEDs
* YELLOW to BUTTON
* RED to VCC
Secure the cable with a zip tie

<img src="media/step8.png" alt="Step 8: solder battery and cable" width="500">


# Assembly instructions - laser blaster


### 1. Resistors
Components:
- 47Ω for R1
- 47Ω for R2

<img src="media/blaster-step-1.jpg" alt="Step 1: solder R1 and R2" width="500">


### 2. Trigger switch
Components:
- SPDT 12.8x5.7mm Limit switch for SW1

From the other side of the PCB, insert the switch. Note the direction of the lever. Solder the switch.

<img src="media/blaster-step-2.jpg" alt="Step 2: switch" width="500">


### 3. LEDS
Components:
- TSAL6100 IR LED
- Pink THT LED

From the other side of the PCB, insert the switch. Note the direction of the lever. Solder the switch.


### 4. Cable
Solder the cable to the pads
* BLACK to LEDs
* YELLOW to BUTTON
* RED to VCC

The cable should point from the pads towards the switch and LEDs (as pictured)

<img src="media/blaster-step-4.jpg" alt="Step 2: switch" width="500">


### 5. Insert into 3d printed blaster
* Find the half of the 3d printed blaster that has the cable holding teeth (shown with arrow)
* Insert PCB into 3d print. You may need to bend the LEDs a little
* Fold the cable over and push between PCB and cable holding teeth
* Clip second half of 3d print on top

<img src="media/blaster-step-5.jpg" alt="Step 2: switch" width="500">
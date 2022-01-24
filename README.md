Author: Benedikt Mautner
  
Date: 19.1.22

Descripton: This sketch acts as a controller for stepper motors with the JSS57P2N controller.

One short press of the buttons moves the platform to the stops.

Long presses move the platform until the button is released. Endstops are always respected!

Enable, Pulse and Direction pins can be the same for all steppers since they have to move the same distance anyways

This could be changed if one wants different pulses/rev for each stepper, but I see no need for this

Buttons use input_pullup, so they should be connected to short to ground when pressed!


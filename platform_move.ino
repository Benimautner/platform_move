/*
  Author: Benedikt Mautner
  Date: 19.1.22
  Descripton: This sketch acts as a controller for stepper motors with the JSS57P2N controller.
  One short press of the buttons moves the platform to the stops.
  Long presses move the platform until the button is released. Endstops are always respected!

  Enable, Pulse and Direction pins can be the same for all steppers since they have to move the same distance anyways
  This could be changed if one wants different pulses/rev for each stepper, but I see no need for this

  Buttons use input_pullup, so they should be connected to short to ground when pressed!

*/

#define TYPE_TIME unsigned long

#define UP_PIN 3
#define DOWN_PIN 2
#define PULSE_PIN 5
#define ENABLE_PIN 6
#define DIRECTION_PIN 4
#define TOP_LIMITER_PIN 8
#define BOTTOM_LIMITER_PIN 9

#define PULSESPERREV 5000
#define PULSE_DELAY_US 220 // can be as low as 5, according to datasheet of motor
#define SHORT_PRESS_MS 200 // clicks of buttons under 200 ms count as short clicks

#define DIRECTION_UP 0  // HIGH or LOW, depending on the preferred direction for upwards motion
#define DIRECTION_DOWN !DIRECTION_UP

enum ButtonStatus : int {
  RELEASED,     // do not change order! RELEASED has to be 0 and pressed has to be 1
  PRESSED
};

enum DriveStatus : int {
  STOPPED,
  UP_MANUAL,
  DOWN_MANUAL,
  UP_TILL_STOP,
  DOWN_TILL_STOP,
};

struct Button {
  TYPE_TIME duration = 0;
  TYPE_TIME prev_duration = 0;
  ButtonStatus status = RELEASED;
};

struct Buttons {
  Button up;
  Button down;
  Button top_limiter;
  Button bottom_limiter;

  bool ignore = false;
};

struct Controller {
  Buttons buttons;
  int direction = -1;
  int pulsesperrev = PULSESPERREV;
  DriveStatus driveStatus = STOPPED;

  void pulse() {
    digitalWrite(PULSE_PIN, HIGH);
    delayMicroseconds(PULSE_DELAY_US);
    digitalWrite(PULSE_PIN, LOW);
    delayMicroseconds(PULSE_DELAY_US);
  }

  void update_input_button(bool read, Button& btn) {
    // if button is released, set status and prev_duration
    if (read) {
      btn.status = RELEASED;
      if (btn.duration > 0) {
        btn.prev_duration = millis() - btn.duration;
        btn.duration = 0;
      }
    } else {
      // if button is pressed, record current time and set status
      if (btn.status == RELEASED) {
        btn.duration = millis();
      }
      btn.status = PRESSED;
    }
  }

  void update_buttons() {

    // set variables for each available button
    update_input_button(digitalRead(UP_PIN), buttons.up);
    update_input_button(digitalRead(DOWN_PIN), buttons.down);
    update_input_button(digitalRead(TOP_LIMITER_PIN), buttons.top_limiter);
    update_input_button(digitalRead(BOTTOM_LIMITER_PIN), buttons.bottom_limiter);

    // if  all buttons are released, disable button ignore
    if (buttons.up.status == RELEASED && buttons.down.status == RELEASED && buttons.ignore) {
      buttons.ignore = false;
      Serial.println("Ignore is off");
      buttons.up.prev_duration = 0;
      buttons.down.prev_duration = 0;
    }

  }

  void update() {
    update_buttons();

    // if one of the limiter is pressed, ignore buttons and drive in the other direction
    if (buttons.top_limiter.status || buttons.bottom_limiter.status) {
      buttons.ignore = true;
      digitalWrite(ENABLE_PIN, LOW);
      while (buttons.top_limiter.status || buttons.bottom_limiter.status) {
        update_buttons();
        digitalWrite(DIRECTION_PIN, buttons.top_limiter.status ? DIRECTION_DOWN : DIRECTION_UP);
        pulse();
      }
      digitalWrite(ENABLE_PIN, HIGH);
      driveStatus = STOPPED;
    }

    //if opposite button of automatic direction is pressed, or both buttons are pressed, stop
    if ( (driveStatus == UP_TILL_STOP && buttons.down.status == PRESSED) ||
         (driveStatus == DOWN_TILL_STOP && buttons.up.status == PRESSED) ||
         (buttons.down.status == PRESSED && buttons.up.status == PRESSED)) {
      driveStatus = STOPPED;
      buttons.ignore = true;
      Serial.println("Ignore is on");
    }
    //dont change status if buttons are supposed to be ignored
    if (buttons.ignore) {
      return;
    }

    // if button is pressed and the time pressed is longer than the specified short_press threshold, set to manual
    if (buttons.up.duration > 0 && millis() - buttons.up.duration > (TYPE_TIME)SHORT_PRESS_MS) {
      driveStatus = UP_MANUAL;
    }
    else if (buttons.down.duration > 0 && millis() - buttons.down.duration > (TYPE_TIME)SHORT_PRESS_MS) {
      driveStatus = DOWN_MANUAL;
    }
    // if button is released and the time pressed is shorter than short_press threshold, set mode to automatic
    else if (buttons.up.prev_duration > 0 && buttons.up.prev_duration <= (TYPE_TIME)SHORT_PRESS_MS) {
      driveStatus = UP_TILL_STOP;
    }
    else if (buttons.down.prev_duration > 0 && buttons.down.prev_duration <= (TYPE_TIME)SHORT_PRESS_MS) {
      driveStatus = DOWN_TILL_STOP;
    }

    // resetting durations after having dealt with them
    buttons.up.prev_duration = 0;
    buttons.down.prev_duration = 0;

    // stop driving if manual mode is selected and button is released
    if ((driveStatus == UP_MANUAL && buttons.up.status == RELEASED) ||
        (driveStatus == DOWN_MANUAL && buttons.down.status == RELEASED)) {
      driveStatus = STOPPED;
    }

  }

  void drive() {
    if (driveStatus == STOPPED) {
      digitalWrite(ENABLE_PIN, HIGH);
      return;
    }
  
    if (driveStatus == UP_MANUAL || driveStatus == UP_TILL_STOP) {
      digitalWrite(ENABLE_PIN, LOW);
      digitalWrite(DIRECTION_PIN, DIRECTION_UP);
      // always do a quarter rev at once
      for (int i = 0; i < 20; i++) {
        pulse();
      }
    }
    else if (driveStatus == DOWN_MANUAL || driveStatus == DOWN_TILL_STOP) {
      digitalWrite(ENABLE_PIN, LOW);
      digitalWrite(DIRECTION_PIN, DIRECTION_DOWN);
      // we want a response time of < 20 ms.
      // we spend 1 ms per pulse (at 100us width)
      // so we cannot do more than 20 pulses at once
      for (int i = 0; i < 100; i++) {
        pulse();
      }
    }

  }

};

Controller controller;

void setup() {
  Serial.begin(115200);
  Serial.println("Code available at: https://github.com/Benimautner/platform_move");

  pinMode(PULSE_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);

  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(TOP_LIMITER_PIN, INPUT_PULLUP);
  pinMode(BOTTOM_LIMITER_PIN, INPUT_PULLUP);

  digitalWrite(ENABLE_PIN, HIGH);
}

void loop() {
  controller.update();
  controller.drive();

  /*
    Serial.println("Drive status: " + String(controller.driveStatus));
    Serial.println("Ignore: " + String(controller.buttons.ignore));
    Serial.println("Btn up: " + String(controller.buttons.up));
    Serial.println("Btn down: " + String(controller.buttons.down));
    Serial.println("Btn up duration: " + String(controller.buttons.up_duration));
    Serial.println("Btn down duration: " + String(controller.buttons.down_duration));
    Serial.println("Btn up prev: " + String(controller.buttons.up_prev_duration));
    Serial.println("Btn down prev: " + String(controller.buttons.down_prev_duration));
    Serial.flush();
  */

}
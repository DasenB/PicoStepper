# PicoStepper
A Library to drive stepper motors using the Raspberry Pi Pico (RP2040)

## Installation
Add the `picostepper` folder to your project and adjust your `CMakeLists.txt` file on the basis of the `CMakeLists.txt` file in this repository. Then include `picostepper.h` in your code.

## Examples
## Blocking Movement

```c
#include "picostepper.h"

int main() {
  uint base_pin = 10;
  PicoStepper device = picostepper_init(base_pin, FourWireDriver);

  uint steps = 4000;
  bool direction = true;
  uint delay = 1;
  int delay_change = 0;

  while (true)
  {
    picostepper_move_blocking(device, steps, direction, delay, delay_change );
    sleep_ms(5000);
  }

}
```

## Non-Blocking Movement

```c
#include "picostepper.h"

void movement_finished(PicoStepper device) {
  picostepper_move_async(device, 100, &movement_finished);
}

int main() {
  uint base_pin = 10;
  PicoStepper device = picostepper_init(base_pin, FourWireDriver);

  uint delay = 5;
  bool direction = true;
  bool enabled = true;
  uint steps = 10;


  picostepper_set_async_delay(device, delay);
  picostepper_set_async_direction(device, direction);
  picostepper_set_async_enabled(device, enabled);

  picostepper_move_async(device, steps, &movement_finished);

  while (true) {
    // Accelerate
    for (size_t i = 100; i > 0; i--)
    {
      picostepper_set_async_delay(device, i);
      sleep_ms(10);
    }

    //Deaccelerate
    for (size_t i = 0; i < 100; i++)
    {
      picostepper_set_async_delay(device, i);
      sleep_ms(10);
    }
  }
}
```


# Drivers
- The `FourWireDriver` is used to control drivers that require a direction-signal (DIR), an inverted direction-signal (!DIR), a step-signal (PUL) and an inverted step-signal (!PUL). 
- The `TwoWireDriver` is used to control drivers ghat require a direction-signal (DIR) and a step-signal (STEP).

Currently only two drivers are implemented. 
Other devices can be supported easily by creating a corresponding PIO-program for the signal-generation. Pull requests are highly welcome.

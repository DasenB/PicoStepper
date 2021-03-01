/**
 * Copyright (c) 2021 Bjarne Dasenbrook
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
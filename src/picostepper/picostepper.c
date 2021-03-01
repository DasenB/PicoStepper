/**
 * Copyright (c) 2021 Bjarne Dasenbrook
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "picostepper.h"

// Object containing all status-data and device-handles
struct PicoStepperContainer psc;
bool psc_is_initialised = false;


static void picostepper_async_handler() {
  // Safe interrupt value and clear the interrupt
  uint32_t interrupt_request = dma_hw->ints0;
  dma_hw->ints0 = interrupt_request;

  // Find the channel that invoked the interrupt
  for (size_t bit_position = 0; bit_position < 15; bit_position++)
  {
    int flag = interrupt_request & (1 << bit_position);
    if(flag == 0) {
      continue;
    }

    int dma_channel = bit_position;
    if (dma_channel == -1)
    {
      continue;
    }
    PicoStepper device = (PicoStepper) psc.map_dma_ch_to_device_index[dma_channel];
    // Invoke callback for device
    psc.devices[device].is_running = false;
    if(psc.devices[device].callback != NULL) {
      (*psc.devices[device].callback)(device);
    }
  }
}

// Create a PicoStepperRawDevice
static PicoStepperRawDevice picostepper_create_raw_device() {
  PicoStepperRawDevice psrq;
  psrq.is_configured = false;
  psrq.is_running = false;
  psrq.direction = true;
  psrq.steps = 0;
  psrq.enabled = false;
  psrq.command = 0;
  psrq.pio_id = -1;
  psrq.callback = NULL;
  psrq.dma_config = dma_channel_get_default_config(0);
  psrq.delay = 1;
  return psrq;
}

// Create the PicoStepperContainer if yet not created
static void picostepper_psc_init() {
  if(psc_is_initialised) {
    return;
  }
  psc.max_device_count = 8;
  for (size_t i = 0; i < psc.max_device_count; i++)
  {
    psc.device_with_index_is_in_use[i] = false;
    psc.devices[i] = picostepper_create_raw_device();
  }
  for (size_t i = 0; i < 32; i++)
  {
    psc.map_dma_ch_to_device_index[i] = -1;
  }
  psc_is_initialised = true;
  return;
}

// Allocate hardware resources for a stepper
static PicoStepper picostepper_init_unclaimed_device() {
  // Initialize the PicoStepperContainer (if neccessary)
  picostepper_psc_init();
  // Select an PicoStepper device that is not in use (if possible)
  int unclaimed_device_index = -1;
  for (size_t i = 0; i < psc.max_device_count; i++)
  {
    if(!psc.device_with_index_is_in_use[i]) {
      unclaimed_device_index = i;
      break;
    }
  }
  // Error: No free resources to use for the stepper
  if (unclaimed_device_index == -1)
  {
    return (PicoStepper) -1;
  }
  // Select a PIO-Block and statemachine to generate the signal on
  PIO pio_block = pio0;
  int pio_id = 0;
  uint statemachine = pio_claim_unused_sm(pio_block, false);
  // Error: No free resources to use for the stepper
  if(statemachine == -1) {
    pio_block = pio1;
    pio_id = 1;
    statemachine = pio_claim_unused_sm(pio_block, true);
  }
  // Select a DMA-Channel and configure it
  int dma_ch = dma_claim_unused_channel(true);
  dma_channel_config dma_conf = dma_channel_get_default_config(dma_ch);
  channel_config_set_transfer_data_size(&dma_conf, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_conf, false);
  // DREQ_PIO0_TX0 => 0x0, DREQ_PIO0_TX1 => 0x1, DREQ_PIO0_TX2 => 0x2, DREQ_PIO0_TX3 => 0x3
  // DREQ_PIO1_TX0 => 0x8, DREQ_PIO1_TX1 => 0x9, DREQ_PIO1_TX2 => 0xa, DREQ_PIO1_TX3 => 0xb
  uint drq_source = (pio_id * 8) + statemachine;
  channel_config_set_dreq(&dma_conf, drq_source);
  dma_channel_set_irq0_enabled(dma_ch, true);
  irq_set_exclusive_handler(DMA_IRQ_0, picostepper_async_handler);
  irq_set_enabled(DMA_IRQ_0, true);
  // Update the state of the device and mark it as allocated
  psc.map_dma_ch_to_device_index[dma_ch] = unclaimed_device_index;
  psc.device_with_index_is_in_use[unclaimed_device_index] = true;
  psc.devices[unclaimed_device_index].pio = pio_block;
  psc.devices[unclaimed_device_index].pio_id = pio_id;
  psc.devices[unclaimed_device_index].statemachine = statemachine;
  psc.devices[unclaimed_device_index].dma_channel = dma_ch;
  psc.devices[unclaimed_device_index].dma_config = dma_conf;
  psc.devices[unclaimed_device_index].is_running = false;
  psc.devices[unclaimed_device_index].is_configured = true; 
  return (PicoStepper) unclaimed_device_index;
}

PicoStepper picostepper_init(uint base_pin, PicoStepperMotorType driver) {
  // Choose the programm to generate the steps
  const pio_program_t * picostepper_driver_program;
  picostepper_driver_program = &picostepper_four_wire_program;
  switch(driver){
    case FourWireDriver: 
      picostepper_driver_program = &picostepper_four_wire_program;
      break;

    // Other drivers are not yet implemented
    default:  return -1;
  }
  // Create picostepper object and claim pio resources
  PicoStepper device = picostepper_init_unclaimed_device();
  // Load the driver program into the pio instruction memory
  uint picostepper_program_offset = pio_add_program(psc.devices[device].pio, picostepper_driver_program);
  // Init PIO program

  switch(driver){
    case FourWireDriver:   
      picostepper_four_wire_program_init(psc.devices[device].pio, psc.devices[device].statemachine, picostepper_program_offset, base_pin);
      break;
      
    // Other drivers are not yet implemented
    default:  return -1;
  }
  
  
  return device;
}

// Move the stepper and wait for it to finish before returning from the function
// Warning: Care must be taken whan configurating a delay_change value to
//          prevent integer under or overflows in the resulting delay!!! 
bool picostepper_move_blocking(PicoStepper device, uint steps, bool direction, uint delay, int delay_change) {
  //Error: picostepper delay to big (greater than (2^30)-1
  if (delay > 1073741823) {
      return false;
  }
  // Error: No valid PicoStepper device
  if(device == -1) {
    return false;
  }
  // Error: Device is already running.
  if(psc.devices[device].is_running) {
    return false;
  }
  // Set device status to running
  psc.devices[device].is_running = true;
  // For each step submit a step command to the pio
  uint32_t command;
  uint calculated_delay = delay;
  for (size_t i = 0; i < steps; i++)
  {
    command = (((calculated_delay << 1) | direction) << 1 ) | 1;
    pio_sm_put_blocking(psc.devices[device].pio, psc.devices[device].statemachine, command);
    calculated_delay += delay_change;
  }
  // Wait until the statemachine has consumed all commands from the buffer
  while (pio_sm_is_tx_fifo_empty(psc.devices[device].pio, psc.devices[device].statemachine) == false);
  // Set device status to not running
  psc.devices[device].is_running = false;
  return true;
}

// Set the direction used by picostepper_move_async.
// Can be set during a running async movement.
void picostepper_set_async_direction(PicoStepper device, bool direction) {
  psc.devices[device].direction = direction;
  psc.devices[device].command = (((psc.devices[device].delay << 1) | psc.devices[device].direction) << 1 )| psc.devices[device].enabled;
}

// Set the enabled state for step-commands send by picostepper_move_async
// Can be set during a running async movement.
void picostepper_set_async_enabled(PicoStepper device, bool enabled) {
  psc.devices[device].enabled = enabled;
  psc.devices[device].command = (((psc.devices[device].delay << 1) | psc.devices[device].direction) << 1 )| psc.devices[device].enabled;
}

// Set the delay between steps used by picostepper_move_async.
// Can be set during a running async movement.
void picostepper_set_async_delay(PicoStepper device, uint delay) {
  psc.devices[device].delay = delay;
  psc.devices[device].command = (((psc.devices[device].delay << 1) | psc.devices[device].direction) << 1 )| psc.devices[device].enabled;
}

int picostepper_convert_speed_to_delay(float steps_per_second) {
  return 0;
}



// Move the stepper ans imidiatly return from function without waiting for the movement to finish
bool picostepper_move_async(PicoStepper device, int steps, PicoStepperCallback func) {

  if(psc.devices[device].is_running || !psc.devices[device].is_configured ) {
    return false;
  }

  psc.devices[device].callback = func;
    
  if(psc.devices[device].pio_id == 0) {
    dma_channel_configure(
        psc.devices[device].dma_channel,
        &psc.devices[device].dma_config,
        &pio0_hw->txf[psc.devices[device].statemachine], // Write address (only need to set this once)
        NULL,             // Don't provide a read address yet
        steps,            // Write the same value many times, then halt and interrupt
        false             // Don't start yet
    );
  } else {
    dma_channel_configure(
        psc.devices[device].dma_channel,
        &psc.devices[device].dma_config,
        &pio1_hw->txf[psc.devices[device].statemachine], // Write address (only need to set this once)
        NULL,             // Don't provide a read address yet
        steps,            // Write the same value many times, then halt and interrupt
        false             // Don't start yet
    );
  }
  // Set read address for the dma (switching between two buffers has not been implemented) and start transmission
  dma_channel_set_read_addr(psc.devices[device].dma_channel, &psc.devices[device].command, true);
  psc.devices[device].is_running = true;

  return true;
}




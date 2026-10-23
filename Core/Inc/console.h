#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stdint.h>

/* Console transport for printf output and command input.
 *
 *   default build : USART2 (blocking TX, interrupt RX) - unchanged behaviour
 *   -DCONSOLE_USB : USB CDC (ring-buffered, non-blocking TX; RX via the
 *                   USB interrupt). Selected with cmake -DCONSOLE_USB=ON.
 *
 * Never use CONSOLE_USB together with RENODE_BUILD: the simulation talks
 * to USART2. */

/* Queue bytes for transmission. Safe from main loop and ISR context.
 * USB build: never blocks; a write that does not fit in the ring is
 * dropped whole (counted in console_dropped_bytes()), and output is
 * discarded while no host has the device configured. */
void console_write(const uint8_t *data, uint16_t len);

/* Call once per main-loop iteration (before the watchdog refresh).
 * Pushes queued bytes to the USB endpoint. No-op in the UART build. */
void console_poll(void);

/* USB build only. Hooks for the generated usbd_cdc_if.c (see USER CODE
 * blocks): received bytes (USB ISR context), and DTR changes. */
void console_usb_rx(const uint8_t *data, uint32_t len);
void console_usb_control_line_state(uint16_t wValue);

/* True once after the host opens the port (DTR rising edge). Lets the
 * application re-send its banner, which is otherwise lost because the
 * device boots and prints before the host has enumerated it. */
bool console_take_open_event(void);

uint32_t console_dropped_bytes(void);

#endif /* CONSOLE_H */
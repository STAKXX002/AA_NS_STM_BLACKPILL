#ifndef COMMANDS_H
#define COMMANDS_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

/* Call once at startup, after MX_USART2_UART_Init(). Stores the UART
 * handle and arms the first receive-interrupt. Pass NULL when input
 * arrives through another transport (USB CDC) via commands_feed(). */
void commands_init(UART_HandleTypeDef *huart);

/* Call once per main-loop iteration. If a full line has been received,
 * looks it up in the command table and dispatches it. */
void commands_update(uint32_t now);

/* Feed one received byte into the line parser. Called from the UART RX
 * callback and, in the USB build, from the USB receive interrupt. */
void commands_feed(uint8_t c);

/* True once at least one complete line has been dispatched since boot. */
bool commands_seen_any(void);

#endif /* COMMANDS_H */
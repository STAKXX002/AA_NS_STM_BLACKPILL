#include "console.h"
#include "commands.h"
#include "critical.h"
#include "main.h"

#ifdef CONSOLE_USB
#include "usb_device.h"   /* hUsbDeviceFS */
#include "usbd_cdc.h"     /* USBD_CDC_HandleTypeDef */
#include "usbd_cdc_if.h"  /* CDC_Transmit_FS */

#define TX_SIZE  1024u    /* power of two */
#define TX_MASK  (TX_SIZE - 1u)
#define TX_CHUNK 256u     /* max bytes handed to the USB stack at once */

/* Single ring buffer. Producers (main loop, optionally ISRs) advance
 * tx_head inside a critical section. Only console_poll() advances
 * tx_tail, and only after the USB stack reports the previous transfer
 * finished: the endpoint DMA/FIFO reads straight from tx_buf, so bytes
 * in flight must stay reserved until then. */
static uint8_t           tx_buf[TX_SIZE];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static volatile uint16_t tx_inflight;
static volatile uint32_t dropped;
static volatile bool     open_event;
extern USBD_HandleTypeDef hUsbDeviceFS;

static bool usb_configured(void) {
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

void console_write(const uint8_t *data, uint16_t len) {
    if (!usb_configured() || len == 0u) return;

    CRITICAL_ENTER();
    uint16_t used = (uint16_t)((tx_head - tx_tail) & TX_MASK);
    uint16_t room = (uint16_t)(TX_MASK - used);
    if (len > room) {
        dropped += len;
    } else {
        for (uint16_t i = 0; i < len; i++) {
            tx_buf[(tx_head + i) & TX_MASK] = data[i];
        }
        tx_head = (uint16_t)((tx_head + len) & TX_MASK);
    }
    CRITICAL_EXIT();
}

void console_poll(void) {
    if (!usb_configured()) {
        /* Host went away: discard stale output instead of replaying it. */
        CRITICAL_ENTER();
        tx_tail = tx_head;
        tx_inflight = 0;
        CRITICAL_EXIT();
        return;
    }

    USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hcdc == NULL || hcdc->TxState != 0u) return;   /* transfer still running */

    uint16_t n;
    uint16_t start;
    CRITICAL_ENTER();
    tx_tail = (uint16_t)((tx_tail + tx_inflight) & TX_MASK);   /* previous transfer done */
    tx_inflight = 0;
    start = tx_tail;
    uint16_t used = (uint16_t)((tx_head - tx_tail) & TX_MASK);
    uint16_t contiguous = (uint16_t)(TX_SIZE - tx_tail);
    n = (used < contiguous) ? used : contiguous;
    if (n > TX_CHUNK) n = TX_CHUNK;
    CRITICAL_EXIT();

    if (n == 0u) return;
    if (CDC_Transmit_FS(&tx_buf[start], n) == USBD_OK) {
        tx_inflight = n;
    }
}

void console_usb_rx(const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        commands_feed(data[i]);
    }
}

void console_usb_control_line_state(uint16_t wValue) {
    if (wValue & 0x0001u) {   /* bit 0 = DTR */
        open_event = true;
    }
}

bool console_take_open_event(void) {
    CRITICAL_ENTER();
    bool e = open_event;
    open_event = false;
    CRITICAL_EXIT();
    return e;
}

uint32_t console_dropped_bytes(void) { return dropped; }

#else /* UART build: behaviour identical to the previous _write() */

extern UART_HandleTypeDef huart2;

void console_write(const uint8_t *data, uint16_t len) {
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100);
}

void console_poll(void) {}
void console_usb_rx(const uint8_t *data, uint32_t len) { (void)data; (void)len; }
void console_usb_control_line_state(uint16_t wValue) { (void)wValue; }
bool console_take_open_event(void) { return false; }
uint32_t console_dropped_bytes(void) { return 0u; }

#endif
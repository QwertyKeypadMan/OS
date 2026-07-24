#include "kernel/mouse.h"

#include <stdint.h>

#include "kernel/io.h"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

#define PS2_STATUS_OUTPUT_FULL 0x01
#define PS2_STATUS_INPUT_FULL 0x02
#define PS2_STATUS_AUX_DATA 0x20

#define PS2_CMD_ENABLE_AUX 0xA8
#define PS2_CMD_READ_CONFIG 0x20
#define PS2_CMD_WRITE_CONFIG 0x60
#define PS2_CMD_WRITE_MOUSE 0xD4

#define MOUSE_ACK 0xFA
#define MOUSE_SET_DEFAULTS 0xF6
#define MOUSE_ENABLE_REPORTING 0xF4
#define MOUSE_DISABLE_REPORTING 0xF5

static bool mouse_ready;
static uint8_t packet_bytes[3];
static int packet_index;

static bool wait_input_clear(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) == 0) {
            return true;
        }
        __asm__ __volatile__("pause");
    }
    return false;
}

static bool wait_output_full(void)
{
    for (uint32_t i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) != 0) {
            return true;
        }
        __asm__ __volatile__("pause");
    }
    return false;
}

static bool controller_command(uint8_t command)
{
    if (!wait_input_clear()) {
        return false;
    }
    outb(PS2_COMMAND_PORT, command);
    return true;
}

static bool controller_data(uint8_t data)
{
    if (!wait_input_clear()) {
        return false;
    }
    outb(PS2_DATA_PORT, data);
    return true;
}

static void flush_output(void)
{
    for (uint32_t i = 0; i < 64; i++) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) == 0) {
            return;
        }
        (void)inb(PS2_DATA_PORT);
    }
}

static bool mouse_write(uint8_t value)
{
    if (!controller_command(PS2_CMD_WRITE_MOUSE)) {
        return false;
    }

    if (!controller_data(value)) {
        return false;
    }

    if (!wait_output_full()) {
        return false;
    }

    return inb(PS2_DATA_PORT) == MOUSE_ACK;
}

bool mouse_initialize(void)
{
    mouse_ready = false;
    packet_index = 0;

    flush_output();

    if (!controller_command(PS2_CMD_ENABLE_AUX)) {
        return false;
    }

    if (!controller_command(PS2_CMD_READ_CONFIG) || !wait_output_full()) {
        return false;
    }

    uint8_t config = inb(PS2_DATA_PORT);
    config &= (uint8_t)~0x20;
    config &= (uint8_t)~0x02;

    if (!controller_command(PS2_CMD_WRITE_CONFIG) || !controller_data(config)) {
        return false;
    }

    if (!mouse_write(MOUSE_SET_DEFAULTS)) {
        return false;
    }

    if (!mouse_write(MOUSE_ENABLE_REPORTING)) {
        return false;
    }

    mouse_ready = true;
    return true;
}

void mouse_shutdown(void)
{
    if (mouse_ready) {
        flush_output();
        (void)mouse_write(MOUSE_DISABLE_REPORTING);
    }

    flush_output();
    mouse_ready = false;
    packet_index = 0;
}

bool mouse_poll(mouse_packet_t *packet)
{
    if (!mouse_ready || packet == 0) {
        return false;
    }

    uint8_t status = inb(PS2_STATUS_PORT);
    if ((status & PS2_STATUS_OUTPUT_FULL) == 0 || (status & PS2_STATUS_AUX_DATA) == 0) {
        return false;
    }

    uint8_t byte = inb(PS2_DATA_PORT);
    if (packet_index == 0 && (byte & 0x08) == 0) {
        return false;
    }

    packet_bytes[packet_index++] = byte;
    if (packet_index < 3) {
        return false;
    }

    packet_index = 0;
    packet->dx = (int)(int8_t)packet_bytes[1];
    packet->dy = (int)(int8_t)packet_bytes[2];
    packet->left_button = (packet_bytes[0] & 0x01) != 0;
    packet->right_button = (packet_bytes[0] & 0x02) != 0;
    packet->middle_button = (packet_bytes[0] & 0x04) != 0;
    packet->overflow = (packet_bytes[0] & 0xC0) != 0;
    return true;
}

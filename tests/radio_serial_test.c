#include "radio.h"
#include "radio_queue.h"

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/termios.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s failed\n", __FILE__, __LINE__, #expr); \
        exit(1); \
    } \
} while (0)

static void pause_ms(int ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void read_bytes(int fd, uint8_t *data, size_t length)
{
    while (length > 0) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        CHECK(poll(&pfd, 1, 3000) == 1);
        ssize_t n = read(fd, data, length);
        CHECK(n > 0);
        data += n;
        length -= (size_t)n;
    }
}

static size_t read_packet(int fd, uint8_t *packet, lg_ccsds_profile_view_t *view)
{
    read_bytes(fd, packet, CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE);
    size_t rest = ((size_t)packet[4] << 8 | packet[5]) + 1;
    CHECK(rest + CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE <= RADIO_MAX_PACKET_SIZE);
    read_bytes(fd, packet + CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE, rest);
    size_t length = rest + CCSDS_SPACE_PACKET_PRIMARY_HEADER_SIZE;
    CHECK(lg_ccsds_profile_parse(packet, length, view) == CCSDS_OK);
    return length;
}

static void wait_for_queue(void)
{
    for (int i = 0; i < 300 && radio_queue_pending() != 0; i++) pause_ms(10);
    CHECK(radio_queue_pending() == 0);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    CHECK(unsetenv("E22_UART_DEVICE") == 0);
    CHECK(unsetenv("E22_SWAPPED_UART") == 0);
    CHECK(setenv("E22_TX_DELAY_MS", "0", 1) == 0);
    uint8_t received[RADIO_MAX_PAYLOAD];

    if (strcmp(argv[1], "roundtrip") != 0) {
        if (strcmp(argv[1], "invalid-device") == 0) {
            CHECK(setenv("E22_UART_DEVICE", "/dev/radiohandler-nonexistent", 1) == 0);
        } else if (strcmp(argv[1], "swapped-uart") == 0) {
            CHECK(setenv("E22_SWAPPED_UART", "1", 1) == 0);
        } else if (strcmp(argv[1], "invalid-delay") == 0) {
            CHECK(setenv("E22_TX_DELAY_MS", "-1", 1) == 0);
        } else {
            CHECK(strcmp(argv[1], "missing-device") == 0);
        }
        CHECK(radio_send((const uint8_t *)"x", 1) == -1);
        CHECK(radio_receive(received, sizeof(received)) == -1);
        CHECK(radio_queue_pending() == 0);
        return 0;
    }

    // A pseudo-terminal acts as the USB serial adapter, with no radio hardware.
    int master, slave;
    char device[128];
    CHECK(openpty(&master, &slave, device, NULL, NULL) == 0);
    CHECK(setenv("E22_UART_DEVICE", device, 1) == 0);
    CHECK(radio_receive(NULL, sizeof(received)) == -1);
    CHECK(radio_receive(received, 0) == -1);
    CHECK(radio_receive(received, sizeof(received)) == 0); // initialize + idle timeout

    struct termios tty;
    CHECK(tcgetattr(slave, &tty) == 0);
    CHECK(cfgetispeed(&tty) == B9600 && cfgetospeed(&tty) == B9600);
    CHECK((tty.c_cflag & (CSIZE | PARENB | CSTOPB | CRTSCTS)) == CS8);
    CHECK((tty.c_lflag & (ICANON | ECHO)) == 0);

    uint8_t payload[RADIO_MAX_PAYLOAD + 17];
    for (size_t i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)i;
    CHECK(radio_send(payload, sizeof(payload)) == 0);
    uint8_t first[RADIO_MAX_PACKET_SIZE], second[RADIO_MAX_PACKET_SIZE];
    lg_ccsds_profile_view_t view;
    size_t first_length = read_packet(master, first, &view);
    CHECK(view.payload_length == RADIO_MAX_PAYLOAD);
    CHECK(memcmp(view.payload, payload, view.payload_length) == 0);
    uint16_t sequence = view.metadata.sequence_count;
    size_t second_length = read_packet(master, second, &view);
    CHECK(view.payload_length == 17);
    CHECK(memcmp(view.payload, payload + RADIO_MAX_PAYLOAD, 17) == 0);
    wait_for_queue();

    CHECK(radio_resend(LG_CCSDS_APID_RAW_TELEMETRY, sequence) == 0);
    uint8_t resent[RADIO_MAX_PACKET_SIZE];
    CHECK(read_packet(master, resent, &view) == first_length);
    CHECK(memcmp(resent, first, first_length) == 0);
    wait_for_queue();

    // Reject a bad CRC, resync past garbage, then receive both good packets.
    uint8_t corrupt[RADIO_MAX_PACKET_SIZE];
    memcpy(corrupt, first, first_length);
    corrupt[first_length - 1] ^= 1;
    CHECK(write(master, corrupt, first_length) == (ssize_t)first_length);
    CHECK(write(master, "\xff\xff", 2) == 2);
    CHECK(write(master, first, first_length) == (ssize_t)first_length);
    CHECK(write(master, second, second_length) == (ssize_t)second_length);
    CHECK(radio_receive(received, sizeof(received)) == RADIO_MAX_PAYLOAD);
    CHECK(memcmp(received, payload, RADIO_MAX_PAYLOAD) == 0);
    CHECK(radio_receive(received, sizeof(received)) == 17);
    CHECK(memcmp(received, payload + RADIO_MAX_PAYLOAD, 17) == 0);

    // A partial packet times out and is discarded; the next packet still works.
    CHECK(write(master, first, 5) == 5);
    CHECK(radio_receive(received, sizeof(received)) == 0);
    CHECK(write(master, second, second_length) == (ssize_t)second_length);
    CHECK(radio_receive(received, sizeof(received)) == 17);

    radio_stats_t stats;
    radio_get_stats(&stats);
    CHECK(stats.packets_sent == 3 && stats.packets_resent == 1);
    CHECK(stats.packets_dropped == 0);
    CHECK(stats.rx_packets == 3 && stats.rx_invalid >= 1);
    CHECK(stats.rx_bytes_skipped >= first_length + 7);

    close(slave);
    close(master);
    CHECK(radio_receive(received, sizeof(received)) == -1);

    // Reconnect on a new adapter path after the initialization retry interval.
    CHECK(openpty(&master, &slave, device, NULL, NULL) == 0);
    CHECK(setenv("E22_UART_DEVICE", device, 1) == 0);
    pause_ms(1100);
    CHECK(radio_receive(received, sizeof(received)) == 0);
    CHECK(write(master, second, second_length) == (ssize_t)second_length);
    CHECK(radio_receive(received, sizeof(received)) == 17);
    CHECK(memcmp(received, payload + RADIO_MAX_PAYLOAD, 17) == 0);
    close(slave);
    close(master);
    return 0;
}

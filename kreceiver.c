#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

int main(int argc, char** argv) {
    msg t, *r;
    int timeout_error = 0;
    int transmission_ended = 0;
    int data_len = 0;

    packet *pkt = calloc(1, sizeof(packet));
    packet *response = NULL;

    char file_name[20] = {'\0'};
    char prefix[5] = "recv_";
    int output_file;

    int last_seq = -1;

    init(HOST, PORT);

    /*
        While receiver did not receive timeout error
        for more than three times and there are packets
        to receive from the sender, wait for packets
        continue transmission
    */
    while ((timeout_error <= 3) && (!transmission_ended)) {
        r = receive_message_timeout(5000);
        if (r == NULL) {
            /*
                Count the number of consecutive timeout errors
                If the number is more than three, interrupt transmission
            */
            perror("RECEIVER TIMEOUT ERROR\n");
            ++timeout_error;
            if (timeout_error > 3) {
                if (response == NULL) {
                    printf("[%s] stopped because of timeout on packet [%u]\n", argv[0], 1);
                } else {
                    printf("[%s] stopped because of timeout on packet [%u]\n", argv[0], next_seq(last_seq));
                }

                return -1;
            }
            /*
                If the number of consecutive timeout errors
                is less than three, resend the last response
                with the same sequence number.

                If response is null, it means the receiver
                has not received Send-Init yet.
            */
            if (response != NULL) {
                packet_to_payload(response, &t, data_len);
                send_message(&t);
            }
        } else {
            // Reset the number of consecutive timeout errors
            timeout_error = 0;
            // Copy payload to received packet fields
            payload_to_packet(r, pkt);

            /*
                If the packet received is not the packet
                the receiver was waiting for, resend the
                last response with the same sequence number                
            */
            if (pkt->head.seq != (next_seq(last_seq))) {
                /*
                    If response is null, it means the receiver
                    has not received Send-Init yet.
                */
                if (response != NULL) {
                    packet_to_payload(response, &t, data_len);
                    send_message(&t);
                }
                continue;
            }

            /*
                First response the receiver gives
                (for packet Send-Init)
            */
            if (response == NULL) {
                response = calloc(1, sizeof(packet));
            }

            data_len = 0;
            unsigned short crc = crc16_ccitt(r->payload, r->len - TAIL_SIZE);

            /*
                Verify if the received packet is corrupt
            */
            if (crc == pkt->end.check) {
                /*
                    Packet is not corrupt, send ACK
                */

                /*
                    For packet Send-Init, response data field
                    is the same as Send-Init data
                */
                if (pkt->head.type == SEND_INIT) {
                    data_len = r->len - DATA_OFFSET - TAIL_SIZE;
                }

                /*
                    For File-Header, open output file
                */
                if (pkt->head.type == FILE_HEADER) {
                    memset(file_name, 0, sizeof(file_name));
                    strncpy(file_name, prefix, 5);
                    strcat(file_name, (char *)pkt->data);
                    output_file = open(file_name, O_CREAT | O_TRUNC | O_WRONLY, 0666);
                    if (output_file == -1) {
                        perror("Error opening output file\n");
                        return -1;
                    }
                }

                /*
                    Write data from Data packet to output file
                */
                if (pkt->head.type == DATA) {
                    if (pkt->head.seq != last_seq - 1) {
                        write(output_file, pkt->data, r->len - DATA_OFFSET - TAIL_SIZE);
                    }
                }

                /*
                    Received EOF
                    Close output file
                */
                if (pkt->head.type == END_OF_FILE) {
                    if (pkt->head.seq != last_seq - 1) {
                        close(output_file);
                    }
                }

                /*
                    Received EOT
                    End transmission
                */
                if (pkt->head.type == END_OF_TRANSMISSION) {
                    transmission_ended = 1;
                }
                // Sequence number for last sent response
                last_seq = next_seq(pkt->head.seq);

                /*
                    Fill ACK packet fields
                */
                fill_packet_fields(response, last_seq, ACK);

                /*
                    Fill response data field
                    If it is for Send-Init, it has its data,
                    otherwise it is null
                */
                response->data = (uchar *)calloc(data_len, sizeof(uchar));
                memcpy(response->data, pkt->data, data_len);

                /*
                    Copy response packet to payload
                    Send message to the sender
                */
                packet_to_payload(response, &t, data_len);
                send_message(&t);
            } else {
                // Sequence number for last sent response
                last_seq = next_seq(pkt->head.seq);

                /*
                    Fill NAK packet fields
                */
                fill_packet_fields(response, last_seq, NAK);

                /*
                    Copy response packet to payload
                    Send message to the sender
                */
                packet_to_payload(response, &t, data_len);
                send_message(&t);
            }
        }
    }
	
	return 0;
}

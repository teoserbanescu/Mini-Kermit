#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000


int main(int argc, char** argv) {
    int timeout_error = 0;
    int file_headers_sent = 0;
    int transmission_ended = 0;
    int data_len = 0, last_seq = 0;
    msg t;
    int input_file, eof_flag;

    packet *response = calloc(1, sizeof(packet));

    init(HOST, PORT);


    /*
        Fill Send-init packet fields
    */
    packet *pkt = calloc(1, sizeof(packet));
    fill_packet_fields(pkt, 0, SEND_INIT);
    data_len = sizeof(send_init_data);
    pkt->head.len = DATA_OFFSET + data_len + TAIL_SIZE - 2;

    /*
        Fill Send-init data fields
    */
    send_init_data *S_data = calloc(1, data_len);
    S_data->maxl = MAXL;
    S_data->timeout = TIME;
    S_data->eol = EOL;
    pkt->data = calloc(1, data_len);
    memcpy(pkt->data, S_data, data_len);

    /*
        Copy Send-init packet fields to payload
        Send Send-Init message to receiver
    */
    packet_to_payload(pkt, &t, data_len);
    send_message(&t);

    /*
        While sender did not receive timeout error
        for more than three times and there are packets
        to send to the receiver, wait for ACK/NAK and
        continue transmission
    */
    while ((timeout_error <= 3) && (!transmission_ended)) {
        msg *y = receive_message_timeout(5000);
        if (y == NULL) {
            /*
                Count the number of consecutive timeout errors
                If the number is more than three, interrupt transmission
            */
            perror("SENDER TIMEOUT ERROR\n");
            ++timeout_error;
            if (timeout_error > 3) {
                printf("[%s] stopped because of timeout on packet [%u]\n", argv[0], next_seq(last_seq));
                return -1;
            }
            /*
                If the number of consecutive timeout errors
                is less than three, resend the last packet
                with the same sequence number
            */
            packet_to_payload(pkt, &t, data_len);
            send_message(&t);
        } else {
            // Reset the number of consecutive timeout errors
            timeout_error = 0;

            // Copy payload to response packet fields
            payload_to_packet(y, response);

            /*
                If the response sequence number is the response
                for the last sent packet, resend the packet
                with the same sequence number
            */
            if (response->head.seq != next_seq(last_seq)) {
                packet_to_payload(pkt, &t, data_len);
                send_message(&t);
                continue;
            }

            // Sequence number for last sent packet
            last_seq = next_seq(response->head.seq);

            /*
                If the response received has type NAK,
                resend the last sent packet with its sequence
                number incremented
            */
            if (response->head.type == NAK) {
                pkt->head.seq = last_seq;
                packet_to_payload(pkt, &t, data_len);
                send_message(&t);
                continue;
            }
            /*
                If it gets to this point, it means the response
                has type ACK
                Verify the last sent packet type, in order to
                decide what type of packet to send next
            */

            /*
                After Send-Init and EOF packets: File-Header packet
            */
            if ((pkt->head.type == SEND_INIT) || 
                ((pkt->head.type == END_OF_FILE) 
                    && (file_headers_sent < argc - 1))) {
                file_headers_sent++;
                /*
                    Fill File-Header packet fields
                */
                fill_packet_fields(pkt, last_seq, FILE_HEADER);
                data_len = strlen(argv[file_headers_sent]);
                pkt->data = calloc(1, data_len);
                memcpy(pkt->data, argv[file_headers_sent], data_len);
                
                /*
                    Copy File-Header packet to payload
                    Send message to the receiver
                */
                packet_to_payload(pkt, &t, data_len);
                send_message(&t);

                // Open input file for reading
                input_file = open(argv[file_headers_sent], O_RDONLY);
                if (input_file == -1) {
                    perror("Error opening input file\n");
                    return -1;
                }
                eof_flag = 0;
                continue;
            }

            // Send EOF packet
            if (eof_flag) {
                /*
                    Fill EOF packet fields
                */
                fill_packet_fields(pkt, last_seq, END_OF_FILE);
                data_len = 0;
                memcpy(pkt->data, argv[file_headers_sent], data_len);
                /*
                    Copy EOF packet fields to payload
                    Send message to the receiver
                */
                packet_to_payload(pkt, &t, data_len);
                send_message(&t);

                close(input_file);
                eof_flag = 0;
                continue;
            }

            /*
                After File-Header packet or Data packet(while not EOF):
                Data packet
            */
            if ((pkt->head.type == FILE_HEADER) || (pkt->head.type == DATA)) {
                /*
                    Fill Data packet fields
                    Data field will have maximum MAXL bytes read from the file
                */
                fill_packet_fields(pkt, last_seq, DATA);
                char *buff = calloc(MAXL, sizeof(char *));
                data_len = read(input_file, buff, MAXL);
                memcpy(pkt->data, buff, data_len);
                free(buff);
                /*
                    Copy Data packet fields to payload
                    Send message to the receiver
                */
                packet_to_payload(pkt, &t, data_len);
                send_message(&t);

                /*
                    If the number of bytes read from the file is
                    less than MAXL, the file ended
                    Next packet is EOF
                */
                if (data_len < MAXL) {
                    eof_flag = 1;
                }
                continue;
            }

            /*
                If last sent packet is EOF and all files were sent:
                EOT packet
            */

            if ((pkt->head.type == END_OF_FILE) && (file_headers_sent == argc - 1)) {
                /*
                    Fill EOT packet fields
                */
                fill_packet_fields(pkt, last_seq, END_OF_TRANSMISSION);
                data_len = 0;
                /*
                    Copy EOT packet fields to payload
                    Send message to the receiver
                */
                packet_to_payload(pkt, &t, data_len);
                send_message(&t);
                continue;
            }
            /*
                If ACK for EOT is received, transmission ends
            */
            if (pkt->head.type == END_OF_TRANSMISSION) {
                transmission_ended = 1;
            }
        }
    }

    
    return 0;
}

#ifndef LIB
#define LIB

#define MAXL 250
#define TIME 5 // in second
#define EOL 0x0D
#define SOH 0x01

#define DATA_OFFSET 4
#define TAIL_SIZE 3

#define SEND_INIT 'S'
#define FILE_HEADER 'F'
#define DATA 'D'
#define END_OF_FILE 'Z'
#define END_OF_TRANSMISSION 'B'
#define ACK 'Y'
#define NAK 'N'
#define ERROR 'E'

#define SEQ_MODULO 64

typedef unsigned char uchar;

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef struct {
	uchar soh;
	uchar len;
	uchar seq;
	uchar type;
} header;

typedef struct {
	unsigned short check;
	uchar mark;
} tail;

typedef struct {
	header head;
	uchar *data; 
	tail end;
} packet;

typedef struct {
	uchar maxl;
	uchar timeout;
	uchar npad;
	uchar padc;
	uchar eol;
	//| QCTL| QBIN | CHKT | REPT| CAPA | R
	uchar not_used[6];
} send_init_data;

/*
	DEBUG
	Print Send_init data
*/
void print_data(uchar *data) {
	send_init_data *S_data = (send_init_data *)calloc(1, sizeof(send_init_data));
    memcpy(S_data, data, sizeof(send_init_data));
    printf("maxl: %u\n", S_data->maxl);
    printf("time: %u\n", S_data->timeout);
    printf("npad: %x\n", S_data->npad);
    printf("padc: %x\n", S_data->padc);
    printf("eol: %x\n", S_data->eol);
}

/*
	DEBUG
	Print packet
*/
void print_packet(packet *pkt) {
	printf("soh: %x\n", pkt->head.soh);
	printf("len: %u\n", pkt->head.len);
	printf("seq: %u\n", pkt->head.seq);
	printf("type: %c\n", pkt->head.type);
	if (pkt->head.type == SEND_INIT) {
		printf("data: \n");
		print_data(pkt->data);
	} else {
		printf("data: %s\n", pkt->data);
	}
	printf("check: %hu\n", pkt->end.check);
	printf("mark: %x\n", pkt->end.mark);
}

/*
	Fill packet fields
	Common for all packets: SOH, EOL
	SEQ and TYPE given as parameters
	LEN and DATA are completed afterwards
*/
void fill_packet_fields(packet *pkt, char seq, char type) {
    pkt->head.soh = SOH;   
    pkt->head.seq = seq % SEQ_MODULO;
    pkt->head.type = type;
    pkt->end.mark = EOL;
}

uchar next_seq(uchar seq) {
	return (seq + 1) % SEQ_MODULO;
}

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout); //timeout in milliseconds
unsigned short crc16_ccitt(const void *buf, int len);

/*
	Copies the packet fields to the message payload
*/
void packet_to_payload(packet *pkt, msg *t, uchar data_len) {
	pkt->head.len = DATA_OFFSET + data_len + TAIL_SIZE - 2;
	t->len = DATA_OFFSET + data_len + TAIL_SIZE;
    memcpy(t->payload, &pkt->head, DATA_OFFSET);
    memcpy(t->payload + DATA_OFFSET, pkt->data, data_len);
    pkt->end.check = crc16_ccitt(t->payload, t->len - TAIL_SIZE);
    memcpy(t->payload + DATA_OFFSET + data_len,
            &pkt->end, TAIL_SIZE);
}

/*
	Copies the message payload to the packet fields
*/

void payload_to_packet(msg *r, packet *pkt) {
	uchar data_len = r->len - DATA_OFFSET - TAIL_SIZE;
    pkt->data = (uchar *)calloc(data_len, sizeof(uchar*));

    memcpy(&pkt->head, r->payload, DATA_OFFSET);
    memcpy(pkt->data, r->payload + DATA_OFFSET, data_len);
    memcpy(&pkt->end, r->payload + DATA_OFFSET + data_len, TAIL_SIZE);
}

#endif


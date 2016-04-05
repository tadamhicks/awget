#ifndef AWGET_H_
#define AWGET_H_
#include <stdio.h>
#include <stdlib.h>

struct char_ip
{
	char ch_ip[17];
};

struct int_tuple
{
	int ip_addr;
	int port_num;
}__attribute__((__packed__));

struct ss_packet
{
	int stone_count;
	char url[1500];
	struct int_tuple steps[256];
}__attribute__((__packed__));

#endif

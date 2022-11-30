#ifndef __PerfectLink_H__
#define __PerfectLink_H__
#include <iostream>
#include <vector>
#include <queue>
#include <set>
#include <unordered_map>
#include <mutex>
#include <stdio.h>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define BUFFER_SIZE 4096
using namespace std;
struct Host_info {
  unsigned long id;
  std::string ip_h; // human readable ip
  in_addr_t ip; // machine ip
  unsigned short port_h; //human_readable port
  unsigned short port; // machine port 
};
std::vector<string> str_split(const string& str,const string& delim);
void show_log_list(std::string file_path); 
std::string convert_server_log(std::string message_str);
std::string convert_client_log(std::string message_str);
std::string convert_log_to_message(std::string log_str);
unsigned long int get_target_id(std::string message_str); 
int set_serv_sock(in_addr_t ip_address, unsigned short port);
int set_cli_sock(in_addr_t ip_address, unsigned short port);
std::string lis_pac(int server_socket);
bool check_deliver(std::string meg_str, std::set<std::string> * deliver_set);
bool check_ack(std::string meg_str, std::set<std::string> * ack_set);
void lis_loop(int server_socket, int client_socket, std::unordered_map<long unsigned int, Host_info> host_dict, std::set<std::string> * ack_set); 
void send_pac(int client_socket, struct sockaddr_in target_addr, std::string packet_details);
std::string build_pac(unsigned long int sender_id, int seq_nr);
#endif

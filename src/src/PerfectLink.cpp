#include<PerfectLink.h>

std::string log_str;
std::set<std::string> deliver_set;
std::mutex ack_mutex;
// In Perfect Link Project, single thread to write log, so no need to create mute
// std::mutex log_mutex;

std::vector<string> str_split(const string& str,const string& delim) { 
  vector<string> res;
  if("" == str) return  res;
  string strs = str + delim;
  size_t  pos;
  size_t  size = strs.size();
  for (unsigned long i = 0; i < size; ++i) {
    pos = strs.find(delim, i);
    if( pos < size) {
      string s = strs.substr(i, pos - i);
      res.push_back(s);
      i = pos + delim.size() - 1;
    }		
  }
  return res;	
}

void show_log_list(std::string file_path) {
  // std::cout << "Start to save log!!!!" << std::endl;
  std::ofstream outfile;
  outfile.open(file_path);
  if (log_str.length() > 0) {
    log_str.erase(log_str.end() - 1);
  }
  outfile << log_str << std::endl;
  // for(auto message : log_list) {
  //   outfile << message << std::endl;
  // }
  outfile.close();
}

std::string convert_server_log(std::string message_str) {
  std::vector<std::string> message_items = str_split(message_str, " ");
  std::string server_log = "b ";
  if (sizeof(message_items) >= 2) {
    server_log = server_log + message_items[2];
  }
  return server_log;
}

std::string convert_client_log(std::string message_str) {
  std::vector<std::string> message_items = str_split(message_str, " ");
  std::string client_log = "d ";
  if (sizeof(message_items) >= 2) {
    client_log = client_log + message_items[0] + " " + message_items[1];
  }
  return client_log;
}

std::string convert_log_to_message(std::string log_str) {
  std::vector<std::string> message_items = str_split(log_str, " ");
  std::string message;
  if (sizeof(message_items) >= 2) {
    message = message_items[1] + " " + message_items[2];
  }
  return message;
}

unsigned long int get_target_id(std::string message_str) {
  std::vector<std::string> message_items = str_split(message_str, " ");
  unsigned long int message = 0;
  if (sizeof(message_items) >= 1) {
    message = atoi(message_items[0].c_str());
  }
  return message;
}

bool check_ack(std::string meg_str, std::set<std::string> * ack_set) {
  bool check_res = false;
  ack_mutex.lock();
  if (ack_set->count(convert_log_to_message(meg_str))) {
    check_res = true;
  }
  ack_mutex.unlock();
  return check_res;
}

bool check_deliver(std::string meg_str, std::set<std::string> * deliver_set) {
  bool check_res = false;
  if (deliver_set->count(meg_str)) {
    check_res = true;
  }
  return check_res;
}

int set_serv_sock(in_addr_t ip_address, unsigned short port){
  int serv_sock = 0; 
  if ((serv_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    cerr << "Cannot create socket" << endl;
    exit(1);
  }

  struct sockaddr_in saddr;
  memset(reinterpret_cast<char*>( &saddr), 0, sizeof(saddr) );
  saddr.sin_family = AF_INET;
  saddr.sin_port = port;
  saddr.sin_addr.s_addr = ip_address;
  int size_saddr = sizeof(saddr);
  int socket_ret = bind(serv_sock, reinterpret_cast<struct sockaddr*>( &saddr), size_saddr);
  if (socket_ret < 0){
    cerr << "Error Type:" << socket_ret << " Cannot bind to serv addr" << endl;

    exit(1);
  }

  cout << "Set up serv" << ip_address << ':' \
  << port << endl;

  return serv_sock;
  
}

int set_cli_sock(in_addr_t ip_address, unsigned short port){
  int cli_sock = 0; 
  if ((cli_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    cerr << "Cannot create cli socket" << endl;
    exit(1);
  }

  struct sockaddr_in saddr;
  memset(reinterpret_cast<wchar_t*>( &saddr), 0, sizeof(saddr));
  saddr.sin_family = AF_INET;
  saddr.sin_port = port;
  saddr.sin_addr.s_addr = ip_address;
  int size_saddr = sizeof(saddr);
  if (bind(cli_sock, reinterpret_cast<struct sockaddr*>( &saddr), size_saddr) < 0){
    cerr << "Cannot bind to cli addr" << endl;
    exit(1);
  }

  cout << "Set up cli in " << ip_address << ':' \
  << port << endl;

  return cli_sock;
}

std::string lis_pac(int server_socket){
  char packet[BUFFER_SIZE];
  memset(packet, '\x90', BUFFER_SIZE);
  struct sockaddr_in addr;
  socklen_t sock_size = sizeof(struct sockaddr_in);
  struct sockaddr_in client_addr;
  ssize_t bytes = recvfrom(server_socket, packet, BUFFER_SIZE, 0, reinterpret_cast<struct sockaddr*>(&client_addr), &sock_size);
  if (bytes <= 0){
    return std::string();
  }
  if (bytes > 0){
    // Get packet size 
    int packet_size = sizeof(packet);
    // std::cout << "Receiving: packet size: " << packet_size << ", packet info: " << packet;
  }
  return packet;
}

void lis_loop(int server_socket, int client_socket, std::unordered_map<long unsigned int, Host_info> host_dict, std::set<std::string> * ack_set) {
  // std::cout << "Thread Listening..." << std::endl;
  std::string new_packet;
  std::string client_log;
  std::string message_content;
  unsigned long int target_id;
  char packet[BUFFER_SIZE];
  while (true) {
    new_packet = lis_pac(server_socket);
    // TWO CASES
    // CASE A: recieve normal packet --> add the packet to log ; send ack packet to the sender
    // CASE B: recieve ack packet --> remove the packet from sendlist
    // BAD packet(May due to newwork issues)
    if (new_packet.length() < 1) {
      return;
    }
    if (new_packet.at(0) == 'd') {
    // CASE A
      message_content = convert_log_to_message(new_packet);
      target_id = get_target_id(message_content);
      struct sockaddr_in target_addr;
      target_addr.sin_family = AF_INET;
      target_addr.sin_port = host_dict[target_id].port;
      target_addr.sin_addr.s_addr = host_dict[target_id].ip;
      message_content = "a " + message_content;
      strcpy(packet, message_content.c_str());
      // std::cout << "SENDACK " << message_content << std::endl;
      ssize_t bytes = sendto(client_socket, packet, sizeof(packet), 0, reinterpret_cast<struct sockaddr*>(&target_addr), sizeof(target_addr));
      // ATTENSION! ONLY Deliver message that did not receive to avoid duplicate deliver
      if ( ! check_deliver(new_packet, & deliver_set)) {
        deliver_set.insert(new_packet);  
        // log_mutex.lock();
        log_str += new_packet + "\n";
        // log_mutex.unlock();
	// log_list.push_back(new_packet);
      }
    } else if (new_packet.at(0) == 'a') {
    // CASE B
      // std::cout << "!RECEIVEACK " << new_packet << std::endl;
      message_content = convert_log_to_message(new_packet);
      ack_mutex.lock();
      // To check if already receive ack
      // if no, update log and update ack;
      if ( ! ack_set->count(message_content) ) {
         log_str = log_str + convert_server_log(new_packet) + "\n";
         ack_set->insert(message_content);
      }
      ack_mutex.unlock();
    } else {
    // BAD packet
      // std::cout << "bad message" << std::endl;
    }
  }
}

// Build the structure of packet as follows:
// [SENDER ID, SEQ_NR]
// END structure
std::string build_pac(unsigned long int sender_id, int seq_nr) {
  std::string packet_str = std::to_string(sender_id);
  packet_str = "d " + packet_str + " " + std::to_string(seq_nr);
  return packet_str;
}

void send_pac(int client_socket, struct sockaddr_in target_addr, std::string packet_str) {
  // std::cout << "Thread Sending... : " << packet_str << std::endl;
  char packet[BUFFER_SIZE];
  strcpy(packet, packet_str.c_str());
  ssize_t bytes = sendto(client_socket, packet, sizeof(packet), 0, reinterpret_cast<struct sockaddr*>(&target_addr), sizeof(target_addr));
  // SEND PACKET WHENEVER there is a packet_str
  // std::cout << "Successfully Send!!!" << std::endl;
  return;
}

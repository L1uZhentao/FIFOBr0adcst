#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <set>
#include <mutex>
#include <unordered_map>
#include "parser.hpp"
#include "hello.h"
#include "PerfectLink.h"
#include <signal.h>


std::string output_path;
std::queue<std::string> packet_queue;
std::set<std::string> ack_set;

static void stop(int) {
  // reset signal handlers to default
  signal(SIGTERM, SIG_DFL);
  signal(SIGINT, SIG_DFL);

  // immediately stop network packet processing
  std::cout << "Immediately stopping network packet processing.\n";

  // write/flush output file if necessary
  std::cout << "Writing output.\n";

  show_log_list(output_path);

  // exit directly from signal handler
  exit(0);
}

// Parse Config List
static std::vector<std::vector<int>> config_val_parse(std::string config_path) {
  std::ifstream ifs;
  std::vector<std::vector<int>> config_list;
  std::string string_buf;
  std::string message_cnt;
  std::string process_idx;
  ifs.open(config_path, std::ios::in);
  while (std::getline(ifs, string_buf)) { 
    std::istringstream config_iss; 
    config_iss.str(string_buf); 
    if (config_iss >> message_cnt >> process_idx) {
      std::vector<int> conf;
      conf.push_back(atoi(message_cnt.c_str()));
      conf.push_back(atoi(process_idx.c_str()));
      config_list.push_back(conf); 
    } else {
      std::cout << "error with conf file";
      std::cout << message_cnt.c_str() << process_idx.c_str();
    } 
  }
  return config_list;
}

int main(int argc, char **argv) {
  signal(SIGTERM, stop);
  signal(SIGINT, stop);

  // `true` means that a config file is required.
  // Call with `false` if no config file is necessary.
  bool requireConfig = true;

  Parser parser(argc, argv);
  parser.parse();

  vector<thread> threads_sender;
  vector<thread> threads_receiver;
  
  // GET PID by getpid()
  std::cout << "My PID: " << getpid() << "\n";
  std::cout << "From a new terminal type `kill -SIGINT " << getpid() << "` or `kill -SIGTERM "
            << getpid() << "` to stop processing packets\n\n";
  
  // GET ID by parser.id() 
  unsigned long cur_id = parser.id();
  std::cout << "My ID: " << cur_id << "\n\n";

  // Parsing host list
  std::cout << "List of resolved hosts is:\n";
  std::cout << "==========================\n";
  auto hosts = parser.hosts();
  Host_info host_info; 
  std::unordered_map<long unsigned int, Host_info> host_dict; 
  for (auto &host : hosts) {
    host_info.id = host.id;
    host_info.ip_h = host.ipReadable(); // human readable ip
    host_info.ip = host.ip; // machine ip
    host_info.port_h = host.portReadable(); //human_readable port
    host_info.port = host.port; // machine port 
    host_dict.insert(std::make_pair(host.id, host_info));
    std::cout << host_dict[host.id].id << "\n";
    std::cout << "Human-readable IP: " << host_dict[host.id].ip_h << "\n";
    std::cout << "Machine-readable IP: " << host_dict[host.id].ip << "\n";
    std::cout << "Human-readbale Port: " << host_dict[host.id].port_h << "\n";
    std::cout << "Machine-readbale Port: " << host_dict[host.id].port << "\n";
    std::cout << "\n";
  }

  output_path = parser.outputPath(); 
  std::cout << "Path to output:\n";
  std::cout << "===============\n";
  std::cout << output_path << "\n\n";

  std::cout << "Path to config:\n";
  std::cout << "===============\n";
  std::cout << parser.configPath() << "\n\n";
  
  // Parsing Config file 
  std::vector<std::vector<int>> conf_list = config_val_parse(parser.configPath());
  // To check if config file is correct
  int is_conf_correct = 0;
  // To check if it is receiver
  int is_receiver = 0;
  unsigned long int target_id;
  if (conf_list.size() == 1 ) {
    is_conf_correct = 1;
    target_id = conf_list[0][1];
    std::cout << "Target ID" << target_id << std::endl; 
    if (target_id == cur_id) {
      is_receiver = 1;
      std::cout << "This process is Receiver\n";
    } else {
      std::cout << "This process is Sender\n";
    }
  }  
  if ( is_conf_correct == 1 ) {
    int meg_num = conf_list[0][0];
    // In Perfect Link, you CANNOT send message to your self
    // Set Up Socket
    int main_socket = set_serv_sock(host_dict[cur_id].ip, host_dict[cur_id].port);
    // Attention! There is only one receiver in Perfect Link Project
    // Both Receiver(for receive message) and Sender(for receiver mac) must listen to socket
    // Socket start to listen
    std::thread *receiver_thread = new thread(lis_loop, main_socket, main_socket, host_dict, & ack_set);
    std::cout << "Receiver starts to listen" << std::endl;
    if (is_receiver != 1) {
    // In Perfect Link Project, if you are not listener, you will be Sender
    // 
    // Build up packet
    std::string packet_str;
    for (int i = 1; i <= meg_num; i++) {
      // Build Up Packet [sender_id, msg_seq]
      packet_str = build_pac(cur_id, i);
      // std:: cout << "packet_str" << std::endl;
      packet_queue.push(packet_str);
    }
    // Set Up Target sockaddr_in
    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = host_dict[target_id].port;
    target_addr.sin_addr.s_addr = host_dict[target_id].ip;
    // Send Packet As long as receive no ACK packet
    // int send_cnt = 1;
    // int wait_sec = 1;
    while (packet_queue.size() > 0 ){
      packet_str = packet_queue.front();
      packet_queue.pop();
      // If we have already receive the ack packet, we do not need to send again and just continue
      if ( check_ack(packet_str, &ack_set)) {
	// std::cout << packet_str<< " receive\n";
        // send_cnt = send_cnt + 1;
        continue;
      }
      // If we do not receive the ack packet, then we add this to the end of queue
      packet_queue.push(packet_str);
      // std::cout << "sending... \n" << packet_str << std::endl;; 
      std::thread *sender_thread = new thread(send_pac, main_socket, target_addr, packet_str);
      // send_pac( main_socket, target_addr, packet_str);
      // std::cout << "finish send\n"; 
      // sender_thread->join();
      // Now we don't take the waiting idea like TCP into consideration(Increasing wait_sec every time no ack packet receive)
      //if (send_cnt % meg_num == 0) {
      //  sleep(wait_sec);
      //  wait_sec = wait_sec + 1;
      //}
      //send_cnt = send_cnt + 1;
      sender_thread->join();
    }
    }
  } else { 
    std::cout << "For test Perfect Links . Only one Process is Allowed to receive message at this time";  }  
  // After a process finishes broadcasting,
  // it waits forever for the delivery of messages.
  std::cout << "Finished Sending" << std::endl;
  while (true) {
    std::this_thread::sleep_for(std::chrono::hours(1));
  }
  
  return 0;
}

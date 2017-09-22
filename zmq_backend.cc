#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include <node.h>
#include <zmq.h>

namespace zmq_backend {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;
using ms = std::chrono::milliseconds;
using string = std::string;
using thread = std::thread;

void* context = zmq_ctx_new();

void Push() {
  std::cout << "Sender\n";
  
  void* PushSocket = zmq_socket(context, ZMQ_PUSH);
  int rc = zmq_bind(PushSocket, "tcp://127.0.0.1:5555");
  if(rc == -1) {
      std::cout << "E: bind failed in Sender: " << zmq_strerror(errno) << "\n";
      return;
  }
  
  int i = 0;
  
  while(true) {
    string s = std::to_string(i++);
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, s.length() + 1);
    memcpy(zmq_msg_data(&msg), s.c_str(), s.length() + 1);
    
    zmq_msg_send(&msg, PushSocket, 0);
    std::this_thread::sleep_for(ms(1000));
  }
}

void Pull() {
  std::cout << "Receiver\n";
  
  void* PullSocket = zmq_socket(context, ZMQ_PULL);
  int rc = zmq_connect(PullSocket, "tcp://127.0.0.1:5555");
  if(rc == -1) {
      std::cout << "E: bind failed in Receiver: " << zmq_strerror(errno) << "\n";
      return;
  }
  
  while(true) {
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    zmq_msg_recv(&msg, PullSocket, 0);
    string s = string((char*) zmq_msg_data(&msg));
    
    std::cout << "Received " << s << " from ZMQ\n";
  }
}

void Run(const FunctionCallbackInfo<Value>& args) {
  thread push (Push);
  thread pull (Pull);
  
  push.join();
  pull.join();
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "run", Run);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)

}  // namespace zmq_backend


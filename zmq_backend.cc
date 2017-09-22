#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>

#include <nan.h>
#include <zmq.h>

namespace zmq_backend {

using namespace Nan;
using namespace v8;

using std::string;
using std::thread;

template <typename T>
class blocking_deque
{
private:
    std::mutex              d_mutex;
    std::condition_variable d_condition;
    std::deque<T>           d_queue;
public:
    void push(T const& value) {
        {
            std::unique_lock<std::mutex> lock(this->d_mutex);
            d_queue.push_front(value);
        }
        this->d_condition.notify_one();
    }
    T pop() {
        std::unique_lock<std::mutex> lock(this->d_mutex);
        this->d_condition.wait(lock, [=]{ return !this->d_queue.empty(); });
        T rc(std::move(this->d_queue.back()));
        this->d_queue.pop_back();
        return rc;
    }
};

blocking_deque<string>* in_q = new blocking_deque<string>();
blocking_deque<string>* out_q = new blocking_deque<string>();

void buffer_delete_callback(char* data, void* str) {
  delete reinterpret_cast<string *>(str);
}

class PullWorker : public AsyncWorker {
  public:
  PullWorker(Callback * callback) : AsyncWorker(callback){}
  
  void Execute() {
    s = new string(out_q->pop());
  }
  
  void HandleOKCallback () {
    Local<Object> b = 
           NewBuffer(const_cast<char*>(s->c_str()), 
           s->length(), buffer_delete_callback, 
           s).ToLocalChecked();
    Local<Value> argv[] = { b };
    callback->Call(1, argv);
  }
  private:
  string* s;
};

void* context = zmq_ctx_new();

void Push() {
  std::cout << "Sender\n";
  
  void* PushSocket = zmq_socket(context, ZMQ_PUSH);
  int rc = zmq_bind(PushSocket, "tcp://127.0.0.1:5555");
  if(rc == -1) {
      std::cout << "E: bind failed in Sender: " << zmq_strerror(errno) << "\n";
      return;
  }
  
  while(true) {
    string s = in_q->pop(); // blocks
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, s.length() + 1);
    memcpy(zmq_msg_data(&msg), s.c_str(), s.length() + 1);
    
    zmq_msg_send(&msg, PushSocket, 0);
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
    
    out_q->push(s);
  }
}

NAN_METHOD(Run) {
  char* buffer = (char*) node::Buffer::Data(info[0]->ToObject());
  unsigned int size = info[1]->Uint32Value();
  Callback* callback = new Callback(info[2].As<Function>());
  
  AsyncQueueWorker(new PullWorker(callback));
  
  string s = string(buffer, size);
  
  std::cout << "Called Run with " << s << "\n";
  
  in_q->push(s);
}

NAN_MODULE_INIT(Init) {
  Nan::Set(target, New<String>("Run").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(Run)).ToLocalChecked());
  
  new thread(Push);
  new thread(Pull);
}

NODE_MODULE(bfx_test, Init)

}  // namespace zmq_backend


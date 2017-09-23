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

struct BidAsk {
  double bid;
  double ask;
};

blocking_deque<BidAsk>* in_q = new blocking_deque<BidAsk>();
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

void ZmqPush() {
  std::cout << "Sender\n";
  
  void* PushSocket = zmq_socket(context, ZMQ_PUSH);
  int rc = zmq_bind(PushSocket, "tcp://127.0.0.1:5555");
  if(rc == -1) {
      std::cout << "E: bind failed in Sender: " << zmq_strerror(errno) << "\n";
      return;
  }
  
  while(true) {
    BidAsk ba = in_q->pop(); // blocks
    zmq_msg_t msg;
    zmq_msg_init_size(&msg, sizeof(BidAsk));
    memcpy(zmq_msg_data(&msg), &ba, sizeof(BidAsk));
    
    zmq_msg_send(&msg, PushSocket, 0);
  }
}

void ZmqPull() {
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
    BidAsk ba = *((BidAsk *) zmq_msg_data(&msg));
    
    std::cout << "Received " << ba.bid << " " << ba.ask << " from ZMQ\n";
    
    double mid = (ba.bid + ba.ask) / 2;
    
    std::cout << "Computed " << mid << " as an average price\n";
    
    out_q->push(std::to_string(mid));
  }
}

NAN_METHOD(BidAskAvg) {
  Local<Object> obj = info[0]->ToObject();
  Local<String> bidKey = Nan::New<String>("bid").ToLocalChecked();
  Local<String> askKey = Nan::New<String>("ask").ToLocalChecked();
  
  double bidVal = Nan::Get(obj, bidKey).ToLocalChecked()->NumberValue();
  double askVal = Nan::Get(obj, askKey).ToLocalChecked()->NumberValue();
  
  std::cout << "Called Run with " << bidVal << " " << askVal << "\n";
  
  Callback* callback = new Callback(info[1].As<Function>());
  
  AsyncQueueWorker(new PullWorker(callback));
  
  BidAsk ba = { bidVal, askVal };
  
  in_q->push(ba);
}

NAN_MODULE_INIT(Init) {
  Nan::Set(target, New<String>("BidAskAvg").ToLocalChecked(),
    GetFunction(New<FunctionTemplate>(BidAskAvg)).ToLocalChecked());
  
  new thread(ZmqPush);
  new thread(ZmqPull);
}

NODE_MODULE(bfx_test, Init)

}  // namespace zmq_backend


#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>

#include "pink/include/server_thread.h"
#include "pink/include/pink_conn.h"
#include "pink/include/pb_conn.h"
#include "pink/include/pink_thread.h"
#include "myproto.pb.h"

using namespace pink;

class MyConn: public PbConn {
 public:
  MyConn(int fd, std::string ip_port, ServerThread *thread, void* private_data);
  virtual ~MyConn();

  ServerThread* thread() {
    return thread_;
  }

 protected:
  virtual int DealMessage();

 private:
  ServerThread *thread_;
  int* private_data_;
  myproto::Ping ping_;
  myproto::PingRes ping_res_;
};

MyConn::MyConn(int fd, ::std::string ip_port, ServerThread *thread,
               void* worker_specific_data)
    : PbConn(fd, ip_port, thread),
      thread_(thread),
      private_data_(static_cast<int*>(worker_specific_data)) {
}

MyConn::~MyConn() {
}

int MyConn::DealMessage() {
  printf("In the myconn DealMessage branch\n");
  ping_.ParseFromArray(rbuf_ + cur_pos_ - header_len_, header_len_);
  printf ("DealMessage receive (%s) port %d \n", ping_.address().c_str(), ping_.port());

  int* data = static_cast<int*>(private_data_);
  printf("Worker's Env: %d\n", *data);

  ping_res_.Clear();
  ping_res_.set_res(11234);
  ping_res_.set_mess("heiheidfdfdf");
  res_ = &ping_res_;
  set_is_reply(true);
  return 0;
}

class MyConnFactory : public ConnFactory {
 public:
  virtual PinkConn *NewPinkConn(int connfd, const std::string &ip_port,
                                ServerThread *thread,
                                void* worker_specific_data) const {
    return new MyConn(connfd, ip_port, thread, worker_specific_data);
  }
};

class MyServerHandle : public ServerHandle {
public:
  virtual void CronHandle() const override {
    printf ("Cron operation\n");
  }
  using ServerHandle::AccessHandle;
  virtual bool AccessHandle(std::string& ip) const override {
    printf ("Access operation, receive:%s\n", ip.c_str());
    return true;
  }
  virtual int CreateWorkerSpecificData(void** data) const {
    int *num = new int(1234);
    *data = static_cast<void*>(num);
    return 0;
  }
  virtual int DeleteWorkerSpecificData(void* data) const {
    delete static_cast<int*>(data);
    return 0;
  }
};

static std::atomic<bool> running(false);

static void IntSigHandle(const int sig) {
  printf("Catch Signal %d, cleanup...\n", sig);
  running.store(false);
  printf("server Exit");
}

static void SignalSetup() {
  signal(SIGHUP, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, &IntSigHandle);
  signal(SIGQUIT, &IntSigHandle);
  signal(SIGTERM, &IntSigHandle);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf ("Usage: ./server port\n");
    exit(0);
  }

  int my_port = (argc > 1) ? atoi(argv[1]) : 8221;

  SignalSetup();

  MyConnFactory conn_factory;
  MyServerHandle handle;

  ServerThread* my_thread = NewHolyThread(my_port, &conn_factory, 1000, &handle);
  if (my_thread->StartThread() != 0) {
    printf("StartThread error happened!\n");
    exit(-1);
  }
  running.store(true);
  while (running.load()) {
    sleep(1);
  }
  my_thread->StopThread();

  delete my_thread;

  return 0;
}

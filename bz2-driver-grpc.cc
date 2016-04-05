#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>
#include <execinfo.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rpc-util.h"

#include <memory>
#include <grpc++/grpc++.h>

#include "bzlib.grpc.pb.h"
#include "bzlib.h"

int verbose = 4;

namespace bz2 {

class Bz2ServiceImpl final : public Bz2::Service {
  grpc::Status CompressStream(grpc::ServerContext* context,
                              const CompressStreamRequest* msg,
                              CompressStreamReply* rsp) {
    static const char *method = "BZ2_bzCompressStream";
    int ifd = msg->ifd();
    int ofd = msg->ofd();
    int blockSize100k = msg->blocksize100k();
    int verbosity = msg->verbosity();
    int workFactor = msg->workfactor();
    api_("=> %s(%d, %d, %d, %d, %d)", method, ifd, ofd, blockSize100k, verbosity, workFactor);
    int retval = BZ2_bzCompressStream(ifd, ofd, blockSize100k, verbosity, workFactor);
    api_("=> %s(%d, %d, %d, %d, %d) return %d", method, ifd, ofd, blockSize100k, verbosity, workFactor, retval);
    rsp->set_result(retval);
    return grpc::Status::OK;
  }
  grpc::Status DecompressStream(grpc::ServerContext* context,
                                const DecompressStreamRequest* msg,
                                DecompressStreamReply* rsp) {
    static const char *method = "BZ2_bzDecompressStream";
    int ifd = msg->ifd();
    int ofd = msg->ofd();
    int verbosity = msg->verbosity();
    int small = msg->small();
    api_("=> %s(%d, %d, %d, %d)", method, ifd, ofd, verbosity, small);
    int retval = BZ2_bzDecompressStream(ifd, ofd, verbosity, small);
    api_("=> %s(%d, %d, %d, %d) return %d", method, ifd, ofd, verbosity, small, retval);
    rsp->set_result(retval);
    return grpc::Status::OK;
  }
  grpc::Status TestStream(grpc::ServerContext* context,
                          const TestStreamRequest* msg,
                          TestStreamReply* rsp) {
    static const char *method = "BZ2_bzTestStream";
    int ifd = msg->ifd();
    int verbosity = msg->verbosity();
    int small = msg->small();
    api_("=> %s(%d, %d, %d)", method, ifd, verbosity, small);
    int retval = BZ2_bzTestStream(ifd, verbosity, small);
    api_("=> %s(%d, %d, %d) return %d", method, ifd, verbosity, small, retval);
    rsp->set_result(retval);
    return grpc::Status::OK;
  }
  grpc::Status LibVersion(grpc::ServerContext* context,
                          const LibVersionRequest* msg,
                          LibVersionReply* rsp) {
    static const char *method = "BZ2_bzlibVersion";
    api_("=> %s()", method);
    const char * retval = BZ2_bzlibVersion();
    rsp->set_version(retval);
    api_("<= %s() return '%s'", method, retval);
    return grpc::Status::OK;
  }
};

}  // namespace bz2

int main(int argc, char *argv[]) {
  signal(SIGSEGV, CrashHandler);
  signal(SIGABRT, CrashHandler);
  api_("'%s' program start", argv[0]);

  // Listen on a UNIX socket
  const char *sockfile = tempnam(nullptr, "gsck");
  std::string server_address = "unix:";
  server_address += sockfile;
  log_("listening on %s", server_address.c_str());

  // Tell the parent the address we're listening on.
  uint32_t len = server_address.size() + 1;

  const char *fd_str = getenv("API_NONCE_FD");
  assert (fd_str != NULL);
  int sock_fd = atoi(fd_str);
  int rc;
  rc = write(sock_fd, &len, sizeof(len));
  assert (rc == sizeof(len));
  rc = write(sock_fd, server_address.c_str(), len);
  assert ((uint32_t)rc == len);
  close(sock_fd);

  bz2::Bz2ServiceImpl service;
  grpc::ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  server->Wait();

  api_("'%s' program stop", argv[0]);
  return 0;
}

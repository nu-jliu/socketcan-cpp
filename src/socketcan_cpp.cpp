#include "socketcan_cpp/socketcan_cpp.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #ifdef HAVE_SOCKETCAN_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/can/raw.h>
/* CAN DLC to real data length conversion helpers */

static const unsigned char dlc2len[] = {0, 1, 2, 3, 4, 5, 6, 7,
  8, 12, 16, 20, 24, 32, 48, 64};

/* get data length from can_dlc with sanitized can_dlc */
unsigned char can_dlc2len(unsigned char can_dlc)
{
  return dlc2len[can_dlc & 0x0F];
}

static const unsigned char len2dlc[] = {0, 1, 2, 3, 4, 5, 6, 7, 8,              /* 0 - 8 */
  9, 9, 9, 9,                                                                   /* 9 - 12 */
  10, 10, 10, 10,                                                               /* 13 - 16 */
  11, 11, 11, 11,                                                               /* 17 - 20 */
  12, 12, 12, 12,                                                               /* 21 - 24 */
  13, 13, 13, 13, 13, 13, 13, 13,                                               /* 25 - 32 */
  14, 14, 14, 14, 14, 14, 14, 14,                                               /* 33 - 40 */
  14, 14, 14, 14, 14, 14, 14, 14,                                               /* 41 - 48 */
  15, 15, 15, 15, 15, 15, 15, 15,                                               /* 49 - 56 */
  15, 15, 15, 15, 15, 15, 15, 15};                                              /* 57 - 64 */

/* map the sanitized data length to an appropriate data length code */
unsigned char can_len2dlc(unsigned char len)
{
  if (len > 64) {
    return 0xF;
  }

  return len2dlc[len];
}

// #endif

namespace scpp
{
SocketCan::SocketCan()
{
}
SocketCanStatus SocketCan::open(
  const std::string & can_interface, int32_t read_timeout_ms,
  SocketMode mode)
{
  interface_ = can_interface;
  socket_mode_ = mode;
  read_timeout_ms_ = read_timeout_ms;
// #ifdef HAVE_SOCKETCAN_HEADERS

  /* open socket */
  if ((socket_ = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
    perror("socket");
    return SocketCanStatus::STATUS_SOCKET_CREATE_ERROR;
  }
  int mtu, enable_canfd = 1;
  struct sockaddr_can addr;
  struct ifreq ifr;

  strncpy(ifr.ifr_name, can_interface.c_str(), IFNAMSIZ - 1);
  ifr.ifr_name[IFNAMSIZ - 1] = '\0';
  ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);
  if (!ifr.ifr_ifindex) {
    perror("if_nametoindex");
    return SocketCanStatus::STATUS_INTERFACE_NAME_TO_IDX_ERROR;
  }

  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;

  if (mode == SocketMode::MODE_CANFD_MTU) {
    /* check if the frame fits into the CAN netdevice */
    if (ioctl(socket_, SIOCGIFMTU, &ifr) < 0) {
      perror("SIOCGIFMTU");
      return SocketCanStatus::STATUS_MTU_ERROR;
    }
    mtu = ifr.ifr_mtu;

    if (mtu != CANFD_MTU) {
      return SocketCanStatus::STATUS_CANFD_NOT_SUPPORTED;
    }

    /* interface is ok - try to switch the socket into CAN FD mode */
    if (setsockopt(
        socket_,
        SOL_CAN_RAW,
        CAN_RAW_FD_FRAMES,
        &enable_canfd,
        sizeof(enable_canfd)
    ))
    {
      return SocketCanStatus::STATUS_ENABLE_FD_SUPPORT_ERROR;
    }

  }

//   const int timestamping_flags = (SOF_TIMESTAMPING_SOFTWARE | \
//     SOF_TIMESTAMPING_RX_SOFTWARE | \
//     SOF_TIMESTAMPING_RAW_HARDWARE);

//   if (setsockopt(
//       socket_, SOL_SOCKET, SO_TIMESTAMPING,
//       &timestamping_flags, sizeof(timestamping_flags)) < 0)
//   {
//     perror("setsockopt SO_TIMESTAMPING is not supported by your Linux kernel");
//   }

//   /* disable default receive filter on this RAW socket */
//   /* This is obsolete as we do not read from the socket at all, but for */
//   /* this reason we can remove the receive list in the Kernel to save a */
//   /* little (really a very little!) CPU usage.                          */
//   setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

// LINUX
  struct timeval tv;
  tv.tv_sec = 0;        /* 30 Secs Timeout */
  tv.tv_usec = read_timeout_ms_ * 1000;        // Not init'ing this can cause strange errors
  setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(struct timeval));

  if (bind(socket_, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return SocketCanStatus::STATUS_BIND_ERROR;
  }
  return SocketCanStatus::STATUS_OK;
}
SocketCanStatus SocketCan::write(const CanFrame & msg)
{
  struct canfd_frame frame;
  memset(&frame, 0, sizeof(frame));       /* init CAN FD frame, e.g. LEN = 0 */
  //convert CanFrame to canfd_frame
  frame.can_id = msg.id;
  frame.len = msg.len;
  frame.flags = msg.flags;
  memcpy(frame.data, msg.data, msg.len);

  if (socket_mode_ == SocketMode::MODE_CANFD_MTU) {
    /* ensure discrete CAN FD length values 0..8, 12, 16, 20, 24, 32, 64 */
    frame.len = can_dlc2len(can_len2dlc(frame.len));
  }
  /* send frame */
  if (::write(socket_, &frame, int(socket_mode_)) != int(socket_mode_)) {
    perror("write");
    return SocketCanStatus::STATUS_WRITE_ERROR;
  }
  return SocketCanStatus::STATUS_OK;
}
SocketCanStatus SocketCan::read(CanFrame & msg)
{
  struct canfd_frame frame;

  // Read in a CAN frame
  auto num_bytes = ::read(socket_, &frame, CANFD_MTU);
  if (num_bytes != CAN_MTU && num_bytes != CANFD_MTU) {
    //perror("Can read error");
    return SocketCanStatus::STATUS_READ_ERROR;
  }

  msg.id = frame.can_id;
  msg.len = frame.len;
  msg.flags = frame.flags;
  memcpy(msg.data, frame.data, frame.len);

  return SocketCanStatus::STATUS_OK;
}
SocketCanStatus SocketCan::close()
{
  ::close(socket_);
  return SocketCanStatus::STATUS_OK;
}
const std::string & SocketCan::interfaceName() const
{
  return interface_;
}
SocketCan::~SocketCan()
{
  close();
}
}

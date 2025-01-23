#ifndef SOCKETCAN_CPP__SOCKETCAN_CPP_HPP___
#define SOCKETCAN_CPP__SOCKETCAN_CPP_HPP___

#include <string>
#include <linux/can.h>

namespace scpp
{
/// \brief Socket mode
enum class SocketMode
{
  MODE_CAN_MTU = CAN_MTU,
  MODE_CANFD_MTU = CANFD_MTU
};

/// \brief CAN frame
struct CanFrame
{
  uint32_t id = 0;
  uint8_t len = 0;
  uint8_t flags = 0;
  uint8_t data[64];

};

/// \brief Status of the socketcan interface
enum class SocketCanStatus
{
  STATUS_OK = 1 << 0,
  STATUS_SOCKET_CREATE_ERROR = 1 << 2,
  STATUS_INTERFACE_NAME_TO_IDX_ERROR = 1 << 3,
  STATUS_MTU_ERROR = 1 << 4,       /// maximum transfer unit
  STATUS_CANFD_NOT_SUPPORTED = 1 << 5,       /// Flexible data-rate is not supported on this interface
  STATUS_ENABLE_FD_SUPPORT_ERROR = 1 << 6,       /// Error on enabling fexible-data-rate support
  STATUS_WRITE_ERROR = 1 << 7,
  STATUS_READ_ERROR = 1 << 8,
  STATUS_BIND_ERROR = 1 << 9,
};

class SocketCan
{
public:
  /// Constructor
  SocketCan();

  /// Destructor
  ~SocketCan();

  /// \brief Open a socketcan interface
  /// \param can_interface Name of the can interface
  /// \param read_timeout_ms Read timeout in milliseconds
  /// \param mode Socket mode, can be either SocketMode::MODE_CAN_MTU or SocketMode::MODE_CANFD_MTU
  /// \return SocketCanStatus
  SocketCanStatus open(
    const std::string & can_interface,
    int32_t read_timeout_ms = 3,
    SocketMode mode = SocketMode::MODE_CAN_MTU
  );

  /// \brief Write a CAN frame
  /// \param msg CAN frame to write
  SocketCanStatus write(const CanFrame & msg);

  /// \brief Read a CAN frame
  /// \param msg CAN frame to read
  SocketCanStatus read(CanFrame & msg);

  /// \brief Close the socket
  SocketCanStatus close();

  /// \brief Get the interface name
  /// \return Interface name
  const std::string & interfaceName() const;

  SocketCan(const SocketCan &) = delete;
  SocketCan & operator=(const SocketCan &) = delete;

private:
  int socket_ = -1;
  int32_t read_timeout_ms_ = 3;
  std::string interface_;
  SocketMode socket_mode_;
};
}


#endif /// SOCKETCAN_CPP__SOCKETCAN_CPP_HPP___

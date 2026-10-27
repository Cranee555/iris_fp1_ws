#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/int32_multi_array.hpp"

using namespace std::chrono_literals;

class UdpSender : public rclcpp::Node
{
public:
  UdpSender() : Node("udp_sender")
  {
    std::string host = declare_parameter<std::string>("host", "127.0.0.1");
    int port = declare_parameter<int>("port", 12151);

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) {
      RCLCPP_FATAL(get_logger(), "Gagal membuat socket");
      throw std::runtime_error("socket gagal");
    }

    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr_.sin_addr) != 1) {
      RCLCPP_FATAL(get_logger(), "Host tidak valid: %s", host.c_str());
      throw std::runtime_error("host tidak valid");
    }

    sub_ = create_subscription<std_msgs::msg::Int32MultiArray>(
      "/car/command", 10,
      std::bind(&UdpSender::commandCallback, this, std::placeholders::_1));

    timer_ = create_wall_timer(20ms, std::bind(&UdpSender::sendPacket, this));

    last_cmd_time_ = now();
    RCLCPP_INFO(get_logger(), "udp_sender siap, kirim ke %s:%d", host.c_str(), port);
  }

  ~UdpSender() override
  {
    if (sock_ >= 0) {
      close(sock_);
    }
  }

private:
  void commandCallback(const std_msgs::msg::Int32MultiArray::SharedPtr msg)
  {
    if (msg->data.size() < 2) {
      RCLCPP_WARN(get_logger(), "Perintah harus berisi 2 angka: [pwm, steering]");
      return;
    }
    pwm_ = std::clamp<int32_t>(msg->data[0], -255, 255);
    steer_ = std::clamp<int32_t>(msg->data[1], -90, 90);
    last_cmd_time_ = now();
  }

  void sendPacket()
  {
    int32_t pwm = pwm_;
    if ((now() - last_cmd_time_).seconds() > 0.5) {
      pwm = 0;
    }

    uint8_t buf[12];
    const char header[4] = {'I', 'R', 'I', 'S'};
    uint32_t pwm_le = htole32(static_cast<uint32_t>(pwm));
    uint32_t steer_le = htole32(static_cast<uint32_t>(steer_));
    std::memcpy(buf, header, 4);
    std::memcpy(buf + 4, &pwm_le, 4);
    std::memcpy(buf + 8, &steer_le, 4);

    ssize_t n = sendto(sock_, buf, sizeof(buf), 0,
                       reinterpret_cast<sockaddr *>(&addr_), sizeof(addr_));
    if (n < 0) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "sendto gagal");
    }
  }

  int sock_{-1};
  sockaddr_in addr_{};
  int32_t pwm_{0};
  int32_t steer_{0};
  rclcpp::Time last_cmd_time_;
  rclcpp::Subscription<std_msgs::msg::Int32MultiArray>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<UdpSender>());
  rclcpp::shutdown();
  return 0;
}
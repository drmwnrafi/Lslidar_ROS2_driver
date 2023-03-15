#include "lslidar_driver/lslidar_driver.h"
#include "std_msgs/msg/string.h"

using namespace lslidar_ch_driver;
volatile sig_atomic_t flag = 1;

static void my_handler(int sig) {
    flag = 0;
}

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    signal(SIGINT, my_handler);
    auto node = std::make_shared<lslidar_ch_driver::LslidarChDriver>();
    if (!node->initialize()) {
        RCLCPP_ERROR(node->get_logger(), "cannot initialize lslidar driver.");
        return 0;
    }
    while (rclcpp::ok()) {
        node->polling();

    }
    rclcpp::shutdown();
    return 0;
}

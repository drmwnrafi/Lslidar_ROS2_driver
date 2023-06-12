/******************************************************************************
 * This file is part of lslidar_cx driver.
 *
 * Copyright 2022 LeiShen Intelligent Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "lslidar_driver/lslidar_driver.h"
#include "sensor_msgs/point_cloud2_iterator.hpp"

#include <memory>

namespace lslidar_ch_driver {
    LslidarChDriver::LslidarChDriver() : LslidarChDriver(rclcpp::NodeOptions()) {
        return;
    }

    LslidarChDriver::LslidarChDriver(
            const rclcpp::NodeOptions &options) :
            rclcpp::Node("lslidar_node", options),
            socket_id(-1),
            sweep_data(new lslidar_msgs::msg::LslidarScan()),
            sweep_data_bak(new lslidar_msgs::msg::LslidarScan()) {
        last_packet_timestamp = 0.0;
        is_update_difop = false;
        packetType = false;
        first_frame_flag = 0;
        return;
    }

    LslidarChDriver::~LslidarChDriver() {

        if (nullptr != difop_thread_) {
            difop_thread_->join();
        }
        return;
    }

    bool LslidarChDriver::loadParameters() {
        this->declare_parameter<std::string>("pcap", "");
        this->declare_parameter<std::string>("device_ip", "192.168.1.200");
        this->declare_parameter<std::string>("lidar_type", "ch64");
        this->declare_parameter<int>("msop_port", 2368);
        this->declare_parameter<int>("difop_port", 2369);
//        this->declare_parameter<int>("field_angle_type", 1);
        this->declare_parameter<bool>("pcl_type", false);
        this->declare_parameter<bool>("add_multicast", false);
        this->declare_parameter<std::string>("group_ip", "234.2.3.2");
        this->declare_parameter<std::string>("frame_id", "laser_link");
        this->declare_parameter<double>("min_range", 0.3);
        this->declare_parameter<double>("max_range", 150.0);
        this->declare_parameter<double>("packet_rate", 3388.0);
        this->declare_parameter<double>("angle_disable_min", 0.0);
        this->declare_parameter<double>("angle_disable_max", 0.0);
        this->declare_parameter<std::string>("topic_name", "lslidar_point_cloud");
        this->declare_parameter<double>("horizontal_angle_resolution", 0.09);
        this->declare_parameter<bool>("use_time_service", false);
        this->declare_parameter<int>("channel_num", 15);
        this->declare_parameter<bool>("publish_scan", false);
        this->declare_parameter<int>("echo_num", 0);

        this->get_parameter("pcap", dump_file);
        this->get_parameter("device_ip", lidar_ip_string);
        this->get_parameter("lidar_type", lidar_type);
        this->get_parameter("msop_port", msop_udp_port);
        this->get_parameter("difop_port", difop_udp_port);
//        this->get_parameter("field_angle_type", field_angle_type);
        this->get_parameter("pcl_type", pcl_type);
        this->get_parameter("add_multicast", add_multicast);
        this->get_parameter("group_ip", group_ip_string);
        this->get_parameter("frame_id", frame_id);
        this->get_parameter("min_range", min_range);
        this->get_parameter("max_range", max_range);
        this->get_parameter("packet_rate", packet_rate);
        this->get_parameter("angle_disable_min", angle_disable_min);
        this->get_parameter("angle_disable_max", angle_disable_max);
        this->get_parameter("topic_name", pointcloud_topic);
        this->get_parameter("horizontal_angle_resolution", horizontal_angle_resolution);
        this->get_parameter("use_time_service", use_time_service);
        this->get_parameter("channel_num", channel_num);
        this->get_parameter("publish_scan", publish_laserscan);
        this->get_parameter("echo_num", echo_num);

        RCLCPP_INFO(this->get_logger(), "dump_file: %s", dump_file.c_str());
        RCLCPP_INFO(this->get_logger(), "device_ip: %s", lidar_ip_string.c_str());
        RCLCPP_INFO(this->get_logger(), "lidar_type: %s", lidar_type.c_str());
        RCLCPP_INFO(this->get_logger(), "msop_port: %d", msop_udp_port);
        RCLCPP_INFO(this->get_logger(), "difop_udp_port: %d", difop_udp_port);
        //       RCLCPP_INFO(this->get_logger(), "field_angle_type: %d", field_angle_type);
        RCLCPP_INFO(this->get_logger(), "pcl_type: %d", pcl_type);
        RCLCPP_INFO(this->get_logger(), "add_multicast: %d", add_multicast);
        RCLCPP_INFO(this->get_logger(), "group_ip_string: %s", group_ip_string.c_str());
        RCLCPP_INFO(this->get_logger(), "frame_id: %s", frame_id.c_str());
        RCLCPP_INFO(this->get_logger(), "min_range: %f", min_range);
        RCLCPP_INFO(this->get_logger(), "max_range: %f", max_range);
        RCLCPP_INFO(this->get_logger(), "packet_rate: %f", packet_rate);
        RCLCPP_INFO(this->get_logger(), "angle_disable_min: %f", angle_disable_min);
        RCLCPP_INFO(this->get_logger(), "angle_disable_max: %f", angle_disable_max);
        RCLCPP_INFO(this->get_logger(), "topic_name: %s", pointcloud_topic.c_str());
        RCLCPP_INFO(this->get_logger(), "horizontal_angle_resolution: %f", horizontal_angle_resolution);
        RCLCPP_INFO(this->get_logger(), "use_time_service: %d", use_time_service);
        RCLCPP_INFO(this->get_logger(), "channel_num: %d", channel_num);
        RCLCPP_INFO(this->get_logger(), "publish_scan: %d", publish_laserscan);
        RCLCPP_INFO(this->get_logger(), "echo_num: %d", echo_num);

        inet_aton(lidar_ip_string.c_str(), &lidar_ip);
        //angle_disable_min = angle_disable_min * DEG_TO_RAD;
        //angle_disable_max = angle_disable_max * DEG_TO_RAD;

        if (add_multicast)
            RCLCPP_WARN(this->get_logger(), "Opening UDP socket: group_address  %s", group_ip_string.c_str());

        if (publish_laserscan) {
            if (channel_num < 0) {
                channel_num = 0;
                RCLCPP_WARN(this->get_logger(), "channel_num outside of the index, select channel 0 instead!");
            } else if (channel_num > 119) {
                channel_num = 119;
                RCLCPP_WARN(this->get_logger(), "channel_num outside of the index, select channel 119 instead!");
            }
            RCLCPP_INFO(this->get_logger(), "select channel num: %d", channel_num);
        }

/*        switch (frequency) {
            case 5:
                horizontal_angle_resolution = DEG2RAD(0.1);
                break;
            case 20:
                horizontal_angle_resolution = DEG2RAD(0.4);
                break;
            default:
                horizontal_angle_resolution = DEG2RAD(0.2);
        }*/


        return true;
    }

    bool LslidarChDriver::createRosIO() {

        // ROS diagnostics

        pointcloud_pub = this->create_publisher<sensor_msgs::msg::PointCloud2>(pointcloud_topic, 10);
        laserscan_pub = this->create_publisher<sensor_msgs::msg::LaserScan>("scan", 10);

        if (dump_file != "") {
            msop_input_.reset(new lslidar_ch_driver::InputPCAP(this, msop_udp_port, 1206, packet_rate, dump_file));
            difop_input_.reset(new lslidar_ch_driver::InputPCAP(this, difop_udp_port, 1206, 1, dump_file));
        } else {
            msop_input_.reset(new lslidar_ch_driver::InputSocket(this, msop_udp_port, 1206));
            difop_input_.reset(new lslidar_ch_driver::InputSocket(this, difop_udp_port, 1206));
        }

        difop_thread_ = std::make_shared<std::thread>([this]() { difopPoll(); });
        return true;
    }

    bool LslidarChDriver::initialize() {

        this->initTimeStamp();

        if (!loadParameters()) {
            RCLCPP_ERROR(this->get_logger(), "Cannot load all required ROS parameters...");
            return false;
        }

        if (!createRosIO()) {
            RCLCPP_ERROR(this->get_logger(), "Cannot create all ROS IO...");
            return false;
        }
        for (int j = 0; j < 36000; ++j) {
            sin_azimuth_table[j] = sin(j * 0.01 * 0.017453293);
            cos_azimuth_table[j] = cos(j * 0.01 * 0.017453293);
        }
        for (int i = 0; i < 120; i = i + 6) {
            theta_t[i] = sin((-13 + i * 0.167) * DEG_TO_RAD);
            theta_t[i + 1] = sin((-13 + i * 0.167) * DEG_TO_RAD);
            theta_t[i + 2] = sin((-13 + i * 0.167) * DEG_TO_RAD);
            theta_t[i + 3] = sin((-13 + i * 0.167) * DEG_TO_RAD);
            theta_t[i + 4] = sin((-13 + i * 0.167) * DEG_TO_RAD);
            theta_t[i + 5] = sin((-13 + i * 0.167) * DEG_TO_RAD);

            theta_q[i] = 0;
            theta_q[i + 1] = sin(0.12 * DEG_TO_RAD);
            theta_q[i + 2] = sin(0.24 * DEG_TO_RAD);
            theta_q[i + 3] = sin(0.36 * DEG_TO_RAD);
            theta_q[i + 4] = sin(0.48 * DEG_TO_RAD);
            theta_q[i + 5] = sin(0.60 * DEG_TO_RAD);
        }

        return true;
    }


    void LslidarChDriver::publishLaserScan() {
        if (!is_update_difop || first_frame_flag == 1) {
            return;
        }
        std::unique_lock<std::mutex> lock(pointcloud_lock);
        sensor_msgs::msg::LaserScan::UniquePtr scan_msg(new sensor_msgs::msg::LaserScan());
        scan_msg->header.frame_id = frame_id;

        //scan_msg->header.stamp = pcl_conversions::fromPCL(point_cloud_timestamp);
        scan_msg->header.stamp = rclcpp::Time(point_cloud_timestamp);

        scan_msg->angle_min = DEG2RAD(0);
        scan_msg->angle_max = DEG2RAD(180);
        scan_msg->range_min = min_range;
        scan_msg->range_max = max_range;
        scan_msg->angle_increment = horizontal_angle_resolution * DEG_TO_RAD;
        uint point_size = ceil((scan_msg->angle_max - scan_msg->angle_min) / scan_msg->angle_increment);
        scan_msg->ranges.assign(point_size, std::numeric_limits<float>::quiet_NaN());
        scan_msg->intensities.assign(point_size, std::numeric_limits<float>::quiet_NaN());
        if (sweep_data_bak->points.size() > 0) {
            for (size_t j = 0; j < sweep_data_bak->points.size(); ++j) {
                if (channel_num == sweep_data_bak->points[j].line) {
                    float horizontal_angle = sweep_data_bak->points[j].azimuth * DEG_TO_RAD;
                    uint point_index = (int) ((horizontal_angle - scan_msg->angle_min) / scan_msg->angle_increment);
                    point_index = (point_index < point_size) ? point_index : (point_index % point_size);
                    scan_msg->ranges[point_index] = sweep_data_bak->points[j].distance;
                    scan_msg->intensities[point_index] = sweep_data_bak->points[j].intensity;
                }
            }
            laserscan_pub->publish(std::move(scan_msg));
        }


    }

    void LslidarChDriver::publishPointCloud() {
        if (!is_update_difop || first_frame_flag == 1) {
            return;
        }
        std::unique_lock<std::mutex> lock(pointcloud_lock);
        if (pcl_type) {
            pcl::PointCloud<pcl::PointXYZI>::Ptr point_cloud(new pcl::PointCloud<pcl::PointXYZI>);
            point_cloud->header.frame_id = frame_id;
            point_cloud->height = 1;
            point_cloud->header.stamp = static_cast<uint64_t>(point_cloud_timestamp * 1e6);
            pcl::PointXYZI point;
            if (sweep_data_bak->points.size() > 0) {

                //   point_cloud->header.stamp = point_cloud_timestamp;
                for (size_t j = 0; j < sweep_data_bak->points.size(); ++j) {
                    if ((sweep_data_bak->points[j].azimuth > angle_disable_min) and
                        (sweep_data_bak->points[j].azimuth < angle_disable_max)) {
                        continue;
                    }
                    point.x = sweep_data_bak->points[j].x;
                    point.y = sweep_data_bak->points[j].y;
                    point.z = sweep_data_bak->points[j].z;
                    point.intensity = sweep_data_bak->points[j].intensity;
                    point_cloud->points.push_back(point);
                    ++point_cloud->width;
                }
                sensor_msgs::msg::PointCloud2 pc_msg;
                pcl::toROSMsg(*point_cloud, pc_msg);
                //pc_msg.header.stamp = rclcpp::Time(point_cloud_timestamp);
                pointcloud_pub->publish(pc_msg);
            }
        } else {
            pcl::PointCloud<VPoint>::Ptr point_cloud(new pcl::PointCloud<VPoint>);
            point_cloud->header.frame_id = frame_id;
            point_cloud->height = 1;
            point_cloud->header.stamp = static_cast<uint64_t>(point_cloud_timestamp * 1e6);

            //pcl::PointXYZI point;
            VPoint point;
            if (sweep_data_bak->points.size() > 0) {
                // point_cloud->header.stamp = point_cloud_timestamp;
                for (size_t j = 0; j < sweep_data_bak->points.size(); ++j) {
                    if ((sweep_data_bak->points[j].azimuth > angle_disable_min) and
                        (sweep_data_bak->points[j].azimuth < angle_disable_max)) {
                        continue;
                    }
                    point.time = sweep_data_bak->points[j].time;
                    point.x = sweep_data_bak->points[j].x;
                    point.y = sweep_data_bak->points[j].y;
                    point.z = sweep_data_bak->points[j].z;
                    point.intensity = sweep_data_bak->points[j].intensity;
                    point.ring = sweep_data_bak->points[j].line;
                    point_cloud->points.push_back(point);
                    ++point_cloud->width;
                }
                sensor_msgs::msg::PointCloud2 pc_msg;
                pcl::toROSMsg(*point_cloud, pc_msg);
                //pc_msg.header.stamp = rclcpp::Time(point_cloud_timestamp);
                pointcloud_pub->publish(pc_msg);
            }
        }
        return;
    }


    int LslidarChDriver::convertCoordinate(struct Firing lidardata) {
        if (!isPointInRange(lidardata.distance) || lidardata.azimuth > 180.0) {
            return -1;
        }
        double x = 0.0, y = 0.0, z = 0.0;
        double z_sin_theat = 0.0;
        double Z_cos_theat = 0.0;
        line_num = lidardata.vertical_line;
        int temp = (line_num / 6) % 2;
        if (temp == 0) {
            z_sin_theat =
                    theta_t[line_num] + 2 * cos((lidardata.azimuth - 0.767) * DEG_TO_RAD * 0.5) * theta_q[line_num];
        } else {
            z_sin_theat =
                    theta_t[line_num] + 2 * cos((lidardata.azimuth + 0.767) * DEG_TO_RAD * 0.5) * theta_q[line_num];
        }
        Z_cos_theat = sqrt(1 - z_sin_theat * z_sin_theat);

        x = lidardata.distance * Z_cos_theat * cos_azimuth_table[static_cast<int>(lidardata.azimuth * 100)];
        y = lidardata.distance * Z_cos_theat * sin_azimuth_table[static_cast<int>(lidardata.azimuth * 100)];
        z = lidardata.distance * z_sin_theat;


        sweep_data->points.push_back(lslidar_msgs::msg::LslidarPoint());
        lslidar_msgs::msg::LslidarPoint &new_point =        // new_point 为push_back最后一个的引用
                sweep_data->points[sweep_data->points.size() - 1];

        new_point.x = x;
        new_point.y = y;
        new_point.z = z;
        new_point.azimuth = lidardata.azimuth;
        new_point.distance = lidardata.distance;
        new_point.intensity = lidardata.intensity;
        new_point.line = lidardata.vertical_line;
        new_point.time = lidardata.time;
        return 0;
    }


    bool LslidarChDriver::polling() {
        // Allocate a new shared pointer for zero-copy sharing with other nodelets.
        lslidar_msgs::msg::LslidarPacket::UniquePtr packet(new lslidar_msgs::msg::LslidarPacket());
        int rc = -1;
        // Since the lslidar delivers data at a very high rate, keep
        // reading and publishing scans as fast as possible.
        while (true) {
            // keep reading until full packet received
            rc = msop_input_->getPacket(packet);

            if (rc == 0) break;       // got a full packet?
            if (rc < 0) return false; // end of file reached?
        }

        if (use_time_service) {
            //struct tm cur_time{};
            memset(&cur_time, 0, sizeof(cur_time));
            if (packet->data[1205] == 0x01) {
                cur_time.tm_sec = packet->data[1199];
                cur_time.tm_min = packet->data[1198];
                cur_time.tm_hour = packet->data[1197];
                cur_time.tm_mday = this->packetTimeStamp[7];
                cur_time.tm_mon = this->packetTimeStamp[8] - 1;
                cur_time.tm_year = this->packetTimeStamp[9] + 2000 - 1900;
            } else if (packet->data[1205] == 0x02) {
                cur_time.tm_sec = packet->data[1199];
                cur_time.tm_min = this->packetTimeStamp[5];
                cur_time.tm_hour = this->packetTimeStamp[6];
                cur_time.tm_mday = this->packetTimeStamp[7];
                cur_time.tm_mon = this->packetTimeStamp[8] - 1;
                cur_time.tm_year = this->packetTimeStamp[9] + 2000 - 1900;

            }

            packet_timestamp_s = timegm(&cur_time);
            packet_timestamp_ns = (packet->data[1200] +
                                   packet->data[1201] * pow(2, 8) +
                                   packet->data[1202] * pow(2, 16) +
                                   packet->data[1203] * pow(2, 24)) * 1e3; //ns

            packet_timestamp = packet_timestamp_s + packet_timestamp_ns * 1e-9;
            // ch128x1_packet->timestamp = packet_timestamp;
            // it is already the msop msg

        } else {
            packet_timestamp = this->get_clock()->now().seconds();
        }

        struct Firing lidardata{};

        packet_interval_time = packet_timestamp - last_packet_timestamp;
        last_packet_timestamp = packet_timestamp;



        // Decode the packet
        if (packet->data[1205] == 0x01) {
            for (size_t point_idx = 0; point_idx < 1197; point_idx += 7) {
                if ((packet->data[point_idx] == 0xff) && (packet->data[point_idx + 1] == 0xaa) &&
                    (packet->data[point_idx + 2] == 0xbb) && (packet->data[point_idx + 3] == 0x00)) {
                    packetType = true;
                    if (first_frame_flag == 0 || first_frame_flag == 1) {
                        first_frame_flag++;
                    }

                    point_cloud_timestamp =
                            packet_timestamp - packet_interval_time + point_idx * packet_interval_time / 1197.0;
                   // RCLCPP_INFO(get_logger(), "pct=%.6f", point_cloud_timestamp);
                }
                // Compute the time of the point

                point_time = packet_timestamp - packet_interval_time + point_idx * packet_interval_time / 1197.0 -
                             point_cloud_timestamp;


                if (packet->data[point_idx] < 120) {
                    memset(&lidardata, 0, sizeof(lidardata));
                    lidardata.vertical_line = packet->data[point_idx];
                    lidardata.azimuth = (packet->data[point_idx + 1] * 256 + packet->data[point_idx + 2]) * 0.01f;
                    lidardata.distance =
                            (packet->data[point_idx + 3] * 65536 +
                             packet->data[point_idx + 4] * 256 +
                             packet->data[point_idx + 5]) * DISTANCE_RESOLUTION;
                    lidardata.intensity = packet->data[point_idx + 6];
                    lidardata.time = point_time;
                    convertCoordinate(lidardata);
                }
                if (packetType) {
                    {
                        std::unique_lock<std::mutex> lock(pointcloud_lock);
                        sweep_data_bak = std::move(sweep_data);
                    }
                    std::thread ppc_thread(&LslidarChDriver::publishPointCloud, this);
                    ppc_thread.detach();
                    //publishPointCloud();

                    if (publish_laserscan) {
                        std::thread pls_thread(&LslidarChDriver::publishLaserScan, this);
                        pls_thread.detach();
                        //publishLaserScan();
                    }
                    packetType = false;
                    sweep_data = std::make_unique<lslidar_msgs::msg::LslidarScan>();
                }
            }
        } else if (packet->data[1205] == 0x02) {
            RCLCPP_INFO_ONCE(this->get_logger(),
                             "lidar is double echo model,and the selected echo is: %d [0 mean double echo; 1 mean first echo; 2 mean second echo]",
                             echo_num);
            for (size_t point_idx = 0; point_idx < 1199; point_idx += 11) {
                if ((packet->data[point_idx] == 0xff) && (packet->data[point_idx + 1] == 0xaa) &&
                    (packet->data[point_idx + 2] == 0xbb) && (packet->data[point_idx + 3] == 0x00)) {
                    if (first_frame_flag == 0 || first_frame_flag == 1) {
                        first_frame_flag++;
                    }
                    packetType = true;
                    point_cloud_timestamp =
                            packet_timestamp - packet_interval_time + point_idx * packet_interval_time / 1199.0;
                }
                // Compute the time of the point

                point_time = packet_timestamp - packet_interval_time +
                             point_idx * packet_interval_time / 1199.0 - point_cloud_timestamp;

                if (packet->data[point_idx] < 120) {
                    if (echo_num == 0) {
                        memset(&lidardata, 0, sizeof(lidardata));
                        lidardata.vertical_line = packet->data[point_idx];
                        lidardata.azimuth =
                                (packet->data[point_idx + 1] * 256 + packet->data[point_idx + 2]) * 0.01f;
                        lidardata.distance =
                                (packet->data[point_idx + 3] * 65536 +
                                 packet->data[point_idx + 4] * 256 +
                                 packet->data[point_idx + 5]) * DISTANCE_RESOLUTION;
                        lidardata.intensity = packet->data[point_idx + 6];
                        lidardata.time = point_time;
                        convertCoordinate(lidardata);

                        memset(&lidardata, 0, sizeof(lidardata));
                        lidardata.vertical_line = packet->data[point_idx];
                        lidardata.azimuth =
                                (packet->data[point_idx + 1] * 256 + packet->data[point_idx + 2]) * 0.01f;
                        lidardata.distance =
                                (packet->data[point_idx + 7] * 65536 +
                                 packet->data[point_idx + 8] * 256 +
                                 packet->data[point_idx + 9]) * DISTANCE_RESOLUTION;
                        lidardata.intensity = packet->data[point_idx + 10];
                        lidardata.time = point_time;
                        convertCoordinate(lidardata);
                    } else if (echo_num == 1) {
                        memset(&lidardata, 0, sizeof(lidardata));
                        lidardata.vertical_line = packet->data[point_idx];
                        lidardata.azimuth =
                                (packet->data[point_idx + 1] * 256 + packet->data[point_idx + 2]) * 0.01f;
                        lidardata.distance =
                                (packet->data[point_idx + 3] * 65536 +
                                 packet->data[point_idx + 4] * 256 +
                                 packet->data[point_idx + 5]) * DISTANCE_RESOLUTION;
                        lidardata.intensity = packet->data[point_idx + 6];
                        lidardata.time = point_time;
                        convertCoordinate(lidardata);
                    } else if (echo_num == 2) {
                        memset(&lidardata, 0, sizeof(lidardata));
                        lidardata.vertical_line = packet->data[point_idx];
                        lidardata.azimuth =
                                (packet->data[point_idx + 1] * 256 + packet->data[point_idx + 2]) * 0.01f;
                        lidardata.distance =
                                (packet->data[point_idx + 7] * 65536 +
                                 packet->data[point_idx + 8] * 256 +
                                 packet->data[point_idx + 9]) * DISTANCE_RESOLUTION;
                        lidardata.intensity = packet->data[point_idx + 10];
                        lidardata.time = point_time;
                        convertCoordinate(lidardata);
                    }

                }
                if (packetType) {
                    //("---------------onesweep--------------------------\n");
                    {
                        std::unique_lock<std::mutex> lock(pointcloud_lock);
                        sweep_data_bak = std::move(sweep_data);
                    }
                    std::thread ppc_thread(&LslidarChDriver::publishPointCloud, this);
                    ppc_thread.detach();
                    //publishPointCloud();

                    if (publish_laserscan) {
                        std::thread pls_thread(&LslidarChDriver::publishLaserScan, this);
                        pls_thread.detach();
                        //publishLaserScan();
                    }
                    packetType = false;
                    sweep_data = std::make_unique<lslidar_msgs::msg::LslidarScan>();
                }
            }
        }


        return true;
    }

    void LslidarChDriver::initTimeStamp(void) {

        for (int i = 0; i < 10; i++) {
            this->packetTimeStamp[i] = 0;
        }
    }

    void LslidarChDriver::difopPoll(void) {
        lslidar_msgs::msg::LslidarPacket::UniquePtr difop_packet(new lslidar_msgs::msg::LslidarPacket());

        // reading and publishing scans as fast as possible.
        while (rclcpp::ok()) {
            // keep reading
            int rc = difop_input_->getPacket(difop_packet);
            if (rc == 0) {
                // getFPGA_GPSTimeStamp(difop_packet);
                if (difop_packet->data[0] == 0xa5 && difop_packet->data[1] == 0xff && difop_packet->data[2] == 0x00 &&
                    difop_packet->data[3] == 0x5a) {
                    this->packetTimeStamp[4] = difop_packet->data[41];
                    this->packetTimeStamp[5] = difop_packet->data[40];
                    this->packetTimeStamp[6] = difop_packet->data[39];
                    this->packetTimeStamp[7] = difop_packet->data[38];
                    this->packetTimeStamp[8] = difop_packet->data[37];
                    this->packetTimeStamp[9] = difop_packet->data[36];
                    is_update_difop = true;
                }
            } else if (rc < 0) {
                return;
            }

        }
    }



/*    void LslidarChDriver::getFPGA_GPSTimeStamp(lslidar_msgs::LslidarChPacketPtr &packet) {
        unsigned char head2[] = {packet->data[0], packet->data[1], packet->data[2], packet->data[3]};

        if (head2[0] == 0xA5 && head2[1] == 0xFF) {
            if (head2[2] == 0x00 && head2[3] == 0x5A) {
                this->packetTimeStamp[7] = packet->data[54];
                this->packetTimeStamp[8] = packet->data[53];
                this->packetTimeStamp[9] = packet->data[52];
            }
        }
    }*/

} // namespace lslidar_driver

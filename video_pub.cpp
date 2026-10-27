#include <exception>
#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <chrono>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "zenoh.hxx"

// 按小端写入帧头
void write_header_le(uint8_t* buffer, uint64_t seq, uint64_t timestamp_ns,
    uint32_t width, uint32_t height) {
    std::memcpy(buffer, &seq, 8);
    std::memcpy(buffer + 8, &timestamp_ns, 8);
    std::memcpy(buffer + 16, &width, 4);
    std::memcpy(buffer + 20, &height, 4);
}

int main(int argc, char** argv){
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <video.mp4>" << std::endl;
        return 1;
    }
    cv::VideoCapture video(argv[1]);
    if (!video.isOpened()) {
        std::cerr << "Cannot open video" << std::endl;
        return 1;
    }

    auto config = zenoh::Config::create_default();
    config.insert_json5("transport/shared_memory/enabled",
                        "true");
    auto session = zenoh::Session::open(std::move(config));
    auto publisher = session.declare_publisher(
                                zenoh::KeyExpr("training/work/status"));
    cv::Mat frame;
    zenoh::PosixShmProvider provider(64 * 1024 * 1024);
    uint64_t seq = 0;


    while (video.read(frame)) {
        std::cout << frame.cols << " x " << frame.rows
        << " " << seq << std::endl;
        cv::imshow("video", frame);
        if (cv::waitKey(33) == 27) break;

        if (frame.empty() || frame.type() != CV_8UC3 ||
        !frame.isContinuous()) {
        continue; // 本片段先只处理连续的 BGR8 帧。
        }
        const uint64_t timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const uint32_t width = static_cast<uint32_t>(frame.cols);
        const uint32_t height = static_cast<uint32_t>(frame.rows);
        const size_t n = 24 + size_t(width) * height * 3;
        auto allocation = provider.alloc_gc_defrag(n);
        auto* shm = std::get_if<zenoh::ZShmMut>(&allocation);
        if (!shm) {
        // 打印分配失败并丢弃本帧，不回退普通缓冲区。
        continue;
        }
        write_header_le(shm->data(), seq, timestamp_ns,
        width, height);
        std::memcpy(shm->data() + 24, frame.data, n - 24);
        publisher.put(zenoh::Bytes(std::move(*shm)));

        seq++;
    }
}


#include <exception>
#include <iostream>
#include <string>
#include <cstring>
#include <utility>
#include <thread>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "zenoh.hxx"

#include <opencv2/opencv.hpp>
#include <vector>
#include <utility>

using namespace std;
using namespace cv;
using namespace zenoh;

//由于在识别中的核心物体以及相关的物理特性是灯条，所以建一个灯条类
class LightDescriptor
{	    //在识别以及匹配到灯条的功能中需要用到旋转矩形的长宽偏转角面积中心点坐标等
public:float width, length, angle, area;
      cv::Point2f center;
public:
    LightDescriptor() {};
    //让得到的灯条套上一个旋转矩形，以方便之后对角度这个特殊因素作为匹配标准
    LightDescriptor(const cv::RotatedRect& light)
    {
        width = light.size.width;
        length = light.size.height;
        center = light.center;
        angle = light.angle;
        area = light.size.area();
    }
};


// void drawAndShow(const Mat& img, const vector<pair<RotatedRect, RotatedRect>>& armors) {
//     //克隆显示副本
//     Mat display = img.clone();

//     for (size_t i = 0; i < armors.size(); ++i) {
//         RotatedRect left_bar = armors[i].first;
//         RotatedRect right_bar = armors[i].second;

//         Point2f pts_left[4], pts_right[4];
//         left_bar.points(pts_left);
//         right_bar.points(pts_right);

//         // 寻找左灯条的左上角、左下角，右灯条的右上角、右下角
//         Point2f tl, bl, tr, br;
        
//         // 处理左灯条
//         if (pts_left[0].y < pts_left[2].y) { tl = pts_left[0]; bl = pts_left[2]; }
//         else { tl = pts_left[2]; bl = pts_left[0]; }
        
//         // 处理右灯条
//         if (pts_right[0].y < pts_right[2].y) { tr = pts_right[0]; br = pts_right[2]; }
//         else { tr = pts_right[2]; br = pts_right[0]; }

//         // 2. 画出装甲板的完整四边形轮廓（红色，闭合多边形）
//         vector<Point2f> armor_poly = { tl, tr, br, bl };
//         polylines(display, armor_poly, true, Scalar(0, 0, 255), 2); 

//         // 3. 计算中心点（仅用于确定文字标签的位置，不在图上画出来）
//         Point2f armor_center = (left_bar.center + right_bar.center) / 2.0f;

//         // 4. 写上序号或标签（黄色）
//         char text[20];
//         sprintf(text, "Armor %d", (int)i);
//         putText(display, text, Point((int)armor_center.x - 30, (int)armor_center.y - 15),
//                 FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0, 255, 255), 2);
//     }

//     // 5. 展示结果
//     imshow("Detection Result", display);
//     waitKey(0); 
// }

void read_header_and_validate(const unsigned char* buffer, size_t size, uint32_t& width, uint32_t& height) {
    uint64_t seq = 0;
    std::memcpy(&width, buffer + 16, 4);
    std::memcpy(&height, buffer + 20, 4);
    std::memcpy(&seq, buffer, 8);
    if(size < 24 || (width <= 0 || height <= 0) || 24 + width * height * 3 != size){
        std::cout << "size: " << size << ", width: " << width << ", height: " << height << std::endl;
        throw std::runtime_error("invalid frame");
    }
    std::cout << width << " x " << height << " " << seq << std::endl;
}

auto detect_armors(const cv::Mat& frame) {
    Mat channels[3], binary, Gaussian, dilatee;
    Mat element = getStructuringElement(MORPH_RECT, Size(5, 5));
    Rect boundRect;
    RotatedRect box;
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;
    vector<Point2f> boxPts(4);

    split(frame, channels); //通道分离
    threshold(channels[0], binary, 220, 255, 0);//二值化
    GaussianBlur(binary, Gaussian, Size(5, 5), 0);//滤波
    dilate(Gaussian, dilatee, element);
    // dilate(Gaussian, dilate, element, Point(-1, -1));//膨胀，把滤波得到的细灯条变宽
    findContours(dilatee, contours, hierarchy, RETR_TREE, CHAIN_APPROX_NONE);//轮廓检测
    vector<LightDescriptor> lightInfos;//创建一个灯条类的动态数组
//筛选灯条
    for (int i = 0; i < contours.size(); i++) {
        // 求轮廓面积
        double area = contourArea(contours[i]);
        // 去除较小轮廓&fitEllipse的限制条件
        if (area < 5 || contours[i].size() <= 1)
            continue;//相当于就是把这段轮廓去除掉
        // 用椭圆拟合区域得到外接矩形（特殊的处理方式：因为灯条是椭圆型的，所以用椭圆去拟合轮廓，再直接获取旋转外接矩形即可）
        RotatedRect Light_Rec = fitEllipse(contours[i]);

        // 长宽比和轮廓面积比限制（由于要考虑灯条的远近都被识别到，所以只需要看比例即可）
        if (Light_Rec.size.width / Light_Rec.size.height > 4)
            continue;
        lightInfos.push_back(LightDescriptor(Light_Rec));
    }
    //二重循环多条件匹配灯条
    for (size_t i = 0; i < lightInfos.size(); i++) {
        for (size_t j = i + 1; (j < lightInfos.size()); j++) {
            LightDescriptor& leftLight = lightInfos[i];
            LightDescriptor& rightLight = lightInfos[j];
            float angleGap_ = abs(leftLight.angle - rightLight.angle);
            //由于灯条长度会因为远近而受到影响，所以按照比值去匹配灯条
            float LenGap_ratio = abs(leftLight.length - rightLight.length) / max(leftLight.length, rightLight.length);
            float dis = pow(pow((leftLight.center.x - rightLight.center.x), 2) + pow((leftLight.center.y - rightLight.center.y), 2), 0.5);
            //均长
            float meanLen = (leftLight.length + rightLight.length) / 2;
            float lengap_ratio = abs(leftLight.length - rightLight.length) / meanLen;
            float yGap = abs(leftLight.center.y - rightLight.center.y);
            float yGap_ratio = yGap / meanLen;
            float xGap = abs(leftLight.center.x - rightLight.center.x);
            float xGap_ratio = xGap / meanLen;
            float ratio = dis / meanLen;
            //匹配不通过的条件
            if (angleGap_ > 15 ||
                LenGap_ratio > 1.0 ||
                lengap_ratio > 0.8 ||
                yGap_ratio > 1.5 ||
                xGap_ratio > 2.2 ||
                xGap_ratio < 0.8 ||
                ratio > 3 ||
                ratio < 0.8) {
                continue;
            }
            //绘制矩形
            Point center = Point((leftLight.center.x + rightLight.center.x) / 2, (leftLight.center.y + rightLight.center.y) / 2);
            RotatedRect rect = RotatedRect(center, Size(dis, meanLen), (leftLight.angle + rightLight.angle) / 2);
            Point2f vertices[4];
            rect.points(vertices);
            for (int i = 0; i < 4; i++) {
                line(frame, vertices[i], vertices[(i + 1) % 4], Scalar(0, 0, 255), 2.2);
            }
        }
    }

    namedWindow("video", WINDOW_FREERATIO);
    imshow("video", frame);
    waitKey(5);
}

void process_frame(const zenoh::Sample& sample){
    const auto& payload = sample.get_payload();
    auto shm = payload.as_shm();
    if (!shm) {
        throw std::runtime_error("payload is not SHM");
    }
    const auto* data = shm->get().data();
    const auto size = shm->get().len();

    uint32_t width = 0, height = 0;
    read_header_and_validate(data, size, width, height);


    const cv::Mat view(height, width, CV_8UC3,
                        const_cast<uint8_t*>(data + 24));
    detect_armors(view);
    

};



int main(int argc, char* argv[]) {
    using namespace zenoh;
    std::string key = argc > 1 ? argv[1] : "training/work/status";
    auto config = Config::create_default();
    config.insert_json5("transport/shared_memory/enabled", "true");
    auto session = Session::open(std::move(config));
    auto subscriber = session.declare_subscriber(
        zenoh::KeyExpr(key),
        zenoh::channels::RingChannel(1));
    while (true) {
        auto message = subscriber.handler().try_recv();
        if (cv::waitKey(33) == 27) break;
        if (auto* sample = std::get_if<zenoh::Sample>(&message)) {
            process_frame(*sample);
        }else{
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}
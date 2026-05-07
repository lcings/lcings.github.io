/*
g++ cv.cpp -o cv.exe -I./opencv -I./inc libopencv_core452.dll.a libopencv_videoio452.dll.a libopencv_highgui452.dll.a libopencv_imgproc452.dll.a libarcsoft_face_engine.lib
https://codeload.github.com/huihut/OpenCV-MinGW-Build/zip/refs/tags/OpenCV-4.5.2-x64
https://zenlayer.dl.sourceforge.net/project/mingw-w64/Toolchains%20targetting%20Win64/Personal%20Builds/mingw-builds/8.1.0/threads-posix/seh/x86_64-8.1.0-release-posix-seh-rt_v6-rev0.7z
*/
#include <iostream>
#include <string>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include "arcsoft_face_sdk.h"

static std::string genderToString(int gender)
{
    if (gender == 0) return "Male";
    if (gender == 1) return "Female";
    return "Unknown";
}

static std::string livenessToString(int live)
{
    if (live == 1) return "Live";
    if (live == 0) return "Fake";
    if (live == -1) return "Unknown";
    if (live == -2) return "TooSmall";
    return "Unknown";
}

static ASVLOFFSCREEN cvMatToASFImageData(cv::Mat& bgr)
{
    ASVLOFFSCREEN imgData;
    memset(&imgData, 0, sizeof(ASVLOFFSCREEN));

    imgData.u32PixelArrayFormat = ASVL_PAF_RGB24_B8G8R8;
    imgData.i32Width = bgr.cols;
    imgData.i32Height = bgr.rows;
    imgData.ppu8Plane[0] = bgr.data;
    imgData.pi32Pitch[0] = static_cast<MInt32>(bgr.step[0]);

    return imgData;
}

static cv::Rect arcRectToCvRect(const MRECT& r, int width, int height)
{
    int left = std::max(0, r.left);
    int top = std::max(0, r.top);
    int right = std::min(width - 1, r.right);
    int bottom = std::min(height - 1, r.bottom);

    int w = std::max(0, right - left);
    int h = std::max(0, bottom - top);

    return cv::Rect(left, top, w, h);
}

int main(int argc, char** argv)
{
    MRESULT ret;
    MHandle hEngine = nullptr;
    MInt32 combinedMask =
        ASF_FACE_DETECT |
        ASF_FACERECOGNITION |
        ASF_AGE |
        ASF_GENDER |
        ASF_LIVENESS |
        ASF_IMAGEQUALITY |
        ASF_IR_LIVENESS |
        ASF_MASKDETECT |
        ASF_UPDATE_FACEDATA;

    cv::VideoCapture cap;
    if (argc >= 2) {
        cap.open(argv[1]);
    } else {
        cap.open(0);
    }

    if (!cap.isOpened()) {
        std::cout << "Failed to open video/camera." << std::endl;
        return -1;
    }

    //ret = ASFOfflineActivation("license.txt");
    //printf("ASFOfflineActivation:0x%x\n", ret);
    ret = ASFInitEngine(ASF_DETECT_MODE_VIDEO, ASF_OP_0_ONLY, 1, combinedMask, &hEngine);
    if (ret != 0) {
        std::cout << "ASFInitEngine failed: 0x" << std::hex << ret << std::endl;
        return -1;
    }

    ASF_LivenessThreshold liveThreshold;
    memset(&liveThreshold, 0, sizeof(ASF_LivenessThreshold));
    liveThreshold.thresholdmodel_BGR = 0.5f;
    liveThreshold.thresholdmodel_IR = 0.7f;

    ret = ASFSetLivenessParam(hEngine, &liveThreshold);
    if (ret != 0) {
        std::cout << "ASFSetLivenessParam failed: 0x" << std::hex << ret << std::endl;
    }

    double fps = 0.0;
    unsigned long cnt = 0;
    ASF_AgeInfo ageInfo;
    ASF_GenderInfo genderInfo;
    ASF_LivenessInfo liveInfo;
    CvFont font;
    cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.5, 0.5, 0, 1, 8);
    while (true) {
        cv::Mat frame;
        cap >> frame;

        if (frame.empty()) {
            break;
        }

        int validWidth = frame.cols & ~3;
        if (validWidth != frame.cols) {
            frame = frame(cv::Rect(0, 0, validWidth, frame.rows)).clone();
        }

        if (!frame.isContinuous()) {
            frame = frame.clone();
        }

        /*
        if (frame.cols > 1280) {
            double scale = 1280.0 / frame.cols;
            cv::resize(frame, frame, cv::Size(), scale, scale);
        }
        */

        ASVLOFFSCREEN imgData = cvMatToASFImageData(frame);
        memset(&ageInfo, 0, sizeof(ASF_AgeInfo));
        memset(&genderInfo, 0, sizeof(ASF_GenderInfo));
        memset(&liveInfo, 0, sizeof(ASF_LivenessInfo));
        ASF_MultiFaceInfo detectedFaces;
        memset(&detectedFaces, 0, sizeof(ASF_MultiFaceInfo));
        auto lastTime = std::chrono::high_resolution_clock::now();
        ret = ASFDetectFacesEx(hEngine, &imgData, &detectedFaces);
        if (ret == 0 && detectedFaces.faceNum > 0) {
            MInt32 processMask = ASF_AGE | ASF_GENDER | ASF_LIVENESS;
            ret = ASFProcessEx(hEngine, &imgData, &detectedFaces, processMask);
            if (ret == 0) {
                ASFGetAge(hEngine, &ageInfo);
                ASFGetGender(hEngine, &genderInfo);
                ASFGetLivenessScore(hEngine, &liveInfo);
            }
        }

        auto now = std::chrono::high_resolution_clock::now();
        double costMs = std::chrono::duration<double, std::milli>(now - lastTime).count();
        IplImage ipl_img = cvIplImage(frame);
        for (int i = 0; i < detectedFaces.faceNum; ++i) {
            MRECT rect = detectedFaces.faceRect[i];
            cv::Rect faceRect = arcRectToCvRect(rect, frame.cols, frame.rows);

            if (faceRect.width <= 0 || faceRect.height <= 0) {
                continue;
            }

            int age = -1;
            int gender = -1;
            int live = -1;

            float yaw = 0.0f;
            float pitch = 0.0f;
            float roll = 0.0f;

            if (ageInfo.num > i && ageInfo.ageArray) {
                age = ageInfo.ageArray[i];
            }

            if (genderInfo.num > i && genderInfo.genderArray) {
                gender = genderInfo.genderArray[i];
            }

            if (liveInfo.num > i && liveInfo.isLive) {
                live = liveInfo.isLive[i];
            }

            cv::Scalar boxColor;

            if (live == 1) {
                boxColor = cv::Scalar(0, 255, 0);
            } else if (live == 0) {
                boxColor = cv::Scalar(0, 0, 255);
            } else {
                boxColor = cv::Scalar(0, 255, 255);
            }

            cv::rectangle(frame, faceRect, boxColor, 1);

            std::string text1 =
                "Age:" + std::to_string(age) +
                ",Gender:" + genderToString(gender) +
                ",Liveness:" + livenessToString(live);

            std::string text2 =
                "Yaw:" + std::to_string(static_cast<int>(yaw)) +
                ",Pitch:" + std::to_string(static_cast<int>(pitch)) +
                ",Roll:" + std::to_string(static_cast<int>(roll));
            //std::cout << text1 << text2;
            int baseY = std::max(20, faceRect.y - 10);
            cvPutText(&ipl_img, text1.c_str(), cvPoint(faceRect.x, baseY), &font, cvScalar(0, 255, 0));
            cvPutText(&ipl_img, text2.c_str(), cvPoint(faceRect.x, faceRect.y + faceRect.height + 20), &font, cvScalar(0, 255, 0));
            //cv::putText(frame, text1, cv::Point(faceRect.x, baseY), cv::FONT_HERSHEY_SIMPLEX, 0.6, boxColor, 2);
            //cv::putText(frame, text2, cv::Point(faceRect.x, faceRect.y + faceRect.height + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, boxColor, 1 );
        }

        double curFps = 1000.0 / std::max(1.0, costMs);
        fps += curFps;
        std::string fpsText = "FPS: " + std::to_string(static_cast<int>(fps / ++cnt));

        cvPutText(&ipl_img, fpsText.c_str(), cvPoint(0, 14), &font, cvScalar(255, 0, 0));
        //cv::putText(frame, fpsText, cv::Point(20, 40), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 0, 0), 2 );
        //std::cout << fpsText << std::endl;
        cv::imshow("FacePro RealTime", frame);

        int key = cv::waitKey(1);
        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }
    }

    cap.release();
    cv::destroyAllWindows();
    ASFUninitEngine(hEngine);
    return 0;
}

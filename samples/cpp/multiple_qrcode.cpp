#include "opencv2/objdetect.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/videoio.hpp"
#include <string>
#include <iostream>

using namespace std;
using namespace cv;

static void drawQRCodeContour(Mat &color_image, vector<vector<Point>> transform);
static void drawFPS(Mat &color_image, double fps);
static int  liveQRCodeDetect(const string& out_file);
static int  imageQRCodeDetect(const string& in_file, const string& out_file);

int main(int argc, char *argv[])
{
    const string keys =
        "{h help ? |        | print help messages }"
        "{i in     |        | input  path to file for detect (with parameter - show image, otherwise - camera)}"
        "{o out    |        | output path to file (save image, work with -i parameter) }";
    CommandLineParser cmd_parser(argc, argv, keys);

    cmd_parser.about("This program detects the QR-codes from camera or images using the OpenCV library.");
    if (cmd_parser.has("help"))
    {
        cmd_parser.printMessage();
        return 0;
    }

    string in_file_name  = cmd_parser.get<string>("in");    // input  path to image
    string out_file_name;
    if (cmd_parser.has("out"))
        out_file_name = cmd_parser.get<string>("out");   // output path to image

    if (!cmd_parser.check())
    {
        cmd_parser.printErrors();
        return -1;
    }

    int return_code = 0;
    if (in_file_name.empty())
    {
        return_code = liveQRCodeDetect(out_file_name);
    }
    else
    {
        return_code = imageQRCodeDetect(samples::findFile(in_file_name), out_file_name);
    }
    return return_code;
}

void drawQRCodeContour(Mat &color_image, vector<vector<Point>> transform)
{
    for(size_t i = 0; i < transform.size(); i++)
    {
        if (!transform[i].empty())
        {
            double show_radius = (color_image.rows  > color_image.cols)
                       ? (2.813 * color_image.rows) / color_image.cols
                       : (2.813 * color_image.cols) / color_image.rows;
            double contour_radius = show_radius * 0.4;
            vector<vector<Point>> contours;
            contours.push_back(transform[i]);
            drawContours(color_image, contours, 0, Scalar(211, 0, 148), cvRound(contour_radius));

            RNG rng(1000);
            for (size_t j = 0; j < 4; j++)
            {
                Scalar color = Scalar(rng.uniform(0,255), rng.uniform(0, 255), rng.uniform(0, 255));
                circle(color_image, transform[i][j], cvRound(show_radius), color, -1);
            }
        }
    }
}

void drawFPS(Mat &color_image, double fps)
{
    ostringstream convert;
    convert << cvRound(fps) << " FPS (QR detection)";
    putText(color_image, convert.str(), Point(25, 25), FONT_HERSHEY_DUPLEX, 1, Scalar(0, 0, 255), 2);
}

int liveQRCodeDetect(const string& out_file)
{
    VideoCapture cap(0);
    if(!cap.isOpened())
    {
        cout << "Cannot open a camera" << endl;
        return -4;
    }

    QRCodeDetector qrcode;
    TickMeter total;
    for(;;)
    {
        Mat frame, src;
        vector<Mat> straight_barcode;
        vector<cv::String> decode_info;
        vector<vector<Point>> transform;
        cap >> frame;
        if (frame.empty())
        {
            cout << "End of video stream" << endl;
            break;
        }
        cvtColor(frame, src, COLOR_BGR2GRAY);

        total.start();
        bool result_detection = qrcode.MultipleDetect(src, transform);
        if (result_detection)
        {
            decode_info = qrcode.MultipleDecode(src, transform, straight_barcode);
            for(size_t i = 0; i < decode_info.size(); i++)
            {
                if (!decode_info[i].empty()) { cout << decode_info[i] << endl; }
            }
        }
        total.stop();
        double fps = 1 / total.getTimeSec();
        total.reset();

        if (result_detection) { drawQRCodeContour(frame, transform); }
        drawFPS(frame, fps);

        imshow("Live QR code detector", frame);
        char c = (char)waitKey(30);
        if (c == 27)
            break;
        if (c == ' ' && !out_file.empty())
            imwrite(out_file, frame); // TODO write original frame too
    }
    return 0;
}

int imageQRCodeDetect(const string& in_file, const string& out_file)
{
    Mat color_src = imread(in_file, IMREAD_COLOR), src;
    cvtColor(color_src, src, COLOR_BGR2GRAY);
    vector<Mat> straight_barcode;
    vector<cv::String> decoded_info;
    vector<vector<Point>> transform;
    const int count_experiments = 10;
    double transform_time = 0.0;
    bool result_detection = false;
    TickMeter total;
    QRCodeDetector qrcode;
    for (size_t i = 0; i < count_experiments; i++)
    {
        total.start();
        transform.clear();
        result_detection = qrcode.MultipleDetect(src, transform);
        total.stop();
        transform_time += total.getTimeSec();
        total.reset();
        if (!result_detection)
            continue;

        total.start();
        decoded_info = qrcode.MultipleDecode(src, transform, straight_barcode);
        total.stop();
        transform_time += total.getTimeSec();
        total.reset();
    }
    double fps = count_experiments / transform_time;
    if (!result_detection)
        cout << "QR code not found" << endl;
    for(size_t i = 0; i < decoded_info.size(); i++)
    {
        if (decoded_info[i].empty())
            cout << "QR code cannot be decoded" << endl;
    }
    drawQRCodeContour(color_src, transform);
    drawFPS(color_src, fps);

    cout << "Input  image file path: " << in_file  << endl;
    cout << "Output image file path: " << out_file << endl;
    cout << "Size: " << color_src.size() << endl;
    cout << "FPS: " << fps << endl;
    for(size_t i = 0; i < decoded_info.size(); i++)
        cout << "Decoded info: " << decoded_info[i] << endl;

    if (!out_file.empty())
    {
        imwrite(out_file, color_src);
    }

    for(;;)
    {
        imshow("Detect QR code on image", color_src);
        if (waitKey(0) == 27)
            break;
    }
    return 0;
}

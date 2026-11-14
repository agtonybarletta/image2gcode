#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using namespace cv;

namespace {

double bound(double x, double from, double to) {
    return x < from ? from : (x > to ? to : x);
}

void dithering(Mat& src, Mat& dst) {
    // Floyd–Steinberg error-diffusion weights.
    constexpr double kErrorEast = 7.0 / 16.0;
    constexpr double kErrorSouthWest = 3.0 / 16.0;
    constexpr double kErrorSouth = 5.0 / 16.0;
    constexpr double kErrorSouthEast = 1.0 / 16.0;

    for (int i = 1; i < dst.cols - 1; i++) {
        for (int j = 1; j < dst.rows - 1; j++) {
            const double old_pixel = dst.at<double>(j, i);
            const double new_pixel = old_pixel > 0.5 ? 1 : 0;  // int on purpose (original semantics)
            const double error = old_pixel - new_pixel;

            dst.at<double>(j, i) = new_pixel;
            dst.at<double>(j + 1, i)     = bound(dst.at<double>(j + 1, i)     + error * kErrorEast,     0.0, 1.0);
            dst.at<double>(j - 1, i + 1) = bound(dst.at<double>(j - 1, i + 1) + error * kErrorSouthWest, 0.0, 1.0);
            dst.at<double>(j,     i + 1) = bound(dst.at<double>(j,     i + 1) + error * kErrorSouth,     0.0, 1.0);
            dst.at<double>(j + 1, i + 1) = bound(dst.at<double>(j + 1, i + 1) + error * kErrorSouthEast, 0.0, 1.0);
        }
    }
}

// Minimal flag parser: keeps the original CLI contract (the image path is the
// last argument) while removing the repetitive std::find blocks.
struct Options {
    std::string image_path;
    double beam_width = 0.0;
    double x0 = 0.0;
    double y0 = 0.0;
    double width_mm = 0.0;
    double height_mm = 0.0;
    std::string preview_path;
    std::string gcode_path;
};

Options parse_command_line(int argc, char* argv[]) {
    Options opts;

    auto flag_value = [&](const std::string& flag) -> std::string {
        auto it = std::find(argv, argv + argc, flag);
        if (it != argv + argc && ++it != argv + argc) {
            return std::string(*it);
        }
        return std::string();
    };
    auto as_double = [&](const std::string& flag, double fallback) {
        std::string value = flag_value(flag);
        return value.empty() ? fallback : std::stod(value);
    };

    opts.beam_width   = as_double("--beamWidth", 0.0);
    opts.x0           = as_double("--x0", 0.0);
    opts.y0           = as_double("--y0", 0.0);
    opts.width_mm     = as_double("--wf", 0.0);
    opts.height_mm    = as_double("--hf", 0.0);
    opts.preview_path = flag_value("--output_file_img");
    opts.gcode_path   = flag_value("--output_gcode");
    opts.image_path   = argv[argc - 1];

    std::cout << "-- beamWidth "        << opts.beam_width  << "\n"
              << "-- x0 "               << opts.x0          << "\n"
              << "-- y0 "               << opts.y0          << "\n"
              << "-- wf "               << opts.width_mm    << "\n"
              << "-- hf "               << opts.height_mm   << "\n"
              << "-- output_file_img "  << opts.preview_path << "\n"
              << "-- output_gcode "     << opts.gcode_path   << "\n";
    return opts;
}

}  // namespace

int main(int argc, char* argv[]) {
    const Options opts = parse_command_line(argc, argv);

    Mat img = imread(samples::findFile(opts.image_path), IMREAD_COLOR);
    std::cout << "(" << img.rows << ", " << img.cols << ") | channels: "
              << img.channels() << " | type: " << img.type() << std::endl;
    if (img.empty()) {
        std::cout << "Could not read the image: " << opts.image_path << std::endl;
        return 1;
    }

    imshow("Display window", img);
    waitKey(0);

    // Scale the image so it fits inside the requested work area while keeping
    // its aspect ratio.
    const double rowf = opts.height_mm / opts.beam_width;
    const double colf = opts.width_mm / opts.beam_width;

    const double init_ratio = static_cast<double>(img.rows) / img.cols;
    const double final_ratio = rowf / colf;
    const bool is_x_scaling = final_ratio > init_ratio;
    const double scale_rate = is_x_scaling ? colf / img.cols : rowf / img.rows;

    Size target_size;
    target_size.width = static_cast<int>(img.cols * scale_rate);
    target_size.height = static_cast<int>(img.rows * scale_rate);
    Mat scaled_img;
    cv::resize(img, scaled_img, target_size, INTER_CUBIC);

    imshow("Display window", scaled_img);
    waitKey(0);

    // Grayscale, normalized to [0, 1] for the dithering pass.
    Mat grayscale_img;
    cvtColor(scaled_img, grayscale_img, COLOR_BGR2GRAY);

    imshow("Display window", grayscale_img);
    waitKey(0);

    Mat src;
    grayscale_img.convertTo(src, CV_64FC1, 1.0 / 256.0, 0.0);
    Mat dst = src.clone();

    dithering(src, dst);

    Mat dithered_img;
    dst.convertTo(dithered_img, CV_8UC1, 255, 0);
    imshow("Display window", dithered_img);
    waitKey(0);

    // Flip vertically so the engraved pattern matches the machine's origin.
    Mat flipped_img;
    flip(dithered_img, flipped_img, 0);

    if (!opts.preview_path.empty()) {
        imwrite(opts.preview_path, flipped_img);
    }

    imshow("Display window", flipped_img);
    waitKey(0);

    const std::string head = std::string(
        "G90     ;Absolute programming\n\n"
        "G21     ;Programming in millimeters (mm)\n"
        "G0 X50 Y10 Z100\n"
        "G92 X0 Y0 Z0\n"
        "M03 S1 ;Activate laser and set power to 0 (CUSTOM)\n"
        "\n"
        "; START perimeter strategy\n"
        "G01 F400.0\n"
        "; 5 strips 0.1\n"
        "M107\n");

    const std::string tail = std::string(
        "G00 X0 Y0\n"
        "M05 S0  ;Deactivate laser and set power to 0 (CUSTOM)\n"
        "G0 X-55 Y-20 Z-100\n"
        "G92 X0 Y0 Z0\n"
        "M02 ;End of program\n");

    std::ofstream gcode_stream(opts.gcode_path);
    if (!gcode_stream.is_open()) {
        std::cout << "Unable to open file: " << opts.gcode_path << std::endl;
        return 1;
    }

    gcode_stream << head;
    gcode_stream << std::fixed << std::setprecision(2);

    constexpr int kBurnDelayMs = 65;
    constexpr int kCoolDownDelayMs = 80;
    for (int i = 0; i < flipped_img.cols; i++) {
        for (int j = 0; j < flipped_img.rows; j++) {
            if (flipped_img.at<uchar>(j, i) != 0) {
                continue;
            }
            const double x = opts.x0 + i * opts.beam_width;
            const double y = opts.y0 + j * opts.beam_width;
            gcode_stream << "G00 X" << x << " Y" << y << " F3000.0\n";
            gcode_stream << "M106 S255 F3000.0\n";
            gcode_stream << "G4 P" << kBurnDelayMs << "\n";
            gcode_stream << "M106 S10 F3000.0\n";
            gcode_stream << "G4 P" << kCoolDownDelayMs << "\n";
        }
    }

    gcode_stream << "M107\n";
    gcode_stream << tail;
    gcode_stream.close();

    return 0;
}

#include <iostream>
#include <cstdlib>
#include <ippi.h>
#include <ipps.h>
#include <ippcore.h>
#include <ippcc.h>
#include <jpeglib.h>
#include <fstream>
#include <opencv2/opencv.hpp>
#include "../include/screenshot.h"
#include "../include/cmdopts.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/X.h>

using namespace std;
// main() is where program execution begins.

/*
void compressYUYVtoJPEG(const uint8_t* input, const int width, const int height, vector<uint8_t>& output){
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];
    int row_stride;

    uint8_t* outbuffer = NULL;
    uint64_t outlen = 0;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &outbuffer, &outlen);

    // jrow is a libjpeg row of samples array of 1 row pointer
    cinfo.image_width = width & -1;
    cinfo.image_height = height & -1;
    cinfo.input_components = 2;
    cinfo.in_color_space = JCS_YCbCr; //libJPEG expects YUV 3bytes, 24bit

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 100, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    vector<uint8_t> tmprowbuf(width * 2);

    JSAMPROW row_pointer[1];
    row_pointer[0] = &tmprowbuf[0];
    while (cinfo.next_scanline < cinfo.image_height) {
        unsigned i, j;
        unsigned offset = cinfo.next_scanline * cinfo.image_width * 2; //offset to the correct row
        for (i = 0, j = 0; i < cinfo.image_width * 2; i += 4, j += 6) { //input strides by 4 bytes, output strides by 6 (2 pixels)
            tmprowbuf[j + 0] = input[offset + i + 0]; // Y (unique to this pixel)
            tmprowbuf[j + 1] = input[offset + i + 1]; // U (shared between pixels)
            tmprowbuf[j + 2] = input[offset + i + 3]; // V (shared between pixels)
            tmprowbuf[j + 3] = input[offset + i + 2]; // Y (unique to this pixel)
            tmprowbuf[j + 4] = input[offset + i + 1]; // U (shared between pixels)
            tmprowbuf[j + 5] = input[offset + i + 3]; // V (shared between pixels)
        }
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    std::cout << "libjpeg produced " << outlen << " bytes" << endl;

    output = vector<uint8_t>(outbuffer, outbuffer + outlen);
};

void DisplayRgbFrame(const char* buffer, uint32_t width, uint32_t height) {
    GtkWidget* window;
    GtkWidget* image;

    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_data(
        (const guchar*)buffer,          // data
        GDK_COLORSPACE_RGB,             // colorspace
        true,                           // has alpha?
        8,          // bits per sample
        width,                          // width
        height,                         // height
        width * 4, // rowstride
        NULL,                           // destroy function
        NULL                            // destroy data
    );

    // gdk_pixbuf_save (pixbuf, "rgb-yuv-rgb.jpg", "jpeg", NULL, "quality", "100", NULL);

    image = gtk_image_new_from_pixbuf(pixbuf);
    gtk_container_add(GTK_CONTAINER(window), image);

    gtk_widget_show_all(window);

    gtk_main();
}
*/

void convertX11ImageToYUV422(Display* display, XImage* ximage, Ipp8u* dstImageY, Ipp8u* dstImageU, Ipp8u* dstImageV) {
    int srcWidth = ximage->width;
    int srcHeight = ximage->height;
    int dstWidth = srcWidth;
    int dstHeight = srcHeight;

    // Create an IPP-compatible image
    IppiSize roiSize = { srcWidth, srcHeight };
    int srcStep = ximage->bytes_per_line;
    std::cout << srcStep << std::endl;
    int dstStepY = dstWidth * sizeof(Ipp8u);
    int dstStepUV = dstWidth / 2 * sizeof(Ipp8u);
    
    // Pointers to destination planes
    Ipp8u* pDst[3] = { dstImageY, dstImageU, dstImageV };
    
    // Step sizes for destination planes
    int dstStep[3] = { dstStepY, dstStepUV, dstStepUV };

    // Convert RGB to YUV422
    ippiRGBToYUV422_8u_C3P3R((const Ipp8u*)ximage->data, srcStep, pDst, dstStep, roiSize);
}

void saveRGBToImageFile(const std::string& filename, char* dstImage, int width, int height) {
    cv::Mat rgbImage(height, width, CV_8UC3, dstImage);

    // Save the image as a file
    cv::imwrite(filename, rgbImage);
}

void saveYUV422ToImageFile(const std::string& filename, char* dstImage, int width, int height) {
    // Create an OpenCV Mat object with YUV422 data
    cv::Mat yuvImage(height, width, CV_8UC2, dstImage);

    // Convert YUV422 to RGB
    cvtColor(yuvImage, yuvImage, cv::COLOR_YUV2RGB_YUYV);

    // Save the image as a file
    cv::imwrite(filename, yuvImage);
}

int main(int argc, char ** argv) {
    Display * display;
    Window root;
    int width = 0;
    int height = 0;
    XWindowAttributes gwa;
    
    X11Screenshot screenshot;
    // options
    cmd_options opts = process_options(argc, argv);
    if (opts.verbose) {
        std::cout << "Options" << std::endl;
        std::cout << "width: " << opts.width << std::endl;
        std::cout << "Height: " << opts.height << std::endl;
        std::cout << "Quality: " << opts.quality << std::endl;
        std::cout << "Verbose: " << opts.verbose << std::endl;
        std::cout << "Path: " << opts.path << std::endl;
        std::cout << "Type: " << opts.type << std::endl;
    }
    

    display = XOpenDisplay(NULL);
    if (display == NULL) {
        std::cerr << "No display can be aquired" << std::endl;
        exit(1);
    }
    root = DefaultRootWindow(display);

    XGetWindowAttributes(display, root, &gwa);
    width = gwa.width;
    height = gwa.height;

    if (opts.verbose) {
        std::cout << "Original screen width: " << width << std::endl;
        std::cout << "Original screen height: " << height << std::endl;
    }

    XImage * ximage = XGetImage(
        display,
        root,
        0,
        0,
        width,
        height,
        AllPlanes,
        ZPixmap
    );

    if (std::string(opts.path).length() == 0) {
        std::cerr << "Invalid arguments, use --help to see details." << std::endl;
        exit(0);
    };

    if ((opts.width != 0 and opts.width != width) and (opts.height != 0 and opts.height != height)) {
        screenshot = X11Screenshot(ximage, opts.width, opts.height, "bilinear");
        if (opts.verbose) std::cout << "Process with resizing" << std::endl;
    } else {
        screenshot = X11Screenshot(ximage);
        if (opts.verbose) std::cout << "Process without resizing" << std::endl;
    }

    if (std::string(opts.type) == "png") {
        if (opts.verbose) std::cout << "Saving as png" << std::endl;
        if (screenshot.save_to_png(opts.path)) {
            if (opts.verbose) std::cout << "Succesfully saved to " << opts.path << std::endl;
        }
    }

    if (std::string(opts.type) == "jpg") {
        if (opts.verbose) std::cout << "Saving as jpg" << std::endl;
        if (screenshot.save_to_jpeg(opts.path, opts.quality)) {
            if (opts.verbose) std::cout << "Succesfully saved to " << opts.path << std::endl;
        }
    }

    IppiSize srcSize = {width, height};
    char* input = ximage->data;
    int srcStep = width * sizeof(Ipp8u) * 4;

    int outputStep = width * sizeof(Ipp8u) * 3;
    std::vector<char> output(width * height * 3);

    ippiCopy_8u_AC4C3R(reinterpret_cast<Ipp8u*> (input), srcStep, reinterpret_cast<Ipp8u*> (output.data()), outputStep, srcSize);

    saveRGBToImageFile("rgb.jpg", output.data(), width, height);

    int output_YUVStep = width * sizeof(Ipp8u) * 2;
    std::vector<char> output_YUV(width * height * 2);

    IppStatus status = ippiRGBToYUV422_8u_C3C2R(reinterpret_cast<Ipp8u*> (output.data()), outputStep,
        reinterpret_cast<Ipp8u*> (output_YUV.data()), output_YUVStep, srcSize
    );

    if(status != ippStsNoErr) {
        std::cerr << "ippiRGBToYUV422_8u_C3C2R err" << '\n';
        XDestroyImage(ximage);
        XCloseDisplay(display);
        return false;
    }
    
    saveYUV422ToImageFile("yuv.jpg", output_YUV.data(), width, height);

    // Cleanup X11 resources
    XDestroyImage(ximage);
    XCloseDisplay(display);

    // Perform further processing or save the YUV422 image as needed

    return 0;
}

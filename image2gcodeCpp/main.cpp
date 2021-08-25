#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <iostream>
#include <fstream>
#include <iomanip> 
using namespace cv;
void dithering(Mat* src, Mat* dst);
void printM(Mat tmp);
double bound(double x, double from, double to);
int main( int argc, char* argv[])
{
	// parse command line
    std::string image_path = samples::findFile((std::string)argv[argc-1]);
	auto itr = std::find(argv, argv+argc, std::string("--beamWidth"));
	double beamWidth = 0.0f;
	beamWidth = itr != argv+argc && ++itr != argv+argc ? std::stod(std::string(*itr)): 0.0;
	std::cout << "-- beamWidth "<< beamWidth <<std::endl;

	double x0 = 0.0f;
	itr = std::find(argv, argv+argc, std::string("--x0"));
	x0 = itr != argv+argc && ++itr != argv+argc ? std::stod(std::string(*itr)): 0.0;
	std::cout << "-- x0 "<< x0 <<std::endl;

	double y0 = 0.0f;
	itr = std::find(argv, argv+argc, std::string("--y0"));
	y0 = itr != argv+argc && ++itr != argv+argc ? std::stod(std::string(*itr)): 0.0;
	std::cout << "-- y0 "<< y0 <<std::endl;

	double wf = 0.0f;
	itr = std::find(argv, argv+argc, std::string("--wf"));
	wf = itr != argv+argc && ++itr != argv+argc ? std::stod(std::string(*itr)): 0.0;
	std::cout << "-- wf "<< wf <<std::endl;

	double hf = 0.0f;
	itr = std::find(argv, argv+argc, std::string("--hf"));
	hf = itr != argv+argc && ++itr != argv+argc ? std::stod(std::string(*itr)): 0.0;
	std::cout << "-- hf "<< hf <<std::endl;

	std::string output_file_img = std::string("");
	itr = std::find(argv, argv+argc, std::string("--output_file_img"));
	output_file_img = itr != argv+argc && ++itr != argv+argc ? std::string(*itr): "";
	std::cout << "-- output_file_img "<< output_file_img <<std::endl;

	std::string output_gcode = std::string("");
	itr = std::find(argv, argv+argc, std::string("--output_gcode"));
	output_gcode = itr != argv+argc && ++itr != argv+argc ? std::string(*itr): "";
	std::cout << "-- output_gcode "<< output_gcode <<std::endl;
	
    Mat img = imread(image_path, IMREAD_COLOR);
	std::cout << "( "<< img.rows << ", " << img.cols << " )" << std::endl;
    if(img.empty())
    {
        std::cout << "Could not read the image: " << image_path << std::endl;
        return 1;
    }
	int k;
    imshow("Display window", img);
    k = waitKey(0); // Wait for a keystroke in the window

	std::cout << "img.cols: " << img.cols<< std::endl;
	std::cout << "img.rows: " << img.rows<< std::endl;
	std::cout << "img.chn: " << img.channels()<< std::endl;
	std::cout << "img.type: " << img.type()<< std::endl;

	double rowf= hf/beamWidth;
	double colf = wf/beamWidth;

	std::cout << "rowf :"<< rowf << " colf : " << colf << std::endl;
	std::cout << "img.rows :"<< img.rows << " img.cols : " << img.cols << std::endl;

	//get direction of resize
	double init_ratio = (double)img.rows/img.cols;	
	double final_ratio =rowf/colf;	
	std::cout << "init_ratio :"<< init_ratio << " final_ratio : " << final_ratio << std::endl;
	bool is_x_scaling = false;
	if(final_ratio > init_ratio){
		is_x_scaling = true;
	}
	std::cout << "is_x_scaling: " << is_x_scaling << std::endl;
	double scale_rate = is_x_scaling ? colf/img.cols : rowf/img.rows ;
	std::cout << "scale_rate : " << scale_rate << std::endl;

	// resize image
	Mat scaled_img = img.clone();
	std::cout << "mat type: "<< img.type()<< std::endl;
	Size s;
	s.height = img.rows*scale_rate;
	s.width =  img.cols*scale_rate ;
	cv::resize(img, scaled_img, s, INTER_CUBIC);

    imshow("Display window", scaled_img);
    k = waitKey(0); // Wait for a keystroke in the window
	//return 0;

	Mat black_white_img = scaled_img.clone();		
	cvtColor(scaled_img, black_white_img, COLOR_BGR2GRAY);

    imshow("Display window", black_white_img);
    k = waitKey(0); // Wait for a keystroke in the window

	Mat src = Mat(black_white_img.size(), CV_64FC1);
	src = black_white_img.clone();
	black_white_img.convertTo(src, CV_64FC1, 1.0/256.0, 0.0);
	Mat dst = Mat(src.size(), CV_64FC1);
	dst = src.clone();

	dithering(&src, &dst);
	Mat dithered_img = Mat(dst.size(), CV_8U);
	dst.convertTo(dithered_img, CV_8UC1, 255,0);
	//dithered_img = dst.clone();
    imshow("Display window", dithered_img);
    k = waitKey(0); // Wait for a keystroke in the window


	Mat flipped_img = Mat(dst.size(), CV_8U);
	flip(dithered_img, flipped_img,0);

    if(output_file_img.compare("") != 0){
			imwrite(output_file_img, flipped_img );
	}	

    imshow("Display window", flipped_img);
    k = waitKey(0); // Wait for a keystroke in the window

	std::string head = std::string(
		"G90     ;Absolute programming\n\n"
		"G21     ;Programming in millimeters (mm)\n"
		"G0 X50 Y10 Z100\n"
		"G92 X0 Y0 Z0\n"
		"M03 S1 ;Activate laser and set power to 0 (CUSTOM)\n"
		"\n"
		"; START perimeter strategy\n"
		"G01 F400.0\n"
		"; 5 strips 0.1\n"
		"M107\n"
		);

	std::string tail = std::string(
		"G00 X0 Y0\n"
		"M05 S0  ;Deactivate laser and set power to 0 (CUSTOM)\n"
		"G0 X-55 Y-20 Z-100\n"
		"G92 X0 Y0 Z0\n"
		"M02 ;End of program\n"
	);

	std::ofstream gcode_stream (output_gcode);
	if (gcode_stream.is_open())
	{
		gcode_stream << head;
		gcode_stream << std::fixed << std::setprecision(2);

		int startDelay = 65;
		int endDelay = 80;
		int velocity = 300;
		for(int i = 0; i < flipped_img.cols; i++){
			for( int j = 0; j < flipped_img.rows; j++){
				if( flipped_img.at<uchar>(j,i) == 0 ){
					// 1 px;
					double x = (double)x0 + (double)i *beamWidth;
					double y = y0 + (double)j *beamWidth;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
						
				}
			}
		}

		/*	

		int boxl = 10;
		// dotted
		gcode_stream << "; dotted\n";
		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
				if( ( i%2 == 0 && (j+1) %2 == 0 ) || ((i+1)%2 == 0 && j%2 ==0)){	
					double x = (double)x0 + (double)i *beamWidth;
					double y = y0 + (double)j *beamWidth;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
			}
		}

		y0 += boxl;

		gcode_stream << "; dotted2\n";
		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
				if( !( i%2 == 0 && j %2 == 0 )){	
					double x = (double)x0 + (double)i *beamWidth;
					double y = y0 + (double)j *beamWidth;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
			}
		}
		
		y0 += boxl;

		gcode_stream << "; dottedfull\n";
		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
					double x = (double)x0 + (double)i *beamWidth;
					double y = y0 + (double)j *beamWidth;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
			}
		}

		x0 += boxl;
		gcode_stream << "; lines x\n";
		for(int i = 0; i < boxl/beamWidth; i++){
				if( i%2 == 0){	
					double x = (double)x0 + (double)i *beamWidth;
					double y = y0;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x << " Y" << (double)y+boxl<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
		}

		x0 += boxl;
		gcode_stream << "; lines y\n";
		for(int i = 0; i < boxl/beamWidth; i++){
				if( i%2 == 0){	
					double x = (double)x0 ;
					double y = y0+ (double)i *beamWidth;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x +boxl<< " Y" << y<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";

				}
		}

		x0 += boxl;
		gcode_stream << "; lines overlapped \n";
		for(int i = 0; i < boxl/beamWidth; i++){
				if( i%2 == 0){	
					double x = (double)x0 ;
					double y = y0+ (double)i *beamWidth;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x +boxl<< " Y" << y<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
		}
		for(int i = 0; i < boxl/beamWidth; i++){
				if( i%2 == 0){	
					double x = (double)x0 + (double)i *beamWidth;
					double y = y0;
					gcode_stream<< "G00 X" <<x << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x << " Y" << (double)y+boxl<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
		}
		//y0 += boxl;
		//x0 -= 3*boxl;

		x0 += boxl;
		gcode_stream << "; lines dotted x\n";
		
		//TODO change
		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
				if( (i)%2 == 0 && ((j+i)%3 ==0 || (j+1)%3 ==1) ){	

					double x = (double)x0 + (double)i *beamWidth;
					double y = y0;
					gcode_stream<< "G00 X" <<x << " Y" << (double)y+(j)*beamWidth<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x << " Y" << (double)y+(j+1)*beamWidth<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
			}
		}

		x0 += boxl;
		gcode_stream << "; lines dotted y\n";
		/TODO CHANGE
		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
				if( j%2 == 0 && ( (i+j)%3 ==0 || (i+j)%3 ==1 ) ){

					double x = (double)x0;
					double y = y0 + j* beamWidth;
					gcode_stream<< "G00 X" <<x +(i)*beamWidth << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x +(i+1)*beamWidth<< " Y" << (double)y<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
			}
		}
		x0 += boxl;
		gcode_stream << "; lines dotted y - 5\n";
		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
				if( (i)%2 == 0 && (j+i)%5 ==0 ){	

					double x = (double)x0 + (double)i *beamWidth;
					double y = y0;
					gcode_stream<< "G00 X" <<x << " Y" << (double)y+(j)*beamWidth<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x << " Y" << (double)y+(j+4)*beamWidth<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
			}
		}

		x0 += boxl;
		gcode_stream << "; lines dotted y - 5\n";

		for(int i = 0; i < boxl/beamWidth; i++){
			for(int j = 0; j < boxl/beamWidth; j++){
				if( j%2 == 0 && (i+j)%5 ==0 ){	

					double x = (double)x0;
					double y = y0 + j* beamWidth;
					gcode_stream<< "G00 X" <<x +(i)*beamWidth << " Y" << y<< " F3000.0\n";
					gcode_stream << "M106 S255 F3000.0\n";
					gcode_stream << "G4 P"<< startDelay << "\n";
					gcode_stream<< "G01 X" <<x +(i+4)*beamWidth<< " Y" << (double)y<< " F"<<velocity<<"\n";
					//gcode_stream << "M107\n";
					gcode_stream << "M106 S10 F3000.0\n";
					gcode_stream << "G4 P"<<endDelay<<"\n";
				}
			}
		}

		*/

		gcode_stream << "M107\n";

		gcode_stream << tail;
		gcode_stream.close();
	} else {
		std::cout << "Unable to open file\n";
	}
	return 0;
}

void dithering(Mat* _src, Mat* _dst){

	Mat src = *_src;
	Mat dst = *_dst;
	double w_e = 7.0/16;
	double w_sw = 3.0/16;
	double w_s = 5.0/16;
	double w_se = 1.0/16;


	//printM(src);
	for( int i = 1; i < dst.cols-1; i++){
		for(int j = 1; j < dst.rows-1; j++){

			//printM(dst);
			//std::cout << "--" << std::endl;

			double old_pixel = dst.at<double>(j,i);	
			double new_pixel = old_pixel > 0.5 ? 1 : 0;

			dst.at<double>(j,i) =  new_pixel ;
			double error = (double) old_pixel - new_pixel;

			//std::cout << "--0" << std::endl;
			//printM(dst);
			double update = (double)dst.at<double>(j+1,i) + (error * w_e);
			dst.at<double>(j+1,i) = bound(update, 0, 1);
			//std::cout << "--1" << std::endl;
			//printM(dst);

			update = dst.at<double>(j-1,i+1) + (error * w_sw);
			dst.at<double>(j-1, i+1) = bound(update, 0, 1);
			//std::cout << "--2" << std::endl;
			//printM(dst);

			update = dst.at<double>(j, i+1) +  (error * w_s);
			dst.at<double>(j, i+1) = bound(update, 0, 1);
			//std::cout << "--3" << std::endl;
			//printM(dst);

			update = dst.at<double>(j+1, i+1) + (error * w_se);
			dst.at<double>(j+1, i+1) = bound(update, 0, 1);

			//std::cout << "--4" << std::endl;
			//printM(dst);
		}
	}
	return;
}
void printM(Mat tmp){
	for(int i = 0; i < tmp.cols; i++){
		for( int j =0; j < tmp.rows; j++){
			std::cout << (double)tmp.at<double>(j,i) << " ";
		}
		std::cout << std::endl; 
	}
}

double bound(double x, double from, double to){
	//return x;
	return x < from ? from : (x > to? to: x);
}

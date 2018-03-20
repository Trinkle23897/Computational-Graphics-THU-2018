#include <bits/stdc++.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
int main(int argc, char const *argv[])
{
	cv::Mat img(600,800,CV_8UC3,cv::Scalar(255,0,255));
	cv::namedWindow("main",CV_WINDOW_NORMAL|CV_GUI_EXPANDED);
	cv::imshow("main",img);
	cv::waitKey(0);
	return 0;
}
#include <iostream>
#include <fstream>
#include <string>
#include <yarp/sig/all.h>
#include <yarp/os/all.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Network.h>
#include <yarp/os/Thread.h>
#include <yarp/os/Stamp.h>
#include <iCub/iKin/iKinFwd.h>
#include <iCub/ctrl/math.h>
#include <cv.h>
#include <highgui.h>
#include "opencv2/calib3d/calib3d.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/highgui/highgui.hpp"


using namespace cv;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::iKin; 
using namespace std;
using namespace yarp::os; 
using namespace yarp::sig;
  
class stereoCalibThread : public Thread
{
private:

    ImageOf<PixelRgb> *imageL;
    ImageOf<PixelRgb> *imageR;
    IplImage * imgL;
    IplImage * imgR;

    int numOfPairs;

    Mat Kleft;
    Mat Kright;
    
    Mat DistL;
    Mat DistR;

    Mat R;
    Mat T;
    Mat Q;
    string inputLeftPortName;
    string inputRightPortName;
    string outNameRight;
    string outNameLeft;
    string camCalibFile;

    BufferedPort<ImageOf<PixelRgb> > imagePortInLeft;
    BufferedPort<ImageOf<PixelRgb> > imagePortInRight;
    BufferedPort<ImageOf<PixelRgb> > outPortRight;
    BufferedPort<ImageOf<PixelRgb> > outPortLeft;

    Port *commandPort;
    string dir;
    int startCalibration;
    int boardWidth;
    int boardHeight;
    float squareSize;
    char pathL[256];
    char pathR[256];
    void printMatrix(Mat &matrix);
    bool checkTS(double TSLeft, double TSRight, double th=0.020);
    void preparePath(const char * dir, char* pathL, char* pathR, int num);
    void saveStereoImage(const char * dir, IplImage* left, IplImage * right, int num);
    void monoCalibration(vector<string> imageList, int boardWidth, int boardHeight, Mat K, Mat Dist);
    void stereoCalibration(vector<string> imagelist, int boardWidth, int boardHeight,float sqsizee);
    void saveCalibration(string extrinsicFilePath, string intrinsicFilePath);
    void calcChessboardCorners(Size boardSize, float squareSize, vector<Point3f>& corners);
    bool updateIntrinsics( int width, int height, double fx, double fy,double cx, double cy, double k1, double k2, double p1, double p2, string groupname);
    bool updateExtrinsics(Mat Rot, Mat Tr, string groupname);

public:


    stereoCalibThread(ResourceFinder &rf, Port* commPort, const char *dir);
    void startCalib();
    bool threadInit();
    void threadRelease();
    void run(); 
    void onStop();

};



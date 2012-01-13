#include "stereoCalibThread.h"


stereoCalibThread::stereoCalibThread(ResourceFinder &rf, Port* commPort, const char *dir)
{

    string moduleName=rf.check("name", Value("stereoCalib"),"module name (string)").asString().c_str();

    this->inputLeftPortName         = "/"+moduleName;
    this->inputLeftPortName        +=rf.check("imgLeft",Value("/cam/left:i"),"Input image port (string)").asString();
   
    this->inputRightPortName        = "/"+moduleName;
    this->inputRightPortName       += rf.check("imgRight", Value("/cam/right:i"),"Input image port (string)").asString();

    this->outNameRight        = "/"+moduleName;
    this->outNameRight       += rf.check("outRight",Value("/cam/right:o"),"Output image port (string)").asString();

    this->outNameLeft        = "/"+moduleName;
    this->outNameLeft       +=rf.check("outLeft",Value("/cam/left:o"),"Output image port (string)").asString();

    this->boardWidth=  rf.check("boardWidth", Value(8)).asInt();
    this->boardHeight= rf.check("boardHeight", Value(6)).asInt();
    this->numOfPairs=rf.check("boardHeight", Value(30)).asInt();
    this->squareSize= (float)rf.check("boardSize", Value(0.09241)).asDouble();
    this->commandPort=commPort;
    this->dir=dir;
    this->startCalibration=0;

    camCalibFile=rf.getContextPath().c_str();
    camCalibFile=camCalibFile+"/icubEyes.ini";
    fprintf(stdout, "%s \n", dir);
}

bool stereoCalibThread::threadInit() 
{
     if (!imagePortInLeft.open(inputLeftPortName.c_str())) {
      cout  << ": unable to open port " << inputLeftPortName << endl;
      return false; 
   }

   if (!imagePortInRight.open(inputRightPortName.c_str())) {
      cout << ": unable to open port " << inputRightPortName << endl;
      return false;
   }

    if (!outPortLeft.open(outNameLeft.c_str())) {
      cout << ": unable to open port " << outNameLeft << endl;
      return false;
   }

    if (!outPortRight.open(outNameRight.c_str())) {
      cout << ": unable to open port " << outNameRight << endl;
      return false;
   }    

   return true;
}

void stereoCalibThread::run(){

    imageL=new ImageOf<PixelRgb>;
    imageR=new ImageOf<PixelRgb>;

    Stamp TSLeft;
    Stamp TSRight;

    bool initL=false;
    bool initR=false;

    int count=1;
    Size boardSize, imageSize;
    boardSize.width=this->boardWidth;
    boardSize.height=this->boardHeight;

    std::vector<string> imageListR, imageListL, imageListLR;

   while (!isStopping()) { 
        ImageOf<PixelRgb> *tmpL = imagePortInLeft.read(false);
        ImageOf<PixelRgb> *tmpR = imagePortInRight.read(false);

        if(tmpL!=NULL)
        {
            *imageL=*tmpL;
            imagePortInLeft.getEnvelope(TSLeft);
            initL=true;
        }
        if(tmpR!=NULL) 
        {
            *imageR=*tmpR;
            imagePortInRight.getEnvelope(TSRight);
            initR=true;
        }

        if(initL && initR && checkTS(TSLeft.getTime(),TSRight.getTime())){

            bool foundL=false;
            bool foundR=false;

            if(startCalibration>0) {

                string pathImg=dir;
                preparePath(pathImg.c_str(), pathL,pathR,count);
                string iml(pathL);
                string imr(pathR);
                imgL= (IplImage*) imageL->getIplImage();
                imgR= (IplImage*) imageR->getIplImage();
                Mat Left(imgL);
                Mat Right(imgR);


                std::vector<Point2f> pointbufL;
                std::vector<Point2f> pointbufR;
                foundL = findChessboardCorners( Left, boardSize, pointbufL, CV_CALIB_CB_ADAPTIVE_THRESH & CV_CALIB_CB_FAST_CHECK & CV_CALIB_CB_NORMALIZE_IMAGE);
                foundR = findChessboardCorners( Right, boardSize, pointbufR, CV_CALIB_CB_ADAPTIVE_THRESH & CV_CALIB_CB_FAST_CHECK & CV_CALIB_CB_NORMALIZE_IMAGE);
                
                if(foundL && foundR) {
                        cvCvtColor(imgL,imgL,CV_RGB2BGR);
                        cvCvtColor(imgR,imgR, CV_RGB2BGR);
                        saveStereoImage(pathImg.c_str(),imgL,imgR,count);

                        imageListR.push_back(imr);
                        imageListL.push_back(iml);
                        imageListLR.push_back(iml);
                        imageListLR.push_back(imr);
                        drawChessboardCorners(Left, boardSize, pointbufL, foundL);
                        drawChessboardCorners(Right, boardSize, pointbufR, foundR);
                        count++;
                }

                if(count>numOfPairs) {
                    fprintf(stdout," Running Left Camera Calibration... \n");
                    monoCalibration(imageListL,this->boardWidth,this->boardHeight,this->Kleft,this->DistL);

                    fprintf(stdout," Running Right Camera Calibration... \n");
                    monoCalibration(imageListR,this->boardWidth,this->boardHeight,this->Kright,this->DistR);

                    stereoCalibration(imageListLR, this->boardWidth,this->boardHeight,this->squareSize);

                    fprintf(stdout," Saving Calibration Results... \n");
                    saveCalibration(dir+"/../extrinsics",dir+"/../intrinsics");
                    updateIntrinsics(imgL->width,imgL->height,Kright.at<double>(0,0),Kright.at<double>(1,1),Kright.at<double>(0,2),Kright.at<double>(1,2),DistR.at<double>(0,0),DistR.at<double>(0,1),DistR.at<double>(0,2),DistR.at<double>(0,3),"CAMERA_CALIBRATION_RIGHT");
                    updateIntrinsics(imgL->width,imgL->height,Kleft.at<double>(0,0),Kleft.at<double>(1,1),Kleft.at<double>(0,2),Kleft.at<double>(1,2),DistL.at<double>(0,0),DistL.at<double>(0,1),DistL.at<double>(0,2),DistL.at<double>(0,3),"CAMERA_CALIBRATION_LEFT");

                    Mat Rot=Mat::eye(3,3,CV_64FC1);
                    Mat Tr=Mat::zeros(3,1,CV_64FC1);

                    updateExtrinsics(Rot,Tr,"ALIGN_KIN_LEFT");
                    updateExtrinsics(this->R,this->T,"ALIGN_KIN_RIGHT");

                    fprintf(stdout, "Calibration Results Saved. \n");

                    startCalibration=0;
                }


            }
                ImageOf<PixelRgb>& outimL=outPortLeft.prepare();
                outimL=*imageL;
                outPortLeft.write();

                ImageOf<PixelRgb>& outimR=outPortRight.prepare();
                outimR=*imageR;
                outPortRight.write();

                if(foundL && foundR && startCalibration==1)
                    Time::delay(2.0);
                initL=initR=false;
        }
   }

   delete imageL;
   delete imageR;
}

void stereoCalibThread::threadRelease() 
{
    imagePortInRight.close();
    imagePortInLeft.close();
    outPortLeft.close();
    outPortRight.close();
    commandPort->close();
}

void stereoCalibThread::onStop() {
    imagePortInRight.interrupt();
    imagePortInLeft.interrupt();
    outPortLeft.interrupt();
    outPortRight.interrupt();
    commandPort->interrupt();
    startCalibration=0;
}
void stereoCalibThread::startCalib() {
    startCalibration=1;
  }

void stereoCalibThread::printMatrix(Mat &matrix) {
    int row=matrix.rows;
    int col =matrix.cols;
        cout << endl;
    for(int i = 0; i < matrix.rows; i++)
    {
        const double* Mi = matrix.ptr<double>(i);
        for(int j = 0; j < matrix.cols; j++)
            cout << Mi[j] << " ";
        cout << endl;
    }
        cout << endl;
}


bool stereoCalibThread::checkTS(double TSLeft, double TSRight, double th) {
    double diff=fabs(TSLeft-TSRight);
    if(diff <th)
        return true;
    else return false;

}

void stereoCalibThread::preparePath(const char * dir, char* pathL, char* pathR, int count) {
    char num[5];
    sprintf(num, "%i", count); 


    strncpy(pathL,dir, strlen(dir));
    pathL[strlen(dir)]='\0';
    strcat(pathL,"left");
    strcat(pathL,num);
    strcat(pathL,".png");

    strncpy(pathR,dir, strlen(dir));
    pathR[strlen(dir)]='\0';
    strcat(pathR,"right");
    strcat(pathR,num);
    strcat(pathR,".png");

}


void stereoCalibThread::saveStereoImage(const char * dir, IplImage* left, IplImage * right, int num) {
    char pathL[256];
    char pathR[256];
    preparePath(dir, pathL,pathR,num);
    
    fprintf(stdout,"Saving images number %d \n",num);

    cvSaveImage(pathL,left);
    cvSaveImage(pathR,right);
}
bool stereoCalibThread::updateIntrinsics(int width, int height, double fx, double fy,double cx, double cy, double k1, double k2, double p1, double p2, string groupname){

    std::vector<string> lines;

    bool append = false;

    ifstream in;
    in.open(camCalibFile.c_str()); //camCalibFile.c_str());
    
    if(in.is_open()){
        // file exists
        string line;
        bool sectionFound = false;
        bool sectionClosed = false;

        // process lines
        while(std::getline(in, line)){
            // check if we left calibration section
            if (sectionFound == true && line.find("[", 0) != string::npos)
                sectionClosed = true;   // also valid if no groupname specified
            // check if we enter calibration section
            if (line.find(string("[") + groupname + string("]"), 0) != string::npos)
                sectionFound = true;
            // if no groupname specified
            if (groupname == "")
                sectionFound = true;
            // if we are in calibration section (or no section/group specified)
            if (sectionFound == true && sectionClosed == false){
                // replace w line
                if (line.find("w",0) ==0){
                    stringstream ss;
                    ss << width;
                    line = "w " + string(ss.str());
                }
                // replace h line
                if (line.find("h",0) ==0){
                    stringstream ss;
                    ss << height;
                    line = "h " + string(ss.str());
                }
                // replace fx line
                if (line.find("fx",0) != string::npos){
                    stringstream ss;
                    ss << fx;
                    line = "fx " + string(ss.str());
                }
                // replace fy line
                if (line.find("fy",0) != string::npos){
                    stringstream ss;
                    ss << fy;
                    line = "fy " + string(ss.str());
                }
                // replace cx line
                if (line.find("cx",0) != string::npos){
                    stringstream ss;
                    ss << cx;
                    line = "cx " + string(ss.str());
                }
                // replace cy line
                if (line.find("cy",0) != string::npos){
                    stringstream ss;
                    ss << cy;
                    line = "cy " + string(ss.str());
                }
                // replace k1 line
                if (line.find("k1",0) != string::npos){
                    stringstream ss;
                    ss << k1;
                    line = "k1 " + string(ss.str());
                }
                // replace k2 line
                if (line.find("k2",0) != string::npos){
                    stringstream ss;
                    ss << k2;
                    line = "k2 " + string(ss.str());
                }
                // replace p1 line
                if (line.find("p1",0) != string::npos){
                    stringstream ss;
                    ss << p1;
                    line = "p1 " + string(ss.str());
                }
                // replace p2 line
                if (line.find("p2",0) != string::npos){
                    stringstream ss;
                    ss << p2;
                    line = "p2 " + string(ss.str());
                }
       
            }
            // buffer line
            lines.push_back(line);
        }
        
        in.close();

        // rewrite file
        if (!sectionFound){
            append = true;
            cout << "Camera calibration parameter section " + string("[") + groupname + string("]") + " not found in file " << camCalibFile << ". Adding group..." << endl;
        }
        else{
            // rewrite file
            ofstream out;
            out.open(camCalibFile.c_str(), ios::trunc);
            if (out.is_open()){
                for (int i = 0; i < (int)lines.size(); i++)
                    out << lines[i] << endl;
                out.close();
            }
            else
                return false;
        }
        
    }
    else{
        append = true;
    }

    if (append){
        // file doesn't exist or section is appended 
        ofstream out;
        out.open(camCalibFile.c_str(), ios::app);
        if (out.is_open()){
            out << string("[") + groupname + string("]") << endl;
            out << endl;
            out << "w  " << width << endl;
            out << "h  " << height << endl;
            out << "fx " << fx << endl;
            out << "fy " << fy << endl;
            out << "cx " << cx << endl;
            out << "cy " << cy << endl;
            out << "k1 " << k1 << endl;
            out << "k2 " << k2 << endl;
            out << "p1 " << p1 << endl;
            out << "p2 " << p2 << endl;
            out << endl;
            out.close();
        }
        else
            return false;
    }

    return true;
}

void stereoCalibThread::monoCalibration(vector<string> imageList, int boardWidth, int boardHeight, Mat K, Mat Dist)
{
    vector<vector<Point2f> > imagePoints;
    Size boardSize, imageSize;
    boardSize.width=boardWidth;
    boardSize.height=boardHeight;
    int flags=0;
    int i;

    float squareSize = 1.f, aspectRatio = 1.f;

      Mat view, viewGray;

    for(i = 0; i<(int)imageList.size();i++)
    {

         view = cv::imread(imageList[i], 1);
         imageSize = view.size();
         vector<Point2f> pointbuf;
         cvtColor(view, viewGray, CV_BGR2GRAY); 

         bool found = findChessboardCorners( view, boardSize, pointbuf,
                                            CV_CALIB_CB_ADAPTIVE_THRESH & CV_CALIB_CB_FAST_CHECK & CV_CALIB_CB_NORMALIZE_IMAGE);

         if(found) 
         {
            drawChessboardCorners( view, boardSize, Mat(pointbuf), found );
            imagePoints.push_back(pointbuf);
         }

    }
    std::vector<Mat> rvecs, tvecs;
    std::vector<float> reprojErrs;
    double totalAvgErr = 0;

    K = Mat::eye(3, 3, CV_64F);
    if( flags & CV_CALIB_FIX_ASPECT_RATIO )
        K.at<double>(0,0) = aspectRatio;
    
    Dist = Mat::zeros(4, 1, CV_64F);
    
    std::vector<std::vector<Point3f> > objectPoints(1);
    calcChessboardCorners(boardSize, squareSize, objectPoints[0]);

    objectPoints.resize(imagePoints.size(),objectPoints[0]);
    
    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, K,
                    Dist, rvecs, tvecs,CV_CALIB_FIX_K3);
    printf("RMS error reported by calibrateCamera: %g\n", rms);
}


void stereoCalibThread::stereoCalibration(vector<string> imagelist, int boardWidth, int boardHeight,float sqsize)
{
    Size boardSize;
    boardSize.width=boardWidth;
    boardSize.height=boardHeight;
    if( imagelist.size() % 2 != 0 )
    {
        cout << "Error: the image list contains odd (non-even) number of elements\n";
        return;
    }
    
    const int maxScale = 2;
    // ARRAY AND VECTOR STORAGE:
    
    std::vector<std::vector<Point2f> > imagePoints[2];
    std::vector<std::vector<Point3f> > objectPoints;
    Size imageSize;
    
    int i, j, k, nimages = (int)imagelist.size()/2;
    
    imagePoints[0].resize(nimages);
    imagePoints[1].resize(nimages);
    std::vector<string> goodImageList;
    
    for( i = j = 0; i < nimages; i++ )
    {
        for( k = 0; k < 2; k++ )
        {
            const string& filename = imagelist[i*2+k];
            Mat img = cv::imread(filename, 0);
            if(img.empty())
                break;
            if( imageSize == Size() )
                imageSize = img.size();
            else if( img.size() != imageSize )
            {
                cout << "The image " << filename << " has the size different from the first image size. Skipping the pair\n";
                break;
            }
            bool found = false;
            std::vector<Point2f>& corners = imagePoints[k][j];
            for( int scale = 1; scale <= maxScale; scale++ )
            {
                Mat timg;
                if( scale == 1 )
                    timg = img;
                else
                    resize(img, timg, Size(), scale, scale);
                found = findChessboardCorners(timg, boardSize, corners, 
                    CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_NORMALIZE_IMAGE);
                if( found )
                {
                    if( scale > 1 )
                    {
                        Mat cornersMat(corners);
                        cornersMat *= 1./scale;
                    }
                    break;
                }
            }
            if( !found )
                break;
            }
        if( k == 2 )
        {
            goodImageList.push_back(imagelist[i*2]);
            goodImageList.push_back(imagelist[i*2+1]);
            j++;
        }
    }
    fprintf(stdout,"%i pairs have been successfully detected.\n",j);
    nimages = j;
    if( nimages < 2 )
    {
        fprintf(stdout,"Error: too few pairs detected");
        return;
    }
    
    imagePoints[0].resize(nimages);
    imagePoints[1].resize(nimages);
    objectPoints.resize(nimages);
    
    for( i = 0; i < nimages; i++ )
    {
        for( j = 0; j < boardSize.height; j++ )
            for( k = 0; k < boardSize.width; k++ )
                objectPoints[i].push_back(Point3f(j*squareSize, k*squareSize, 0));
    }
    
    fprintf(stdout,"Running stereo calibration ...\n");
    
    Mat cameraMatrix[2], distCoeffs[2];
    Mat E, F;
    
    if(this->Kleft.empty() || this->Kright.empty())
    {
        double rms = stereoCalibrate(objectPoints, imagePoints[0], imagePoints[1],
                        this->Kleft, this->DistL,
                        this->Kright, this->DistR,
                        imageSize, this->R, this->T, E, F,
                        TermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 100, 1e-5),
                        CV_CALIB_FIX_ASPECT_RATIO +
                        CV_CALIB_ZERO_TANGENT_DIST +
                        CV_CALIB_SAME_FOCAL_LENGTH +
                        CV_CALIB_RATIONAL_MODEL +
                        CV_CALIB_FIX_K3 + CV_CALIB_FIX_K4 + CV_CALIB_FIX_K5);
        fprintf(stdout,"done with RMS error= %f\n",rms);
    } else
    {
        double rms = stereoCalibrate(objectPoints, imagePoints[0], imagePoints[1],
                this->Kleft, this->DistL,
                this->Kright, this->DistR,
                imageSize, this->R, this->T, E, F,
                TermCriteria(CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 100, 1e-5),CV_CALIB_FIX_INTRINSIC);
        fprintf(stdout,"done with RMS error= %f\n",rms);
    }
// CALIBRATION QUALITY CHECK
    cameraMatrix[0] = this->Kleft;
    cameraMatrix[1] = this->Kright;
    distCoeffs[0]=this->DistL;
    distCoeffs[1]=this->DistR;
    Mat R, T;
    T=this->T;
    R=this->R;
    double err = 0;
    int npoints = 0;
    std::vector<Vec3f> lines[2];
    for( i = 0; i < nimages; i++ )
    {
        int npt = (int)imagePoints[0][i].size();
        Mat imgpt[2];
        for( k = 0; k < 2; k++ )
        {
            imgpt[k] = Mat(imagePoints[k][i]);
            undistortPoints(imgpt[k], imgpt[k], cameraMatrix[k], distCoeffs[k], Mat(), cameraMatrix[k]);
            computeCorrespondEpilines(imgpt[k], k+1, F, lines[k]);
        }
        for( j = 0; j < npt; j++ )
        {
            double errij = fabs(imagePoints[0][i][j].x*lines[1][j][0] +
                                imagePoints[0][i][j].y*lines[1][j][1] + lines[1][j][2]) +
                           fabs(imagePoints[1][i][j].x*lines[0][j][0] +
                                imagePoints[1][i][j].y*lines[0][j][1] + lines[0][j][2]);
            err += errij;
        }
        npoints += npt;
    }
    Mat R1,R2,P1,P2;
    Rect roi1, roi2;
    stereoRectify( this->Kleft, this->DistL, this->Kright, this->DistR, imageSize, this->R, this->T, R1, R2, P1, P2, this->Q, -1, imageSize, &roi1, &roi2 );
    fprintf(stdout,"average reprojection err = %f\n",err/npoints);
}


void stereoCalibThread::saveCalibration(string extrinsicFilePath, string intrinsicFilePath){

    if( Kleft.empty() || Kright.empty() || DistL.empty() || DistR.empty() || R.empty() || T.empty()) {
            cout << "Error: cameras are not calibrated! Run the calibration or set intrinsic and extrinsic parameters \n";
            return;
    }

    FileStorage fs(intrinsicFilePath+".yml", CV_STORAGE_WRITE);
    if( fs.isOpened() )
    {
        fs << "M1" << Kleft << "D1" << DistL << "M2" << Kright << "D2" << DistR;
        fs.release();
    }
    else
        cout << "Error: can not save the intrinsic parameters\n";

    fs.open(extrinsicFilePath+".yml", CV_STORAGE_WRITE);
    if( fs.isOpened() )
    {
        fs << "R" << R << "T" << T <<"Q" << Q;
        fs.release();
    }
    else
        cout << "Error: can not save the intrinsic parameters\n";

}

void stereoCalibThread::calcChessboardCorners(Size boardSize, float squareSize, vector<Point3f>& corners)
{
    corners.resize(0);
    
    for( int i = 0; i < boardSize.height; i++ )
        for( int j = 0; j < boardSize.width; j++ )
            corners.push_back(Point3f(float(j*squareSize),
                                      float(i*squareSize), 0));
}
bool stereoCalibThread::updateExtrinsics(Mat Rot, Mat Tr, string groupname)
{

    std::vector<string> lines;

    bool append = false;

    ifstream in;
    in.open(camCalibFile.c_str()); //camCalibFile.c_str());
    
    if(in.is_open()){
        // file exists
        string line;
        bool sectionFound = false;
        bool sectionClosed = false;

        // process lines
        while(std::getline(in, line)){
            // check if we left calibration section
            if (sectionFound == true && line.find("[", 0) != string::npos)
                sectionClosed = true;   // also valid if no groupname specified
            // check if we enter calibration section
            if (line.find(string("[") + groupname + string("]"), 0) != string::npos)
                sectionFound = true;
            // if no groupname specified
            if (groupname == "")
                sectionFound = true;
            // if we are in calibration section (or no section/group specified)
            if (sectionFound == true && sectionClosed == false){
                // replace w line
                if (line.find("HN",0) != string::npos){
                    stringstream ss;
                    ss << " (" << Rot.at<double>(0,0) << " " << Rot.at<double>(0,1) <<" " << Rot.at<double>(0,2) <<" " << Tr.at<double>(0,0) << " " << Rot.at<double>(1,0) <<" " << Rot.at<double>(1,1) <<" " << Rot.at<double>(1,2) <<" " << Tr.at<double>(1,0)<< " " << Rot.at<double>(2,0) <<" " << Rot.at<double>(2,1) <<" " << Rot.at<double>(2,2) <<" " << Tr.at<double>(2,0)<< " " << 0 << " " << 0 << " " << 0 << " " << 1 << ")";
                    line = "HN" + string(ss.str());
                }
       
            }
            // buffer line
            lines.push_back(line);
        }
        
        in.close();

        // rewrite file
        if (!sectionFound){
            append = true;
            cout << "Camera calibration parameter section " + string("[") + groupname + string("]") + " not found in file " << camCalibFile << ". Adding group..." << endl;
        }
        else{
            // rewrite file
            ofstream out;
            out.open(camCalibFile.c_str(), ios::trunc);
            if (out.is_open()){
                for (int i = 0; i < (int)lines.size(); i++)
                    out << lines[i] << endl;
                out.close();
            }
            else
                return false;
        }
        
    }
    else{
        append = true;
    }

    if (append){
        // file doesn't exist or section is appended 
        ofstream out;
        out.open(camCalibFile.c_str(), ios::app);
        if (out.is_open()){
            out << endl;
            out << string("[") + groupname + string("]") << endl;
            out << "HN (" << Rot.at<double>(0,0) << " " << Rot.at<double>(0,1) <<" " << Rot.at<double>(0,2) <<" " << Tr.at<double>(0,0)<< " " << Rot.at<double>(1,0) <<" " << Rot.at<double>(1,1) <<" " << Rot.at<double>(1,2) <<" " << Tr.at<double>(1,0)<< " " << Rot.at<double>(2,0) <<" " << Rot.at<double>(2,1) <<" " << Rot.at<double>(2,2) <<" " << Tr.at<double>(2,0)<< " " << 0 << " " << 0 << " " << 0 << " " << 1 << ")";
            out << endl;
            out.close();
        }
        else
            return false;
    }

    return true;
}

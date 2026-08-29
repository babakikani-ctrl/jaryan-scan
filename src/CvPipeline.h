#pragma once
#include "ofMain.h"
#include "ofxOpenCv.h"
#include <opencv2/imgproc.hpp>
// DNN person-detector is optional: define JARYAN_NO_DNN to build without it (falls back to
// blob tracking) — used when a platform's ofxOpenCv lacks the opencv_dnn module.
#ifndef JARYAN_NO_DNN
#include <opencv2/dnn.hpp>
#endif

// Webcam -> foreground -> contours + tracked subjects + motion energy.
// Fake-cam mode (JARYAN_FAKECAM) synthesizes moving figures for headless testing.

struct CvTrack {
    int id = -1;
    ofRectangle bbox;      // camera pixels
    glm::vec2 c{0,0};      // centroid, camera pixels
    glm::vec2 vel{0,0};    // camera px / s
    float speed = 0, area = 0, age = 0, dwell = 0;
    int lastSeen = 0;
};

class CvPipeline {
public:
    void setup(bool fakeCam, int w = 640, int h = 480);
    void update(float dt);

    ofTexture& cameraTexture() { return camImg.getTexture(); }
    ofTexture& silhouetteTexture() { return silImg.getTexture(); }
    ofTexture& subjectTexture()    { return subjImg.getTexture(); }     // silhouette ∩ DNN person boxes (precise)
    ofTexture& motionTexture()     { return motionImg.getTexture(); }   // frame-to-frame motion (hands)
    ofPixels&  silhouettePixels()  { return silImg.getPixels(); }
    ofPixels&  subjectPixels()     { return subjImg.getPixels(); }
    ofPixels&  motionPixels()      { return motionImg.getPixels(); }
    ofPixels&  cameraPixels()      { return camImg.getPixels(); }
    std::vector<CvTrack>     tracks;
    std::vector<ofPolyline>  contours;   // camera px
    float motionEnergy = 0;
    bool  haveFrame = false;
    bool  fake = false;
    int   camW = 640, camH = 480;

    bool mirrorCam = true;                                // selfie mirror (source flipped once); 'm' toggles
    void toggleMirror() { mirrorCam = !mirrorCam; }
    bool detectorLoaded() const { return haveNet; }      // diagnostics
    int  numDetections()  const { return (int)personBoxes.size(); }
    void captureBg() { bgCaptured = false; learn = 0; }
    void nudgeThreshold(int d) { threshold = ofClamp(threshold + d, 4, 120); }
    int  getThreshold() const { return threshold; }
    bool usingDNN = false;

private:
    // DNN person detector (MobileNet-SSD) — detects PEOPLE only, not any moving thing
#ifndef JARYAN_NO_DNN
    cv::dnn::Net net;
    void runDetector();
#endif
    bool  haveNet = false;
    int   frameCount = 0, detEvery = 2;
    float detConf = 0.45f;
    std::vector<ofRectangle> personBoxes;   // camera space
    std::vector<unsigned char> personMask;  // camW*camH, 1 inside a detected person

    ofVideoGrabber grabber;
    ofImage fakeImg, camImg, silImg, subjImg, motionImg;
    ofxCvColorImage color;
    ofxCvGrayscaleImage gray, bg, diff, prevGray, motionCv;
    bool havePrev = false;
    ofxCvContourFinder contour;
    bool bgCaptured = false;
    int  learn = 0, threshold = 30, nextId = 0;
    float ft = 0;
    void makeFake();
};

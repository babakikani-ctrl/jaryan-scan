#include "CvPipeline.h"
#include <algorithm>

void CvPipeline::setup(bool fakeCam, int w, int h) {
    fake = fakeCam; camW = w; camH = h;
    color.allocate(w, h);
    gray.allocate(w, h);
    bg.allocate(w, h);
    diff.allocate(w, h);
    camImg.allocate(w, h, OF_IMAGE_COLOR);
    silImg.allocate(w, h, OF_IMAGE_GRAYSCALE);
    silImg.getPixels().set(0);
    silImg.update();
    subjImg.allocate(w, h, OF_IMAGE_GRAYSCALE);
    subjImg.getPixels().set(0);
    subjImg.update();
    prevGray.allocate(w, h); motionCv.allocate(w, h);
    motionImg.allocate(w, h, OF_IMAGE_GRAYSCALE);
    motionImg.getPixels().set(0); motionImg.update();

    // ---- pick the video source: RTSP / IP-camera (bin/data/source.txt or env JARYAN_SOURCE) else webcam ----
    std::string src;
    if (const char* e = getenv("JARYAN_SOURCE")) src = e;
    if (src.empty()) {
        ofFile sf(ofToDataPath("source.txt", true));
        if (sf.exists()) src = ofTrim(ofBufferFromFile(sf.getAbsolutePath()).getText());
    }
#ifdef JARYAN_KINECT
    if (!fake && (src == "kinect" || src == "KINECT")) {
        useKinect = true; sourceLabel = "KINECT";
        kinect.open();
        kinect.initColorSource();                 // COLOR only — no depth
        ofLogNotice("CvPipeline") << "Kinect v2 color source opened";
    }
#endif
#ifdef JARYAN_RTSP
    if (!fake && !src.empty() && (src.rfind("rtsp://", 0) == 0 || src.rfind("http", 0) == 0)) {
        useRtsp = true; rtspUrl = src; sourceLabel = "RTSP";
        // reliable RTSP over LAN + LOW LATENCY (no buffering, minimal delay) for interactivity
        _putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp|fflags;nobuffer|flags;low_delay|max_delay;120000|reorder_queue_size;0");
        rtspRun = true;
        rtspThread = std::thread(&CvPipeline::rtspLoop, this);
        ofLogNotice("CvPipeline") << "RTSP source: " << rtspUrl;
    }
#endif
#ifdef JARYAN_NDI
    if (!fake && (src == "ndi" || src == "NDI" || src.rfind("ndi:", 0) == 0)) {
        ndiWanted = true; sourceLabel = "NDI";                 // NDI (e.g. TouchDesigner) is the primary source
        if (src.rfind("ndi:", 0) == 0) ndiName = src.substr(4);
        ofLogNotice("CvPipeline") << "NDI source" << (ndiName.empty() ? std::string(" (first sender)") : (": " + ndiName));
    }
#endif
    if (fake) {
        fakeImg.allocate(w, h, OF_IMAGE_COLOR); sourceLabel = "FAKE";
    }
#ifdef JARYAN_KINECT
    else if (useKinect) { /* frames come from the Kinect color source */ }
#endif
#ifdef JARYAN_RTSP
    else if (useRtsp) { /* frames arrive on the RTSP thread */ }
#endif
    else if (ndiWanted) { /* frames arrive from the NDI receiver (created lazily on first update) */ }
    else {
        grabber.setDeviceID(0);
        grabber.setDesiredFrameRate(30);
        grabber.setup(w, h);
        sourceLabel = "CAM";
    }
    baseLabel = sourceLabel;                  // remember the base source label (NDI toggle restores it)

    // load the person detector (MobileNet-SSD, VOC 'person' class)
    personMask.assign(camW * camH, 0);
#ifndef JARYAN_NO_DNN
    std::string proto = ofToDataPath("model/MobileNetSSD_deploy.prototxt", true);
    std::string model = ofToDataPath("model/MobileNetSSD_deploy.caffemodel", true);
    try {
        net = cv::dnn::readNetFromCaffe(proto, model);
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        haveNet = !net.empty();
    } catch (const std::exception& e) {
        ofLogError("CvPipeline") << "person detector load failed: " << e.what();
        haveNet = false;
    }
    usingDNN = haveNet && !fake;        // fake-cam has no real people -> use blob tracking for testing
#else
    usingDNN = false;                   // built without opencv_dnn -> blob tracking
#endif
    ofLogNotice("CvPipeline") << "person detector " << (haveNet ? "LOADED" : "NOT loaded (fallback: blob tracking)");
}

CvPipeline::~CvPipeline() {
#ifdef JARYAN_RTSP
    rtspRun = false;
    if (rtspThread.joinable()) rtspThread.join();
#endif
#ifdef JARYAN_KINECT
    if (useKinect) kinect.close();
#endif
#ifdef JARYAN_NDI
    if (ndiInit) ndiRecv.ReleaseReceiver();
#endif
}

void CvPipeline::reconnectSource() {
#ifdef JARYAN_RTSP
    if (useRtsp) rtspReopen = true;    // the RTSP thread will release + re-open the stream
#endif
#ifdef JARYAN_NDI
    if (ndiWanted && ndiInit) { ndiRecv.ReleaseReceiver(); ndiInit = false; }   // re-find the NDI sender
#endif
}

void CvPipeline::setNdiEnabled(bool on) {
#ifdef JARYAN_NDI
    if (on == ndiWanted) return;
    ndiWanted = on;
    if (on) { sourceLabel = "NDI"; }
    else    { sourceLabel = baseLabel; ndiHasFrame = false; }
#else
    (void)on;                          // built without NDI -> the panel toggle is inert
#endif
}
bool CvPipeline::ndiEnabled()   const { return ndiWanted; }
bool CvPipeline::ndiReceiving() const { return ndiWanted && ndiHasFrame; }

#ifdef JARYAN_RTSP
void CvPipeline::rtspLoop() {
    while (rtspRun.load()) {
        if (rtspReopen.exchange(false) && vcap.isOpened()) { vcap.release(); rtspConnected = false; }
        if (!vcap.isOpened()) {
            vcap.open(rtspUrl, cv::CAP_FFMPEG);
            if (!vcap.isOpened()) { rtspConnected = false; std::this_thread::sleep_for(std::chrono::milliseconds(800)); continue; }
            vcap.set(cv::CAP_PROP_BUFFERSIZE, 1);      // keep only the newest frame -> minimal latency
            rtspConnected = true;
        }
        cv::Mat f;
        bool ok = false;
        try { ok = vcap.read(f); } catch (...) { ok = false; }
        if (ok && !f.empty()) {
            std::lock_guard<std::mutex> lk(rtspMx);
            f.copyTo(rtspFrame);
            rtspNew = true;
        } else {                                   // stream dropped -> reconnect
            rtspConnected = false;
            vcap.release();
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }
    if (vcap.isOpened()) vcap.release();
}
#endif

#ifndef JARYAN_NO_DNN
void CvPipeline::runDetector() {
    if (!haveNet) return;
    ofPixels& px = camImg.getPixels();
    if (px.getWidth() <= 0) return;
    cv::Mat rgb(camH, camW, CV_8UC3, px.getData());
    cv::Mat bgr; cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    cv::Mat blob = cv::dnn::blobFromImage(bgr, 0.007843f, cv::Size(300, 300), cv::Scalar(127.5, 127.5, 127.5), false, false);
    net.setInput(blob);
    cv::Mat out = net.forward();
    cv::Mat det(out.size[2], out.size[3], CV_32F, out.ptr<float>());

    personBoxes.clear();
    for (int i = 0; i < det.rows; i++) {
        float conf = det.at<float>(i, 2);
        int   cls  = (int)det.at<float>(i, 1);
        if (cls == 15 && conf > detConf) {                       // 15 = person
            float x1 = det.at<float>(i, 3) * camW, y1 = det.at<float>(i, 4) * camH;
            float x2 = det.at<float>(i, 5) * camW, y2 = det.at<float>(i, 6) * camH;
            x1 = ofClamp(x1, 0, camW); x2 = ofClamp(x2, 0, camW);
            y1 = ofClamp(y1, 0, camH); y2 = ofClamp(y2, 0, camH);
            if (x2 - x1 > 10 && y2 - y1 > 10) personBoxes.push_back(ofRectangle(x1, y1, x2 - x1, y2 - y1));
        }
    }
    personMask.assign(camW * camH, 0);
    for (auto& b : personBoxes) {
        int bx0 = (int)ofClamp(b.x, 0, camW - 1), bx1 = (int)ofClamp(b.x + b.width, 0, camW);
        int by0 = (int)ofClamp(b.y, 0, camH - 1), by1 = (int)ofClamp(b.y + b.height, 0, camH);
        for (int y = by0; y < by1; y++) { int row = y * camW; for (int x = bx0; x < bx1; x++) personMask[row + x] = 1; }
    }
}
#endif  // JARYAN_NO_DNN

void CvPipeline::makeFake() {
    ft += 1.0f / 60.0f;
    ofPixels& px = fakeImg.getPixels();
    for (int y = 0; y < camH; y++)
        for (int x = 0; x < camW; x++) {
            int i = (y * camW + x) * 3;
            unsigned char v = 10;                    // static clean background
            px[i] = v; px[i + 1] = v; px[i + 2] = v;
        }
    auto blob = [&](float cx, float cy, float rw, float rh) {
        for (int y = (int)(cy - rh); y <= cy + rh; y++)
            for (int x = (int)(cx - rw); x <= cx + rw; x++) {
                if (x < 0 || y < 0 || x >= camW || y >= camH) continue;
                float dx = (x - cx) / rw, dy = (y - cy) / rh;
                float d = dx * dx + dy * dy;
                if (d > 1) continue;
                int i = (y * camW + x) * 3;
                unsigned char v = (unsigned char)ofClamp(150 * (1 - sqrt(d)) + 70, 0, 255);
                px[i] = std::max(px[i], v); px[i + 1] = std::max(px[i + 1], v); px[i + 2] = std::max(px[i + 2], v);
            }
    };
    blob(camW * 0.40f + camW * 0.16f * sin(ft * 0.8f),        camH * 0.55f + camH * 0.12f * cos(ft * 0.6f),       camW * 0.06f, camH * 0.15f);
    blob(camW * 0.66f + camW * 0.10f * sin(ft * 0.5f + 2.0f), camH * 0.50f + camH * 0.14f * cos(ft * 0.4f + 1.0f), camW * 0.05f, camH * 0.13f);
    fakeImg.update();
}

void CvPipeline::update(float dt) {
    bool isNew = false;
    ndiHasFrame = false;
#ifdef JARYAN_NDI
    if (ndiWanted) {                                       // NDI (e.g. TouchDesigner) selected as the source
        if (!ndiInit) {                                    // ReceiveImage() finds/creates the receiver internally
            if (!ndiName.empty()) ndiRecv.SetSenderName(ndiName);   // optional: bind to a specific named sender
            ndiInit = true;
        }
        if (ndiRecv.ReceiveImage(ndiPix)) {
            ofPixels p = ndiPix;
            if (p.getNumChannels() >= 3) p.setImageType(OF_IMAGE_COLOR);   // NDI RGBA/UYVY -> RGB
            if ((int)p.getWidth() != camW || (int)p.getHeight() != camH) p.resize(camW, camH);
            if (mirrorCam) p.mirror(false, true);
            color.setFromPixels(p);
            camImg.setFromPixels(p);
            isNew = true; ndiHasFrame = true; sourceLabel = "NDI";
        }
    }
#endif
    if (!isNew) {                                          // no NDI frame -> fall back to the configured source
#ifdef JARYAN_KINECT
    if (useKinect) {
        kinect.update();
        auto cs = kinect.getColorSource();
        if (cs && cs->isFrameNew()) {
            ofPixels p = cs->getPixels();                 // Kinect v2 colour (1920x1080)
            if (p.getWidth() > 0) {
                if (p.getNumChannels() >= 3) p.setImageType(OF_IMAGE_COLOR);   // drop alpha -> RGB
                if ((int)p.getWidth() != camW || (int)p.getHeight() != camH) p.resize(camW, camH);
                if (mirrorCam) p.mirror(false, true);
                color.setFromPixels(p);
                camImg.setFromPixels(p);
                isNew = true;
            }
        }
    } else
#endif
#ifdef JARYAN_RTSP
    if (useRtsp) {
        cv::Mat f;
        { std::lock_guard<std::mutex> lk(rtspMx); if (rtspNew.load()) { rtspFrame.copyTo(f); rtspNew = false; } }
        if (!f.empty()) {
            cv::Mat rz, rgb;
            if (f.cols != camW || f.rows != camH) cv::resize(f, rz, cv::Size(camW, camH)); else rz = f;
            cv::cvtColor(rz, rgb, cv::COLOR_BGR2RGB);
            ofPixels px; px.setFromPixels(rgb.data, camW, camH, OF_PIXELS_RGB);
            if (mirrorCam) px.mirror(false, true);
            color.setFromPixels(px);
            camImg.setFromPixels(px);
            isNew = true;
        }
    } else
#endif
    if (fake) {
        makeFake();
        ofPixels src = fakeImg.getPixels();
        if (mirrorCam) src.mirror(false, true);            // selfie mirror once -> all downstream is consistent
        color.setFromPixels(src);
        camImg.setFromPixels(src);
        isNew = true;
    } else {
        grabber.update();
        if (grabber.isFrameNew()) {
            ofPixels src = grabber.getPixels();
            if (mirrorCam) src.mirror(false, true);
            color.setFromPixels(src);
            camImg.setFromPixels(src);
            isNew = true;
        }
    }
    }   // end fallback-source block (skipped when an NDI frame arrived)
    if (!isNew) return;
    haveFrame = true;

    gray = color;

    if (bgCaptured) {   // global brightness normalization — cancels webcam auto-exposure /
                        // gain drift so the whole frame never reads as a white foreground mass
        ofPixels& gp = gray.getPixels();
        ofPixels& bp = bg.getPixels();
        size_t n = std::min(gp.size(), bp.size());
        long sg = 0, sb = 0;
        for (size_t i = 0; i < n; i++) { sg += gp[i]; sb += bp[i]; }
        int delta = (int)((sb - sg) / (long)std::max<size_t>(1, n));
        if (delta != 0) {
            for (size_t i = 0; i < n; i++) { int v = (int)gp[i] + delta; gp[i] = (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); }
            gray.setFromPixels(gp);
        }
    }

    if (havePrev) {                                  // frame-to-frame motion (hand movement)
        motionCv.absDiff(prevGray, gray);
        motionCv.blurGaussian(11);
        motionImg.setFromPixels(motionCv.getPixels());
        motionImg.update();
    }
    prevGray = gray; havePrev = true;

    if (!bgCaptured) {                     // learn a clean background plate
        if (learn > 10) { bg = gray; bgCaptured = true; }
        learn++;
        return;
    }

    frameCount++;
#ifndef JARYAN_NO_DNN
    if (usingDNN && (frameCount % detEvery == 0)) runDetector();   // detect PEOPLE (not any motion)
#endif

    {   // REAL-TIME selective running-average background — never needs a manual 'b'.
        // Background-looking pixels adapt fast (absorb lighting / webcam auto-exposure
        // drift so no white mass builds up); genuine foreground adapts slowly so a
        // moving person stays detected. Fully self-calibrating for a days-long install.
        ofPixels& bp = bg.getPixels();
        ofPixels& gp = gray.getPixels();
        size_t n = std::min(bp.size(), gp.size());
        bool prot = usingDNN && personMask.size() == n;
        for (size_t i = 0; i < n; i++) {
            if (prot && personMask[i]) continue;                 // don't absorb detected people -> silhouette persists
            int d = gp[i] > bp[i] ? gp[i] - bp[i] : bp[i] - gp[i];
            float a = (d < threshold) ? 0.060f : 0.016f;
            bp[i] = (unsigned char)(bp[i] + a * (gp[i] - bp[i]));
        }
        bg.setFromPixels(bp);
    }

    diff.absDiff(bg, gray);
    diff.threshold(threshold);
    diff.erode();                 // clean sensor speckle, solidify the silhouette
    diff.dilate();
    diff.dilate();

    silImg.setFromPixels(diff.getPixels());   // silhouette for the LED field
    silImg.update();

    {   // SUBJECT mask = foreground restricted to DETECTED people -> ignores background clutter
        ofPixels sp = diff.getPixels();
        if (usingDNN && personMask.size() == sp.size()) {
            unsigned char* s = sp.getData();
            size_t n = sp.size();
            for (size_t i = 0; i < n; i++) if (!personMask[i]) s[i] = 0;
        }
        subjImg.setFromPixels(sp);            // == silhouette when no DNN (fake-cam / fallback)
        subjImg.update();
    }

    int minA = (int)((camW * camH) * 0.004f);
    int maxA = (int)((camW * camH) * 0.70f);
    contour.findContours(diff, minA, maxA, 10, false, true);

    contours.clear();
    for (auto& b : contour.blobs) {
        ofPolyline pl;
        for (auto& p : b.pts) pl.addVertex(p.x, p.y);
        pl.close();
        contours.push_back(pl);
    }

    // greedy nearest-neighbour tracking in camera space
    struct Det { glm::vec2 c; ofRectangle r; float area; };
    std::vector<Det> dets;
    if (usingDNN) {                                              // tracks come from PERSON detections
        for (auto& b : personBoxes)
            dets.push_back({ glm::vec2(b.x + b.width * 0.5f, b.y + b.height * 0.5f), b, b.getArea() });
    } else {
        for (auto& b : contour.blobs)
            dets.push_back({ glm::vec2(b.centroid.x, b.centroid.y), b.boundingRect, (float)b.area });
    }

    std::vector<bool> used(dets.size(), false);
    for (auto& t : tracks) {
        int best = -1; float bd = 1e9f;
        for (size_t i = 0; i < dets.size(); i++) {
            if (used[i]) continue;
            float d = glm::distance(t.c, dets[i].c);
            if (d < bd) { bd = d; best = (int)i; }
        }
        if (best >= 0 && bd < camW * 0.18f) {
            used[best] = true;
            glm::vec2 nc = dets[best].c;
            glm::vec2 v = (nc - t.c) / dt;
            t.vel = glm::mix(t.vel, v, 0.4f);
            t.speed = glm::length(t.vel);
            t.c = glm::mix(t.c, nc, 0.4f);
            ofRectangle& d = dets[best].r;                 // smoothed bbox -> steady, clean boxes
            float k = 0.35f;
            t.bbox.x      = ofLerp(t.bbox.x, d.x, k);
            t.bbox.y      = ofLerp(t.bbox.y, d.y, k);
            t.bbox.width  = ofLerp(t.bbox.width, d.width, k);
            t.bbox.height = ofLerp(t.bbox.height, d.height, k);
            t.area = ofLerp(t.area, dets[best].area, k);
            t.age += dt;
            t.dwell = (t.speed < camW * 0.03f) ? t.dwell + dt : 0.0f;
            t.lastSeen = 0;
        } else {
            t.lastSeen++;
        }
    }
    tracks.erase(std::remove_if(tracks.begin(), tracks.end(),
                 [](const CvTrack& t) { return t.lastSeen > 10; }), tracks.end());
    for (size_t i = 0; i < dets.size(); i++)
        if (!used[i]) {
            CvTrack t; t.id = nextId++; t.c = dets[i].c; t.bbox = dets[i].r; t.area = dets[i].area;
            tracks.push_back(t);
        }

    float e = 0;
    for (auto& t : tracks) e = std::max(e, t.speed / (camW * 0.5f));
    motionEnergy = glm::mix(motionEnergy, ofClamp(e, 0, 1.0f), 0.25f);
}

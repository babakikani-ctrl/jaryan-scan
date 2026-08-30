#include "ofApp.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>

// ================================================================= setup
void ofApp::setup() {
    ofSetVerticalSync(true);
    ofSetFrameRate(60);
    ofDisableArbTex();
    ofSetCircleResolution(64);
    ofBackground(0);

    bool fake = (getenv("JARYAN_FAKECAM") != nullptr);
    autoShot  = (getenv("JARYAN_AUTOSHOT") != nullptr);
    if (getenv("JARYAN_FULLSCREEN")) { ofSetFullscreen(true); ofHideCursor(); previewMode = 2; }
    cv.setup(fake, 1280, 720);          // high-res source for sharp crops

    fLabel.load("fonts/mono.ttf", 12, true, true);
    fTiny.load("fonts/mono.ttf", 9, true, true);
    fBin.load("fonts/mono.ttf", 8, true, true);
    fBig.load("fonts/mono.ttf", 20, true, true);

    machine.load("shaders/post.vert", "shaders/machine.frag");
    coderain.load("shaders/post.vert", "shaders/coderain.frag");
    floorRainShader.load("shaders/post.vert", "shaders/floor_data.frag");   // clean flowing numbers
    floorFlowShader.load("shaders/post.vert", "shaders/floor_flow.frag");
    aFlow.load("shaders/post.vert", "shaders/a_flow.frag");                  // acid effects on the floor
    aPInit.load("shaders/a_passthru.vert", "shaders/a_pinit.frag");
    aPVel.load("shaders/a_passthru.vert", "shaders/a_psim_vel.frag");
    aPPos.load("shaders/a_passthru.vert", "shaders/a_psim_pos.frag");
    aPRender.load("shaders/a_prender.vert", "shaders/a_prender.frag");
    aCodeRain.load("shaders/post.vert", "shaders/a_coderain.frag");
    personFg.load("shaders/post.vert", "shaders/fg_person.frag");            // clear person in FRONT of the effect
    cascInit.load("shaders/a_passthru.vert", "shaders/casc_init.frag");     // floor code-cascade
    cascSim.load("shaders/a_passthru.vert", "shaders/casc_sim.frag");
    cascRender.load("shaders/casc_render.vert", "shaders/casc_render.frag");
    fGlyph.load("fonts/mono.ttf", 30, true, true);
    buildGlyphAtlas();
    initFloorFx();
    initCascade();
    initTerminal();                                                         // FLOOR live terminal

    pcloud.load("shaders/pcloud.vert", "shaders/pcloud.frag");
    glitchPost.load("shaders/post.vert", "shaders/glitch.frag");
    cloudMesh.setMode(OF_PRIMITIVE_POINTS);
    for (int y = 0; y < 150; y++)
        for (int x = 0; x < 200; x++) {
            cloudMesh.addVertex(glm::vec3(0, 0, 0));
            cloudMesh.addTexCoord(glm::vec2((x + 0.5f) / 200.0f, (y + 0.5f) / 150.0f));
        }
    ofFbo::Settings f3; f3.width = WALL_W; f3.height = WALL_H; f3.internalformat = GL_RGBA;
    f3.useDepth = true; f3.numSamples = 4; f3.minFilter = GL_LINEAR; f3.maxFilter = GL_LINEAR;
    fbo3d.allocate(f3);
    cam3.setNearClip(0.01f); cam3.setFarClip(60.0f);
    glEnable(GL_PROGRAM_POINT_SIZE);

    unitQuad.setMode(OF_PRIMITIVE_TRIANGLES);
    auto q = [&](float x, float y) { unitQuad.addVertex(glm::vec3(x, y, 0)); unitQuad.addTexCoord(glm::vec2(x, y)); };
    q(0,0); q(1,0); q(1,1); q(0,0); q(1,1); q(0,1);

    ofFbo::Settings fs; fs.internalformat = GL_RGBA; fs.numSamples = 4; fs.useDepth = false;
    fs.minFilter = GL_LINEAR; fs.maxFilter = GL_LINEAR;
    fs.width = WALL_W;  fs.height = WALL_H;  wallFbo.allocate(fs);
    fs.width = FLOOR_W; fs.height = FLOOR_H; floorFbo.allocate(fs);
    ofFbo::Settings mf; mf.width = WALL_W; mf.height = WALL_H; mf.internalformat = GL_RGBA;
    mf.numSamples = 0; mf.useDepth = false; mf.minFilter = GL_LINEAR; mf.maxFilter = GL_LINEAR;
    for (int i = 0; i < 2; i++) { motionFbo[i].allocate(mf); motionFbo[i].begin(); ofClear(0, 0, 0, 255); motionFbo[i].end(); }

    voices.resize(24);
    ofSoundStreamSettings ss;
    ss.numOutputChannels = 2; ss.numInputChannels = 0;
    ss.sampleRate = sampleRate; ss.bufferSize = 512; ss.numBuffers = 2;
    ss.setOutListener(this);
    if (soundOn) stream.setup(ss);      // muted for now — focus on the visual

    // layout panel — hand-tune each surface's placement on the HDMI canvas, then save ('s')
    canvasW.set("canvasW", CANVAS_W, 320, 4096);
    canvasH.set("canvasH", CANVAS_H, 320, 8192);
    wallX.set("wallX", 0, 0, 4096);        wallY.set("wallY", 0, 0, 8192);
    wallW.set("wallW", WALL_W, 16, 4096);  wallH.set("wallH", WALL_H, 16, 8192);
    floorX.set("floorX", 0, 0, 4096);      floorY.set("floorY", WALL_H, 0, 8192);
    floorW.set("floorW", FLOOR_W, 16, 4096); floorH.set("floorH", FLOOR_H, 16, 8192);
    showSeam.set("show seam", false);
    // ---- live controls (tune everything on-site) ----
    pMirror.set("mirror", true);
    pAutoCycle.set("auto-cycle worlds", true);
    pManualMode.set("manual world 0-9", 0, 0, 9);
    pDetConf.set("detect sensitivity", 0.45f, 0.10f, 0.95f);
    pCutSec.set("world seconds", 120, 5, 300);
    pBright.set("brightness", 1.0f, 0.2f, 1.6f);
    pGlitch.set("glitch", 0.0f, 0.0f, 1.0f);
    pEnergy.set("interactivity", 1.0f, 0.2f, 3.0f);
    pShowPerson.set("show person", true);
    pRim.set("person rim/scan", 1.0f, 0.0f, 2.0f);
    pTermSpeed.set("terminal speed", 1.0f, 0.2f, 3.0f);
    pShowDbgP.set("show debug (i)", true);
    pUseNDI.set("NDI source (n)", false);          // take video FROM an NDI sender (TouchDesigner) instead of the camera

    gui.setup("CONTROL  P:hide  s:save");
    gui.add(ifCanvasW.setup(canvasW)); gui.add(ifCanvasH.setup(canvasH));   // typeable exact numbers
    gui.add(ifWallX.setup(wallX));   gui.add(ifWallY.setup(wallY));
    gui.add(ifWallW.setup(wallW));   gui.add(ifWallH.setup(wallH));
    gui.add(ifFloorX.setup(floorX)); gui.add(ifFloorY.setup(floorY));
    gui.add(ifFloorW.setup(floorW)); gui.add(ifFloorH.setup(floorH));
    gui.add(showSeam);
    gui.add(pMirror); gui.add(pAutoCycle); gui.add(pManualMode);
    gui.add(pDetConf); gui.add(pCutSec);
    gui.add(pBright); gui.add(pGlitch); gui.add(pEnergy);
    gui.add(pShowPerson); gui.add(pRim); gui.add(pTermSpeed);
    gui.add(pShowDbgP);
    gui.add(pUseNDI);
    gui.add(btnReconnect.setup("reconnect camera"));
    gui.add(btnRelearnBg.setup("relearn background"));
    gui.add(btnFullscreen.setup("fullscreen"));
    btnReconnect.addListener(this, &ofApp::onReconnect);
    btnRelearnBg.addListener(this, &ofApp::onRelearnBg);
    btnFullscreen.addListener(this, &ofApp::onFullscreen);
    gui.loadFromFile("layout.xml");
    gui.setPosition(10, 10);

    nextCut = ofRandom(0.5f, 2.6f);
}

// ================================================================= meta / update
ofApp::Meta& ofApp::metaFor(const CvTrack& tr) {
    auto it = meta.find(tr.id);
    if (it != meta.end()) return it->second;
    Meta m;
    ofColor cols[3] = { ofColor(235, 40, 45), ofColor(40, 205, 75), ofColor(45, 115, 240) };
    m.tint = cols[tr.id % 3];
    std::string b; for (int i = 0; i < 64; i++) b += (ofRandom(1) > 0.5f ? '1' : '0');
    m.bits = b; m.ox = ofRandom(-30, 30); m.oy = ofRandom(-24, 24);
    meta[tr.id] = m;
    return meta[tr.id];
}

void ofApp::update() {
    float dt = ofClamp(ofGetLastFrameTime(), 0.0001f, 0.1f);
    t += dt;
    cv.update(dt);
    energy = cv.motionEnergy;

    std::set<int> now;
    for (auto& tr : cv.tracks) {
        now.insert(tr.id);
        Meta& m = metaFor(tr);
        m.trail.push_back(glm::vec2(tr.c.x, tr.c.y));
        if (m.trail.size() > 64) m.trail.pop_front();
    }
    for (int id : now)  if (!seen.count(id)) triggerDetect();
    for (int id : seen) if (!now.count(id)) triggerLost();
    seen = now;
    for (auto it = meta.begin(); it != meta.end(); )        // prune dead tracks -> no leak over days
        if (!now.count(it->first)) it = meta.erase(it); else ++it;
    droneTarget = ofClamp((float)cv.tracks.size() * 0.05f, 0.0f, 0.35f);

    energyHist.push_back(energy);
    if ((int)energyHist.size() > 600) energyHist.erase(energyHist.begin());

    cv.mirrorCam = pMirror;                                    // live panel controls
    cv.setDetConf(pDetConf);
    showDbg = pShowDbgP;
    if (!ndiUiInit) { pUseNDI = cv.ndiEnabled(); ndiUiInit = true; }      // reflect the source.txt choice on the panel
    else if (pUseNDI.get() != cv.ndiEnabled()) cv.setNdiEnabled(pUseNDI.get());   // live switch camera <-> NDI
    if (!autoShot) {
        if (pAutoCycle) { modeT += dt; if (modeT > nextCut) cut(); }
        else            { mode = ofClamp((int)pManualMode, 0, nEffects - 1); modeT = 0; }
    }

    cyber = std::max(0.0f, cyber - dt * 1.8f);                 // cyber-burst decays
    if (!autoShot && ofRandom(1) < 0.006f) cyber = ofRandom(0.6f, 1.0f);   // occasional intense cyber/glitch moment

    updateTerminal(dt);                                       // FLOOR live terminal: typing + events
}

void ofApp::cut() {
    int nm; do { nm = (int)ofRandom(nEffects); } while (nm == mode && nEffects > 1);
    mode = nm; modeT = 0;
    nextCut = pCutSec;                     // world duration from the panel (default ~2 min)
    triggerCut();
}

void ofApp::onReconnect()  { cv.reconnectSource(); }
void ofApp::onRelearnBg()  { cv.captureBg(); }
void ofApp::onFullscreen() { ofToggleFullscreen(); ofHideCursor(); }

// ================================================================= helpers
ofRectangle ofApp::trackRect(const CvTrack& tr) {
    // camera source is already selfie-mirrored in CvPipeline -> map straight through
    float x0 = (tr.bbox.x / cv.camW) * rw;
    float x1 = ((tr.bbox.x + tr.bbox.width) / cv.camW) * rw;
    float y0 = (tr.bbox.y / cv.camH) * rh, y1 = ((tr.bbox.y + tr.bbox.height) / cv.camH) * rh;
    return ofRectangle(x0, y0, x1 - x0, y1 - y0);
}
glm::vec4 ofApp::camUv(const CvTrack& tr) {
    // source is already selfie-mirrored in CvPipeline -> sample straight (no in-place flip)
    float u0 = tr.bbox.x / cv.camW, u1 = (tr.bbox.x + tr.bbox.width) / cv.camW;
    float v0 = tr.bbox.y / cv.camH, v1 = (tr.bbox.y + tr.bbox.height) / cv.camH;
    return glm::vec4(u0, v0, u1, v1);
}
void ofApp::drawMachine(ofRectangle dst, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec2 cells, ofColor tint, float gray, float scan) {
    machine.begin();
    machine.setUniformTexture("uCam", cv.cameraTexture(), 0);
    machine.setUniformTexture("uSil", cv.silhouetteTexture(), 1);
    machine.setUniform2f("uUvMin", uvMin.x, uvMin.y);
    machine.setUniform2f("uUvMax", uvMax.x, uvMax.y);
    machine.setUniform2f("uCells", cells.x, cells.y);
    machine.setUniform3f("uTint", tint.r / 255.0f, tint.g / 255.0f, tint.b / 255.0f);
    machine.setUniform1f("uGray", gray);
    machine.setUniform1f("uScan", scan);
    ofSetColor(255);
    ofPushMatrix(); ofTranslate(dst.x, dst.y); ofScale(dst.width, dst.height); unitQuad.draw(); ofPopMatrix();
    machine.end();
}
void ofApp::drawBox(const ofRectangle& r, const std::string& id, float conf, ofColor c) {
    ofPushStyle();
    ofNoFill(); ofSetLineWidth(1.3f); ofSetColor(c);
    float L = ofClamp(std::min(r.width, r.height) * 0.22f, 8, 26);
    ofDrawLine(r.x, r.y, r.x + L, r.y);                       ofDrawLine(r.x, r.y, r.x, r.y + L);
    ofDrawLine(r.x + r.width, r.y, r.x + r.width - L, r.y);   ofDrawLine(r.x + r.width, r.y, r.x + r.width, r.y + L);
    ofDrawLine(r.x, r.y + r.height, r.x + L, r.y + r.height); ofDrawLine(r.x, r.y + r.height, r.x, r.y + r.height - L);
    ofDrawLine(r.x + r.width, r.y + r.height, r.x + r.width - L, r.y + r.height);
    ofDrawLine(r.x + r.width, r.y + r.height, r.x + r.width, r.y + r.height - L);
    ofRectangle bb = fTiny.getStringBoundingBox(id, 0, 0);
    ofFill(); ofSetColor(c); ofDrawRectangle(r.x, r.y - 14, bb.width + 8, 14);
    ofSetColor(255); fTiny.drawString(id, r.x + 4, r.y - 4);
    ofSetColor(c); fTiny.drawString(ofToString(conf, 2), r.x + r.width - 28, r.y - 4);
    ofPopStyle();
}
void ofApp::drawPersonReal(const CvTrack& tr) {
    // the person's real, recognizable video (mirrored, near full-res grayscale) — so visitors SEE THEMSELVES
    ofRectangle r = trackRect(tr);
    glm::vec4 uv = camUv(tr);
    glm::vec2 cells(std::max(2.0f, r.width * 0.5f), std::max(2.0f, r.height * 0.5f));
    ofSetColor(255);
    drawMachine(r, glm::vec2(uv.x, uv.y), glm::vec2(uv.z, uv.w), cells, ofColor(255), 1.0f, 0.12f);
}

void ofApp::drawPersonFg() {
    // the tracked visitor, CLEAR & sharp (real mirrored video, silhouette-masked, cyan scan-rim),
    // drawn IN FRONT of whatever effect is running behind — so people always see themselves.
    if (cv.tracks.empty()) return;
    personFg.begin();
    personFg.setUniformTexture("uCam", cv.cameraTexture(), 0);
    personFg.setUniformTexture("uSil", cv.subjectTexture(), 1);       // only DETECTED people (no background clutter)
    personFg.setUniform2f("uTexel", 1.0f / cv.camW, 1.0f / cv.camH);
    personFg.setUniform1f("uTime", t);
    personFg.setUniform1f("uRim", pRim);
    ofSetColor(255);
    ofPushMatrix(); ofScale(rw, rh); unitQuad.draw(); ofPopMatrix();
    personFg.end();
}

void ofApp::updateMotionTrail() {
    int nx = 1 - trailCur;
    motionFbo[nx].begin();
    ofClear(0, 0, 0, 255);
    ofSetColor(255, 255, 255, 243);                        // slower decay -> long flowing energy trails
    motionFbo[trailCur].draw(0, 0);
    ofPixels& mp = cv.motionPixels();
    int mw = (int)mp.getWidth(), mh = (int)mp.getHeight();
    if (mw > 0 && !cv.tracks.empty()) {
        const unsigned char* d = mp.getData();
        const unsigned char* sub = cv.subjectPixels().getData();
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        int GX = 120, GY = 68;
        for (int gy = 0; gy < GY; gy++) for (int gx = 0; gx < GX; gx++) {
            float u = (gx + 0.5f) / GX, v = (gy + 0.5f) / GY;
            int idx = ((int)(v * mh)) * mw + (int)(u * mw);
            if (sub[idx] < 60) continue;                        // only on detected people (ignore background)
            float m = d[idx] / 255.0f;
            if (m < 0.10f) continue;
            float e = ofClamp((m - 0.10f) / 0.45f, 0, 1);
            e = ofClamp(e * pEnergy.get(), 0.0f, 1.0f);         // interactivity strength (panel)
            float sx = u * WALL_W, sy = v * WALL_H;              // source already mirrored -> follows the hands
            ofColor col = ofColor(40, 190, 255).getLerped(ofColor(255, 70, 210), e);   // cyan -> magenta by speed
            ofSetColor(col, (int)(110 * e)); ofDrawCircle(sx, sy, 10 + e * 34);         // big soft aura
            ofSetColor(col, (int)(200 * e)); ofDrawCircle(sx, sy, 4 + e * 14);          // bright body
            ofSetColor(255, 255, 255, (int)(230 * e)); ofDrawCircle(sx, sy, 1.5f + e * 4.5f);   // white-hot core
        }
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    }
    motionFbo[nx].end();
    trailCur = nx;
}

void ofApp::drawMotionSparks() {
    ofPixels& mp = cv.motionPixels();
    int mw = (int)mp.getWidth(), mh = (int)mp.getHeight();
    if (mw <= 0) return;
    const unsigned char* d = mp.getData();
    const unsigned char* sub = cv.subjectPixels().getData();
    int GX = 72, GY = 40;
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    for (int gy = 0; gy < GY; gy++) for (int gx = 0; gx < GX; gx++) {
        float u = (gx + 0.5f) / GX, v = (gy + 0.5f) / GY;
        int idx = ((int)(v * mh)) * mw + (int)(u * mw);
        if (sub[idx] < 60) continue;                            // only on detected people
        float m = d[idx] / 255.0f;
        if (m < 0.12f) continue;
        float e = ofClamp((m - 0.12f) / 0.5f, 0, 1);
        e = ofClamp(e * pEnergy.get(), 0.0f, 1.0f);
        float sx = u * rw, sy = v * rh;                             // source already mirrored -> follows the visitor
        ofSetColor(150, 230, 255, (int)(235 * e)); ofSetLineWidth(1.0f + e * 1.8f);   // radiating energy tendrils
        int ns = 5 + (int)(e * 9);
        for (int i = 0; i < ns; i++) {
            float a = (gx * 7 + gy * 3 + i * 2.399f) + t * 3.6f;
            float len = 10.0f + e * 46.0f;
            ofDrawLine(sx, sy, sx + cosf(a) * len, sy + sinf(a) * len);
        }
        ofNoFill(); ofSetLineWidth(1.4f); ofSetColor(120, 220, 255, (int)(200 * e)); ofDrawCircle(sx, sy, 5 + e * 14);
        ofFill(); ofSetColor(255, 255, 255, (int)(240 * e)); ofDrawCircle(sx, sy, 2.0f + e * 4.0f);
    }
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofPopStyle();
}

void ofApp::drawMotionCode() {
    // where the visitor moves, colourful CODE glyphs bloom inside the white energy — CLEAR
    // and readable (a dark backing lifts them off the bright glow).
    ofPixels& mp = cv.motionPixels();
    int mw = (int)mp.getWidth(), mh = (int)mp.getHeight();
    if (mw <= 0) return;
    const unsigned char* d = mp.getData();
    const unsigned char* sub = cv.subjectPixels().getData();
    static const char* CH = "0123456789ABCDEF#%*+=<>/\\{}$&@";
    const int nc = 29;
    ofColor pal[6] = { ofColor(0,220,255), ofColor(255,60,200), ofColor(255,210,0),
                       ofColor(70,255,140), ofColor(255,120,20), ofColor(235,245,255) };
    int GX = 42, GY = 26, frame = (int)(t * 7.0f);
    ofPushStyle();
    for (int gy = 0; gy < GY; gy++) for (int gx = 0; gx < GX; gx++) {
        float u = (gx + 0.5f) / GX, v = (gy + 0.5f) / GY;
        int idx = ((int)(v * mh)) * mw + (int)(u * mw);
        if (sub[idx] < 60) continue;                            // only on detected people
        float m = d[idx] / 255.0f;
        if (m < 0.20f) continue;
        float e = ofClamp((m - 0.20f) / 0.5f, 0, 1);
        e = ofClamp(e * pEnergy.get(), 0.0f, 1.0f);
        float sx = u * rw, sy = v * rh;
        std::string s(1, CH[(gx * 7 + gy * 13 + frame) % nc]);
        ofColor col = pal[(gx + gy * 2 + frame / 3) % 6];
        ofSetColor(0, 0, 0, (int)(190 * e));      fBig.drawString(s, sx + 1.5f, sy + 1.5f);   // readable backing
        ofSetColor(col, (int)(200 + 55 * e));     fBig.drawString(s, sx, sy);                 // clear colour code
    }
    ofPopStyle();
}

void ofApp::drawDet(const CvTrack& tr, ofColor c) {
    ofRectangle r = trackRect(tr);
    float age = tr.age;
    bool acq = age < 0.7f;
    float k = ofClamp(age / 0.7f, 0, 1);
    float ex = acq ? (1.0f - k) * 28.0f : 0.0f;                      // acquiring: reticle closes in
    ofRectangle rr(r.x - ex, r.y - ex, r.width + 2 * ex, r.height + 2 * ex);
    ofPushStyle();
    ofNoFill(); ofSetLineWidth(1.3f);
    ofSetColor(c, acq ? (int)(110 + 130 * fabsf(sinf(t * 18.0f))) : 255);
    float L = ofClamp(std::min(rr.width, rr.height) * 0.22f, 8, 26);
    ofDrawLine(rr.x, rr.y, rr.x + L, rr.y);                                   ofDrawLine(rr.x, rr.y, rr.x, rr.y + L);
    ofDrawLine(rr.x + rr.width, rr.y, rr.x + rr.width - L, rr.y);             ofDrawLine(rr.x + rr.width, rr.y, rr.x + rr.width, rr.y + L);
    ofDrawLine(rr.x, rr.y + rr.height, rr.x + L, rr.y + rr.height);           ofDrawLine(rr.x, rr.y + rr.height, rr.x, rr.y + rr.height - L);
    ofDrawLine(rr.x + rr.width, rr.y + rr.height, rr.x + rr.width - L, rr.y + rr.height);
    ofDrawLine(rr.x + rr.width, rr.y + rr.height, rr.x + rr.width, rr.y + rr.height - L);
    std::string lab = acq ? "ACQUIRING" : "ID:" + ofToString(tr.id % 1000, 3, '0');
    ofRectangle bb = fTiny.getStringBoundingBox(lab, 0, 0);
    ofFill(); ofSetColor(c); ofDrawRectangle(rr.x, rr.y - 14, bb.width + 8, 14);
    ofSetColor(255); fTiny.drawString(lab, rr.x + 4, rr.y - 4);
    float conf = acq ? k * 0.9f : 0.82f + 0.16f * ofNoise(tr.id * 3.1f, t * 0.3f);
    ofSetColor(c); fTiny.drawString(ofToString(conf, 2), rr.x + rr.width - 28, rr.y - 4);
    if (!acq && age < 1.5f) { ofSetColor(c, 210); fTiny.drawString("LOCKED", rr.x, rr.y + rr.height + 12); }
    ofPopStyle();
}

void ofApp::drawHUD(const std::string& tag) {
    // persistent system chrome — ties every mode into ONE machine
    ofColor ink(200, 206, 212);
    float m = 14, x0 = m, y0 = m, x1 = rw - m, y1 = rh - m, L = 16;
    ofPushStyle();
    ofNoFill(); ofSetLineWidth(1);
    ofSetColor(ink, 34); ofDrawRectangle(x0, y0, x1 - x0, y1 - y0);            // frame
    ofSetColor(ink, 150);                                                      // corner registration ticks
    ofDrawLine(x0, y0, x0 + L, y0); ofDrawLine(x0, y0, x0, y0 + L);
    ofDrawLine(x1, y0, x1 - L, y0); ofDrawLine(x1, y0, x1, y0 + L);
    ofDrawLine(x0, y1, x0 + L, y1); ofDrawLine(x0, y1, x0, y1 - L);
    ofDrawLine(x1, y1, x1 - L, y1); ofDrawLine(x1, y1, x1, y1 - L);
    float scanY = y0 + fmodf(t * 55.0f, y1 - y0);                              // slow scan sweep (life)
    ofSetColor(ink, 22); ofDrawLine(x0, scanY, x1, scanY);
    ofFill();
    ofSetColor(230, 40, 45); ofDrawCircle(x0 + 8, y0 + 10, 3);                 // REC
    ofSetColor(ink);
    fTiny.drawString("REC", x0 + 18, y0 + 14);
    fTiny.drawString("TRK " + ofToString((int)cv.tracks.size(), 2, '0'), x0 + 50, y0 + 14);
    fTiny.drawString(tag, x0 + 104, y0 + 14);
    int fr = ofGetFrameNum(); char tc[40];
    snprintf(tc, sizeof(tc), "SCAN.SYS  %02d:%02d:%02d", (fr / 3600) % 60, (fr / 60) % 60, fr % 60);
    ofRectangle bb = fTiny.getStringBoundingBox(tc, 0, 0);
    fTiny.drawString(tc, x1 - bb.width, y0 + 14);
    ofSetColor(ink, 70);                                                       // bottom ruler ticks
    for (int i = 0; i <= 32; i++) { float x = ofLerp(x0, x1, i / 32.0f); ofDrawLine(x, y1 - (i % 4 == 0 ? 6 : 3), x, y1); }
    ofPopStyle();
}

// ================================================================= WALL modes
void ofApp::renderWall() {
    wallFbo.begin();
    rw = WALL_W; rh = WALL_H;
    if      (cv.tracks.empty()) modeAttract();     // idle / invite when no one is present
    else if (mode == 0) modeVolumes();
    else if (mode == 1) modeCards();
    else if (mode == 2) modeMesh();
    else if (mode == 3) modePhotogram();
    else if (mode == 4) modeRain();
    else if (mode == 5) modeParts();
    else if (mode == 6) modeCloud();
    else if (mode == 7) modeOrbit();
    else if (mode == 8) modeScan();
    else                modeWallSwarm();           // the acid code-glyph swarm — one of the changing worlds
    if (pShowPerson) drawPersonFg();                // CLEAR tracked visitor IN FRONT of the effect
    if (!cv.tracks.empty()) {                       // interactive energy: white glow + sparks + readable code
        ofEnableBlendMode(OF_BLENDMODE_ADD); ofSetColor(255);
        motionFbo[trailCur].draw(0, 0);
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
        drawMotionSparks();
        drawMotionCode();                           // colourful code glyphs bloom in the white energy
    }
    wallFbo.end();
}

void ofApp::modeWallSwarm() {
    // ambient swirling code-glyph field (background); hands swirl it (interactive)
    ofClear(4, 5, 9, 255);
    drawWallSwarm();
    for (auto& tr : cv.tracks) drawDet(tr, ofColor(200, 206, 212));
    drawHUD("SWARM.CODE");
}

void ofApp::modeVolumes() {
    ofClear(12, 13, 15, 255);
    float rv = ofClamp(modeT / 0.6f, 0, 1);
    for (auto& tr : cv.tracks) {
        float ap = ofClamp(tr.age / 0.4f, 0, 1) * rv;               // build-up (per person + mode)
        ofRectangle r = trackRect(tr); Meta& m = metaFor(tr); glm::vec4 uv = camUv(tr);
        ofRectangle slab(r.getCenter().x - r.width * 0.42f, r.y - r.height * 0.14f, r.width * 0.84f, r.height * 1.28f);
        glm::vec2 cells(ofLerp(4, 28, rv), ofLerp(8, 64, rv));      // resolve coarse -> fine (person recognizable)
        drawMachine(slab, glm::vec2(uv.x, uv.y), glm::vec2(uv.z, uv.w), cells, m.tint, 0.0f, 0.6f);
        ofSetColor(12, 13, 15, (int)(255 * (1.0f - ap)));           // fade in
        ofDrawRectangle(slab);
        drawDet(tr, m.tint);
        ofSetColor(m.tint); fTiny.drawString("PERSON", slab.x, slab.y + slab.height + 12);
    }
    drawHUD("SEG.VOL");
}
void ofApp::modeCards() {
    ofClear(8, 9, 12, 255);
    float rv = ofClamp(modeT / 0.6f, 0, 1);
    std::vector<glm::vec2> ctr;
    for (auto& tr : cv.tracks) {
        float ap = ofClamp(tr.age / 0.4f, 0, 1) * rv;
        ofRectangle r = trackRect(tr); Meta& m = metaFor(tr); glm::vec4 uv = camUv(tr);
        float cw = ofClamp(r.width, 60.0f, 130.0f), ch = cw * 1.7f;
        ofRectangle card(r.getCenter().x - cw * 0.5f + m.ox * 0.3f, r.getCenter().y - ch * 0.5f + m.oy * 0.3f, cw, ch);
        ctr.push_back(card.getCenter());
        drawMachine(card, glm::vec2(uv.x, uv.y), glm::vec2(uv.z, uv.w), glm::vec2(cw / 4.0f, ch / 4.0f), ofColor(255), 1.0f, 0.3f);
        ofSetColor(8, 9, 12, 255); ofDrawRectangle(card.x, card.y + ch * ap, cw, ch * (1.0f - ap));   // scan-in wipe
        ofNoFill(); ofSetLineWidth(1.3f); ofSetColor(185, 192, 200); ofDrawRectangle(card);
        ofFill(); ofSetColor(185, 192, 200); ofDrawRectangle(card.x, card.y - 12, 44, 12);
        ofSetColor(10, 12, 16); fTiny.drawString(ofToString(tr.id % 1000, 3, '0'), card.x + 3, card.y - 3);
    }
    ofSetColor(150, 160, 170, 70); ofSetLineWidth(1);
    for (size_t i = 0; i < ctr.size(); i++)
        for (size_t j = i + 1; j < ctr.size(); j++)
            if (glm::distance(ctr[i], ctr[j]) < 520) ofDrawLine(ctr[i].x, ctr[i].y, ctr[j].x, ctr[j].y);
    drawHUD("CROP.ID");
}
void ofApp::modeMesh() {
    ofClear(8, 9, 12, 255);
    for (auto& tr : cv.tracks) drawPersonReal(tr);       // real person under the wireframe
    std::vector<glm::vec2> n;
    for (auto& pl : cv.contours) {
        int cnt = (int)pl.size(); if (cnt < 3) continue;
        int step = std::max(1, cnt / 26);
        for (int i = 0; i < cnt; i += step) { auto p = pl[i]; n.push_back(glm::vec2((p.x / cv.camW) * rw, (p.y / cv.camH) * rh)); }
    }
    float rv = ofClamp(modeT / 0.6f, 0, 1);
    int shown = (int)(n.size() * rv);
    ofSetColor(150, 160, 170, 190); ofSetLineWidth(1.0f);
    for (int i = 0; i < shown; i++) {
        int best[3] = { -1,-1,-1 }; float bd[3] = { 1e9f,1e9f,1e9f };
        for (size_t j = 0; j < n.size(); j++) {
            if ((int)j == i) continue;
            float d = glm::distance(n[i], n[j]);
            if (d < bd[0]) { bd[2]=bd[1];best[2]=best[1];bd[1]=bd[0];best[1]=best[0];bd[0]=d;best[0]=(int)j; }
            else if (d < bd[1]) { bd[2]=bd[1];best[2]=best[1];bd[1]=d;best[1]=(int)j; }
            else if (d < bd[2]) { bd[2]=d;best[2]=(int)j; }
        }
        for (int k = 0; k < 3; k++) if (best[k] >= 0) ofDrawLine(n[i].x, n[i].y, n[best[k]].x, n[best[k]].y);
    }
    ofFill(); ofSetColor(210, 216, 222);
    for (int i = 0; i < shown; i++) ofDrawRectangle(n[i].x - 2, n[i].y - 2, 4, 4);
    ofSetColor(120, 140, 150); int bi = 0;
    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr); ofRectangle r = trackRect(tr);
        std::string s = m.bits.substr((int)(t * 6 + bi * 7) % 40, 16);
        fBin.drawString(s, r.getCenter().x - 40, r.getCenter().y + (bi % 3 - 1) * 16); bi++;
        drawDet(tr, ofColor(200, 206, 212));   // always-on detection
    }
    drawHUD("MESH.PCL");
}
void ofApp::modePhotogram() {
    ofClear(8, 9, 12, 255);
    float rv = ofClamp(modeT / 0.8f, 0, 1);                          // the ground assembles bit by bit
    float hy = rh * 0.40f, vx = rw * 0.5f;
    ofSetLineWidth(1.0f);
    int rowsShown = (int)(12 * rv), colsShown = (int)(16 * rv);
    for (int r = 0; r <= rowsShown; r++) {
        float f = (float)r / 12.0f, y = hy + (rh - hy) * (f * f);
        ofSetColor(150, 152, 156, (int)(120 + 120 * f));
        ofDrawLine(ofLerp(vx, 0, f + 0.05f), y, ofLerp(vx, rw, f + 0.05f), y);
    }
    for (int c = 0; c <= colsShown; c++) {
        float fx = (float)c / 16.0f;
        ofSetColor(150, 152, 156, 110);
        ofDrawLine(ofLerp(vx, ofLerp(0, rw, fx), 0.10f), hy, ofLerp(vx, ofLerp(0, rw, fx), 1.35f), rh);
    }
    for (auto& tr : cv.tracks) {
        float ap = ofClamp(tr.age / 0.4f, 0, 1);
        ofRectangle r = trackRect(tr); glm::vec4 uv = camUv(tr);
        drawMachine(r, glm::vec2(uv.x, uv.y), glm::vec2(uv.z, uv.w), glm::vec2(r.width / 4.0f, r.height / 4.0f), ofColor(255), 1.0f, 0.25f);
        ofSetColor(8, 9, 12, (int)(255 * (1.0f - ap)));
        ofDrawRectangle(r);
        drawDet(tr, ofColor(200, 206, 212));
    }
    drawHUD("PHOTOGRAM");
}

void ofApp::modeParts() {
    // decompose each detected person into body parts (head / hands / legs) from the silhouette
    ofClear(10, 11, 14, 255);
    for (auto& tr : cv.tracks) drawPersonReal(tr);       // real person under the part markers
    int cw = cv.camW, ch = cv.camH;
    ofPixels& sil = cv.silhouettePixels();
    const unsigned char* d = sil.getData();
    bool ok = (d != nullptr && (int)sil.getWidth() == cw && (int)sil.getHeight() == ch);
    auto S  = [&](float x, float y) { return glm::vec2((x / cw) * rw, (y / ch) * rh); };
    auto fg = [&](int x, int y) { return ok && x >= 0 && x < cw && y >= 0 && y < ch && d[y * cw + x] > 100; };

    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr);
        ofRectangle b = tr.bbox;
        int bx0 = (int)ofClamp(b.x, 0, cw - 1), bx1 = (int)ofClamp(b.x + b.width, 1, cw);
        int by0 = (int)ofClamp(b.y, 0, ch - 1), by1 = (int)ofClamp(b.y + b.height, 1, ch);
        int bw = bx1 - bx0, bh = by1 - by0; if (bw < 8 || bh < 8) continue;

        // HEAD — topmost foreground, centred over the top band
        int headY = by1;
        for (int y = by0; y < by1; y += 2) { bool any = false; for (int x = bx0; x < bx1; x += 2) if (fg(x, y)) { any = true; break; } if (any) { headY = y; break; } }
        int hx0 = bx1, hx1 = bx0, hb = std::min(by1, headY + bh / 6);
        for (int y = headY; y < hb; y += 2) for (int x = bx0; x < bx1; x += 2) if (fg(x, y)) { hx0 = std::min(hx0, x); hx1 = std::max(hx1, x); }
        if (hx1 < hx0) { hx0 = bx0; hx1 = bx1; }
        glm::vec2 head = S((hx0 + hx1) * 0.5f, headY + bh * 0.05f);

        // HANDS — leftmost / rightmost foreground in the arm band
        int ay0 = by0 + (int)(bh * 0.28f), ay1 = by0 + (int)(bh * 0.62f);
        int lx = bx1, lyy = (ay0 + ay1) / 2, rx = bx0, ryy = (ay0 + ay1) / 2;
        for (int y = ay0; y < ay1; y += 2) for (int x = bx0; x < bx1; x += 2) if (fg(x, y)) { if (x < lx) { lx = x; lyy = y; } if (x > rx) { rx = x; ryy = y; } }
        glm::vec2 handA = S(lx, lyy), handB = S(rx, ryy);

        // LEGS — bottommost foreground
        int legY = by0;
        for (int y = by1 - 1; y > by0; y -= 2) { bool any = false; for (int x = bx0; x < bx1; x += 2) if (fg(x, y)) { any = true; break; } if (any) { legY = y; break; } }
        glm::vec2 legs = S((bx0 + bx1) * 0.5f, legY - bh * 0.04f);
        glm::vec2 torso = S((bx0 + bx1) * 0.5f, by0 + bh * 0.5f);

        float S0 = (b.height / ch) * rh;   // person screen height for part sizing

        // skeleton
        ofSetColor(m.tint, 130); ofSetLineWidth(1.3f);
        ofDrawLine(head.x, head.y, torso.x, torso.y);
        ofDrawLine(torso.x, torso.y, handA.x, handA.y);
        ofDrawLine(torso.x, torso.y, handB.x, handB.y);
        ofDrawLine(torso.x, torso.y, legs.x, legs.y);
        ofFill(); ofSetColor(m.tint, 160); ofDrawCircle(torso.x, torso.y, 3.5f);

        auto part = [&](glm::vec2 p, float s, const std::string& lab) {
            ofRectangle r(p.x - s * 0.5f, p.y - s * 0.5f, s, s);
            ofNoFill(); ofSetColor(m.tint); ofSetLineWidth(1.4f); ofDrawRectangle(r);
            ofRectangle bb = fTiny.getStringBoundingBox(lab, 0, 0);
            ofFill(); ofSetColor(m.tint); ofDrawRectangle(r.x, r.y - 12, bb.width + 7, 12);
            ofSetColor(255); fTiny.drawString(lab, r.x + 3, r.y - 3);
        };
        part(head,  ofClamp(S0 * 0.17f, 26, 120), "HEAD");
        part(handA, ofClamp(S0 * 0.10f, 20, 80),  "HAND");
        part(handB, ofClamp(S0 * 0.10f, 20, 80),  "HAND");
        part(legs,  ofClamp(S0 * 0.13f, 22, 96),  "LEGS");

        ofSetColor(210); fTiny.drawString("SUBJ " + ofToString(tr.id % 1000, 3, '0'), head.x + S0 * 0.12f, head.y - S0 * 0.10f);
    }
    drawHUD("BODY.PARTS");
}

void ofApp::renderCloudFbo() {
    // render the rotating 3D point cloud into fbo3d — MUST run OUTSIDE wallFbo (nesting an FBO
    // pass inside the wall pass corrupts wallFbo's matrix and flips everything 180°).
    fbo3d.begin();
    ofClear(6, 8, 12, 255);
    ofEnableDepthTest();
    float ang = sin(t * 0.28f) * 0.5f;
    cam3.setPosition(sin(ang) * 2.2f, 0.12f * sin(t * 0.2f), cos(ang) * 2.2f + 0.3f);
    cam3.lookAt(glm::vec3(0, 0, 0.2f), glm::vec3(0, 1, 0));
    cam3.begin();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    pcloud.begin();
    pcloud.setUniformTexture("uCam", cv.cameraTexture(), 0);
    pcloud.setUniformTexture("uSil", cv.silhouetteTexture(), 1);
    pcloud.setUniformTexture("uMotion", cv.motionTexture(), 2);
    pcloud.setUniform1f("uDepth", 1.2f);
    pcloud.setUniform1f("uPoint", 3.0f);
    pcloud.setUniform1f("uAspect", (float)cv.camW / cv.camH);
    cloudMesh.draw();
    pcloud.end();
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    cam3.end();
    ofDisableDepthTest();
    fbo3d.end();
}

void ofApp::modeCloud() {
    // rotating 3D point-cloud behind the visitor (fbo3d already rendered outside wallFbo)
    ofClear(6, 8, 12, 255);
    ofSetColor(255); fbo3d.draw(0, 0);
    for (auto& tr : cv.tracks) drawDet(tr, ofColor(120, 210, 235));
    drawHUD("VOLUME.3D");
}

void ofApp::modeOrbit() {
    // cyber: dots orbiting each person + data text, reacting to their motion
    ofClear(6, 7, 11, 255);
    for (auto& tr : cv.tracks) drawPersonReal(tr);       // real person at the centre of the orbit
    float e = ofClamp(energy, 0, 1);
    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr);
        ofRectangle r = trackRect(tr);
        glm::vec2 c = r.getCenter();
        float R = std::max(r.width, r.height) * 0.55f;
        for (int rg = 0; rg < 3; rg++) {
            float rad = R * (0.5f + 0.32f * rg);
            int nd = 10 + rg * 6;
            float spd = (0.4f + 1.0f * e) * (rg % 2 == 0 ? 1.0f : -1.0f);
            for (int i = 0; i < nd; i++) {
                float a = t * spd + i * TWO_PI / nd + rg;
                float x = c.x + cosf(a) * rad, y = c.y + sinf(a) * rad * 0.85f;
                ofFill(); ofSetColor(m.tint, 220); ofDrawCircle(x, y, 2.2f);
                if (i % 3 == 0) { ofSetColor(m.tint, 45); ofDrawLine(c.x, c.y, x, y); }
            }
        }
        ofFill(); ofSetColor(255); ofDrawCircle(c.x, c.y, 3);
        ofNoFill(); ofSetColor(m.tint, 160); ofDrawCircle(c.x, c.y, R * 0.5f + 6 * sinf(t * 3 + tr.id));
        ofSetColor(m.tint); fTiny.drawString("SUBJ." + ofToString(tr.id % 1000, 3, '0'), c.x + R * 0.6f, c.y - R * 0.4f);
        ofSetColor(150, 200, 220);
        fBin.drawString(m.bits.substr((int)(t * 8 + tr.id) % 40, 20), c.x - R * 0.5f, c.y + R * 0.7f);
        drawDet(tr, m.tint);
    }
    drawHUD("ORBIT.SYS");
}

void ofApp::drawWallTex(float x, float y, float w, float h) {
    float trans = modeT < 0.12f ? (1.0f - modeT / 0.12f) * 0.85f : 0.0f;        // designed glitch flash on each cut
    float amt = ofClamp(0.03f + energy * 0.30f + cyber + trans + pGlitch, 0.0f, 1.0f);   // + panel glitch
    glitchPost.begin();
    glitchPost.setUniformTexture("uCol", wallFbo.getTexture(), 0);
    glitchPost.setUniform2f("uRes", WALL_W, WALL_H);
    glitchPost.setUniform1f("uTime", t);
    glitchPost.setUniform1f("uAmt", amt);
    glitchPost.setUniform1f("uBright", pBright);
    ofSetColor(255);
    wallFbo.draw(x, y, w, h);
    glitchPost.end();
}

void ofApp::modeAttract() {
    ofClear(6, 7, 11, 255);
    glm::vec2 c(rw * 0.5f, rh * 0.5f);
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);
    for (int i = 0; i < 6; i++) { float y = fmodf(t * 40.0f + i * rh / 6.0f, rh); ofSetColor(90, 160, 190, 26); ofDrawLine(0, y, rw, y); }
    ofNoFill(); ofSetLineWidth(1.2f);
    ofSetColor(200, 206, 212, 90); ofDrawCircle(c.x, c.y, 130);
    ofSetColor(120, 210, 235, (int)(120 + 100 * pulse)); ofDrawCircle(c.x, c.y, 60 + 22 * pulse);
    ofSetColor(200, 206, 212, 70);
    ofDrawLine(c.x - 190, c.y, c.x - 20, c.y); ofDrawLine(c.x + 20, c.y, c.x + 190, c.y);
    ofDrawLine(c.x, c.y - 190, c.x, c.y - 20); ofDrawLine(c.x, c.y + 20, c.x, c.y + 190);
    float a = t * 1.1f; ofSetColor(120, 210, 235, 150);
    ofDrawLine(c.x, c.y, c.x + cosf(a) * 130, c.y + sinf(a) * 130);
    ofFill(); ofSetColor(230, 40, 45); ofDrawCircle(c.x, c.y, 3.5f);
    ofSetColor(200, 206, 212);
    std::string t1 = "AWAITING SUBJECT";
    ofRectangle b1 = fLabel.getStringBoundingBox(t1, 0, 0);
    fLabel.drawString(t1, c.x - b1.width * 0.5f, c.y + 208);
    if (fmodf(t, 1.4f) < 1.0f) {
        ofSetColor(120, 140, 150);
        ofRectangle b2 = fTiny.getStringBoundingBox("STEP INTO FRAME", 0, 0);
        fTiny.drawString("STEP INTO FRAME", c.x - b2.width * 0.5f, c.y + 230);
    }
    drawHUD("STANDBY");
}

void ofApp::modeScan() {
    // the person is reconstructed slice by slice as a scan line sweeps down (being 3D-scanned)
    ofClear(6, 7, 11, 255);
    float sweep = fmodf(t * 0.34f, 1.0f);
    float sy = sweep * rh;
    for (auto& tr : cv.tracks) {
        ofRectangle r = trackRect(tr);
        glm::vec4 uv = camUv(tr);
        Meta& m = metaFor(tr);
        drawMachine(r, glm::vec2(uv.x, uv.y), glm::vec2(uv.z, uv.w), glm::vec2(r.width / 3.0f, r.height / 3.0f), ofColor(255), 1.0f, 0.2f);
        float coverY = std::max(sy, r.y);
        if (coverY < r.y + r.height) {
            ofFill(); ofSetColor(6, 7, 11, 255);
            ofDrawRectangle(r.x, coverY, r.width, r.y + r.height - coverY);      // hide the un-scanned part
            ofSetColor(m.tint, 70);                                             // faint pre-scan point matrix
            for (float yy = coverY; yy < r.y + r.height; yy += 8)
                for (float xx = r.x; xx < r.x + r.width; xx += 8)
                    ofDrawRectangle(xx, yy, 1, 1);
        }
        drawDet(tr, m.tint);
    }
    ofSetColor(120, 220, 245, 60); ofFill(); ofDrawRectangle(0, sy - 3, rw, 6);
    ofSetColor(140, 230, 250); ofSetLineWidth(2); ofDrawLine(0, sy, rw, sy);     // bright scan line
    ofSetColor(120, 210, 235);
    fTiny.drawString("SCAN " + ofToString((int)(sweep * 100), 3, '0') + "%", rw * 0.5f - 26, sy - 6);
    drawHUD("SCAN.RECON");
}

void ofApp::buildGlyphAtlas() {
    std::string chars = "0123456789ABCDEFGHJKLMNPRTUVWXYZ#%*+=/<>[]{}$&@!?:;.-_~^abcdefgh";
    int G = glyphGrid, cell = 48, sz = G * cell;
    glyphAtlas.allocate(sz, sz, GL_RGBA);
    glyphAtlas.begin();
    ofClear(0, 0, 0, 255);
    ofSetColor(255);
    for (int i = 0; i < G * G; i++) {
        std::string s(1, chars[i % chars.size()]);
        int cx = i % G, cy = i / G;
        ofRectangle bb = fGlyph.getStringBoundingBox(s, 0, 0);
        float px = cx * cell + (cell - bb.width) * 0.5f - bb.x;
        float py = cy * cell + (cell + bb.height) * 0.5f;
        fGlyph.drawString(s, px, py);
    }
    glyphAtlas.end();
}

void ofApp::modeRain() {
    ofClear(0, 0, 0, 255);
    coderain.begin();
    coderain.setUniformTexture("uCam", cv.cameraTexture(), 0);
    coderain.setUniformTexture("uSil", cv.silhouetteTexture(), 1);
    coderain.setUniformTexture("uAtlas", glyphAtlas.getTexture(), 2);
    coderain.setUniform2f("uGridR", 56.0f, 46.0f);
    coderain.setUniform1f("uTime", t);
    coderain.setUniform1f("uGlyphGrid", (float)glyphGrid);
    ofSetColor(255);
    ofPushMatrix(); ofScale(rw, rh); unitQuad.draw(); ofPopMatrix();
    coderain.end();
    for (auto& tr : cv.tracks)
        drawBox(trackRect(tr), "ID:" + ofToString(tr.id % 1000, 3, '0'), 0.80f + 0.18f * ofNoise(tr.id * 1.9f, t * 0.3f), ofColor(200, 210, 215));
    drawHUD("CODE.RAIN");
}

// ================================================================= FLOOR computation console
void ofApp::floorTelemetry(ofRectangle field) {
    // professional stacked time-series: each subject's X (cyan) / Y (green) traces + VEL
    int n = (int)cv.tracks.size(); if (n == 0) return;
    int shown = std::min(n, 5);
    float bandH = field.height / (float)shown;
    int idx = 0;
    for (auto& tr : cv.tracks) {
        if (idx >= shown) break;
        Meta& m = metaFor(tr);
        ofRectangle band(field.x, field.y + idx * bandH + 8, field.width, bandH - 20);
        ofNoFill(); ofSetColor(30, 34, 40); ofDrawRectangle(band);
        ofFill(); ofSetColor(m.tint); ofDrawRectangle(band.x, band.y - 12, 4, 4);
        ofSetColor(210, 216, 222); fTiny.drawString("SUBJ " + ofToString(tr.id % 1000, 3, '0'), band.x + 10, band.y + 2);
        ofSetColor(24, 26, 32); ofDrawLine(band.x, band.getCenter().y, band.getRight(), band.getCenter().y);
        int cnt = (int)m.trail.size();
        if (cnt > 1) {
            ofNoFill(); ofSetLineWidth(1.4f);
            ofSetColor(60, 200, 235); ofBeginShape();
            for (int i = 0; i < cnt; i++) ofVertex(band.x + band.width * i / (cnt - 1), band.getCenter().y - (m.trail[i].x / cv.camW - 0.5f) * band.height * 0.8f);
            ofEndShape(false);
            ofSetColor(70, 220, 120); ofBeginShape();
            for (int i = 0; i < cnt; i++) ofVertex(band.x + band.width * i / (cnt - 1), band.getCenter().y - (m.trail[i].y / cv.camH - 0.5f) * band.height * 0.8f);
            ofEndShape(false);
        }
        ofSetColor(245, 190, 60); fBin.drawString("VEL " + ofToString((int)tr.speed, 4, '0'), band.x + 10, band.getBottom() - 4);
        ofSetColor(60, 200, 235); fBin.drawString("X", band.getRight() - 60, band.y + 2);
        ofSetColor(70, 220, 120); fBin.drawString("Y", band.getRight() - 44, band.y + 2);
        idx++;
    }
}

void ofApp::floorRain() {
    float hotX[8]; int hn = 0;
    for (auto& tr : cv.tracks) { if (hn >= 8) break; hotX[hn++] = ofClamp(1.0f - tr.c.x / cv.camW, 0, 1); }
    floorRainShader.begin();
    floorRainShader.setUniformTexture("uAtlas", glyphAtlas.getTexture(), 0);
    floorRainShader.setUniform2f("uGridR", 48.0f, 80.0f);        // aligned, readable numbers
    floorRainShader.setUniform1f("uTime", t);
    floorRainShader.setUniform1f("uGlyphGrid", (float)glyphGrid);
    floorRainShader.setUniform1fv("uHotX", hotX, hn > 0 ? hn : 1);
    floorRainShader.setUniform1i("uHotN", hn);
    floorRainShader.setUniform1f("uEnergy", energy);
    ofSetColor(255);
    ofPushMatrix(); ofScale(rw, rh); unitQuad.draw(); ofPopMatrix();
    floorRainShader.end();
    ofSetColor(235); fLabel.drawString("DATA.STREAM // FLOW", 70, 96);
    ofSetColor(90, 94, 100); fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0'), 70, 120);
    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr);
        float sx = ofClamp(1.0f - tr.c.x / cv.camW, 0, 1) * rw;
        ofFill(); ofSetColor(m.tint); ofDrawRectangle(sx - 34, 150, 68, 2);
        ofSetColor(240); fTiny.drawString("SUBJ " + ofToString(tr.id % 1000, 3, '0'), sx - 28, 168);
        ofSetColor(160, 185, 195); fBin.drawString("X" + ofToString(std::max(0, (int)tr.c.x), 4, '0') + " Y" + ofToString(std::max(0, (int)tr.c.y), 4, '0'), sx - 34, 182);
    }
}

void ofApp::initFloorFx() {
    ofFbo::Settings is; is.width = FLOOR_W / 2; is.height = FLOOR_H / 2; is.internalformat = GL_RGBA16F;
    is.numSamples = 0; is.useDepth = false; is.minFilter = GL_LINEAR; is.maxFilter = GL_LINEAR;
    is.wrapModeHorizontal = GL_CLAMP_TO_EDGE; is.wrapModeVertical = GL_CLAMP_TO_EDGE;
    for (int i = 0; i < 2; i++) { aInk[i].allocate(is); aInk[i].begin(); ofClear(0,0,0,255); aInk[i].end(); }

    ofFbo::Settings ps; ps.width = apW; ps.height = apH; ps.internalformat = GL_RGBA16F;
    ps.numSamples = 0; ps.useDepth = false; ps.minFilter = GL_NEAREST; ps.maxFilter = GL_NEAREST;
    ps.wrapModeHorizontal = GL_CLAMP_TO_EDGE; ps.wrapModeVertical = GL_CLAMP_TO_EDGE;
    for (int i = 0; i < 2; i++) { aPos[i].allocate(ps); aVel[i].allocate(ps);
        aVel[i].begin(); ofClear(0,0,0,0); aVel[i].end(); aPos[i].begin(); ofClear(0,0,0,0); aPos[i].end(); }

    aSimQuad.setMode(OF_PRIMITIVE_TRIANGLES);
    auto q = [&](float x, float y, float u, float v){ aSimQuad.addVertex(glm::vec3(x,y,0)); aSimQuad.addTexCoord(glm::vec2(u,v)); };
    q(-1,-1,0,0); q(1,-1,1,0); q(1,1,1,1); q(-1,-1,0,0); q(1,1,1,1); q(-1,1,0,1);
    aPos[0].begin(); aPInit.begin(); aSimQuad.draw(); aPInit.end(); aPos[0].end();
    aPcur = 0;

    aPMesh.setMode(OF_PRIMITIVE_POINTS);
    for (int y = 0; y < apH; y++) for (int x = 0; x < apW; x++) {
        aPMesh.addVertex(glm::vec3(0,0,0));
        aPMesh.addTexCoord(glm::vec2((x+0.5f)/apW, (y+0.5f)/apH));
    }
}

// Step BOTH GPU sims — the wall code-glyph swarm and the floor cascade. MUST run
// outside wallFbo/floorFbo.begin()/end(): nesting an FBO pass inside the surface pass
// corrupts that surface's matrix and flips it 180°.
void ofApp::stepSims() {
    float dt = 1.0f/60.0f;
    // --- WALL code-glyph swarm: velocity then position ping-pong ---
    {
        int nx = 1 - aPcur;
        aVel[nx].begin();
        aPVel.begin();
        aPVel.setUniformTexture("uPos", aPos[aPcur].getTexture(), 0);
        aPVel.setUniformTexture("uVel", aVel[aPcur].getTexture(), 1);
        aPVel.setUniformTexture("uMotion", cv.motionTexture(), 2);
        aPVel.setUniform1f("uTime", t); aPVel.setUniform1f("uDt", dt);
        aPVel.setUniform1f("uPush", 2.6f); aPVel.setUniform1f("uSpring", 3.2f); aPVel.setUniform1f("uDamp", 0.93f);
        aSimQuad.draw();
        aPVel.end(); aVel[nx].end();
        aPos[nx].begin();
        aPPos.begin();
        aPPos.setUniformTexture("uPos", aPos[aPcur].getTexture(), 0);
        aPPos.setUniformTexture("uVel", aVel[nx].getTexture(), 1);
        aPPos.setUniform1f("uDt", dt);
        aSimQuad.draw();
        aPPos.end(); aPos[nx].end();
        aPcur = nx;
    }
}

void ofApp::drawWallSwarm() {
    // the visitor's live image rebuilt from thousands of coloured code glyphs; hand
    // motion swirls them (interactive), then they spring back and reform the image.
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    aPRender.begin();
    aPRender.setUniformTexture("uPos", aPos[aPcur].getTexture(), 0);
    aPRender.setUniformTexture("uCam", cv.cameraTexture(), 1);
    aPRender.setUniformTexture("uAtlas", glyphAtlas.getTexture(), 2);
    aPRender.setUniform1f("uPoint", 7.0f);
    aPRender.setUniform1f("uGrid", (float)glyphGrid);
    aPRender.setUniform1f("uBright", 1.0f);
    aPMesh.draw();
    aPRender.end();
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
}

// ---- FLOOR code cascade: the wall's data rains down, pools & drifts on pure black ----
void ofApp::initCascade() {
    ofFbo::Settings cs; cs.width = cascW; cs.height = cascH; cs.internalformat = GL_RGBA16F;
    cs.numSamples = 0; cs.useDepth = false; cs.minFilter = GL_NEAREST; cs.maxFilter = GL_NEAREST;
    cs.wrapModeHorizontal = GL_CLAMP_TO_EDGE; cs.wrapModeVertical = GL_CLAMP_TO_EDGE;
    for (int i = 0; i < 2; i++) { cascState[i].allocate(cs); cascState[i].begin(); ofClear(0,0,0,0); cascState[i].end(); }
    cascState[0].begin(); cascInit.begin(); aSimQuad.draw(); cascInit.end(); cascState[0].end();   // seed
    cascCur = 0;
    cascMesh.setMode(OF_PRIMITIVE_POINTS);
    for (int y = 0; y < cascH; y++) for (int x = 0; x < cascW; x++) {
        cascMesh.addVertex(glm::vec3(0,0,0));
        cascMesh.addTexCoord(glm::vec2((x+0.5f)/cascW, (y+0.5f)/cascH));
    }
}

void ofApp::stepCascade() {
    float dt = 1.0f/60.0f;
    float hotX[8]; int hn = 0;
    for (auto& tr : cv.tracks) { if (hn >= 8) break; hotX[hn++] = ofClamp(tr.c.x / cv.camW, 0.0f, 1.0f); }   // mirrored source -> direct
    int nx = 1 - cascCur;
    cascState[nx].begin();
    cascSim.begin();
    cascSim.setUniformTexture("uState", cascState[cascCur].getTexture(), 0);
    cascSim.setUniform1f("uTime", t);
    cascSim.setUniform1f("uDt", dt);
    cascSim.setUniform1f("uEnergy", ofClamp(energy, 0.0f, 1.0f));
    if (hn > 0) cascSim.setUniform1fv("uHotX", hotX, hn);
    cascSim.setUniform1i("uHotN", hn);
    aSimQuad.draw();
    cascSim.end();
    cascState[nx].end();
    cascCur = nx;
}

void ofApp::drawCascade() {
    ofClear(0, 0, 0, 255);                              // pure black — Ikeda precision
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    cascRender.begin();
    cascRender.setUniformTexture("uState", cascState[cascCur].getTexture(), 0);
    cascRender.setUniformTexture("uAtlas", glyphAtlas.getTexture(), 1);
    cascRender.setUniform1f("uPoint", 14.0f);
    cascRender.setUniform1f("uGrid", (float)glyphGrid);
    cascRender.setUniform1f("uBright", 1.0f);
    cascMesh.draw();
    cascRender.end();
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
}

// ===================== FLOOR: live command-line TERMINAL =====================
static ofColor termPal(int i) {
    static const ofColor P[8] = {
        ofColor(70,255,140), ofColor(0,214,224), ofColor(240,60,200), ofColor(255,205,0),
        ofColor(235,60,70),  ofColor(90,150,255), ofColor(255,120,20), ofColor(220,235,240) };
    return P[((i % 8) + 8) % 8];
}

void ofApp::initTerminal() {
    fTerm.load("fonts/mono.ttf", 16, true, true);
    termCharW = fTerm.stringWidth("0000000000") / 10.0f;      // mono advance
    termLineH = fTerm.getLineHeight() * 1.25f;
    termFull = termGenLine(); termPos = 0;
    for (int i = 0; i < termRows - 2; i++) termLines.push_back(termGenLine());   // preload scrollback
}

ofApp::TermLine ofApp::termGenLine() {
    TermLine L;
    ofColor green(70,255,140), dim(40,150,90), cyan(0,214,224), mag(240,60,200),
            yel(255,205,0), red(235,60,70), wht(220,235,240);
    auto add = [&](const std::string& s, ofColor c){ for (char ch : s){ L.text += ch; L.col.push_back(c); } };
    auto isOp = [](char c){ return c=='+'||c=='-'||c=='*'||c=='/'||c=='='||c=='<'||c=='>'||c=='&'||c=='|'; };
    auto isBr = [](char c){ return c=='('||c==')'||c=='{'||c=='}'||c=='['||c==']'||c==';'; };
    int seq = termSeq++;
    int cols = std::max(20, (int)((FLOOR_W - 48) / termCharW));
    int r = (int)ofRandom(100);

    if (r < 14) {                                             // full-width DATA BLOCK row (block-block colour)
        L.block = true;
        for (int i = 0; i < cols; i++) {
            float m = ofNoise(i * 0.16f, seq * 0.7f, t * 0.2f);
            if (m < 0.34f) L.col.push_back(ofColor(0,0,0,0));
            else L.col.push_back(termPal((int)(m * 8) + i / 3));
        }
        return L;
    }
    if (r < 24) {                                             // shell command
        add("$ ", green);
        const char* cmds[] = { "./jaryan --scan --subjects=", "analyze --tensor volumetric --depth=",
            "trace subject.mesh --lod=", "solve pose --iter=", "grep -r person /dev/vision --conf=",
            "render --field curl --seed=", "hash --node p*7.0 --salt=" };
        add(cmds[seq % 7], wht); add(ofToString((int)ofRandom(2,999)), cyan);
        return L;
    }
    if (r < 40) {                                             // hex dump
        char h[16]; snprintf(h, sizeof(h), "0x%06X", (unsigned)ofRandom(0, 0xFFFFFF));
        add(std::string(h) + "  ", dim);
        int nb = std::min(cols / 3 - 6, 20);
        for (int i = 0; i < nb; i++) {
            char b[4]; snprintf(b, sizeof(b), "%02X", (int)ofRandom(0, 255));
            add(std::string(b) + " ", (ofRandom(1) < 0.18f) ? termPal((int)ofRandom(8)) : dim);
        }
        return L;
    }
    if (r < 54) {                                             // log line with status token
        bool ok = ofRandom(1) < 0.72f;
        add("[", dim); add(ok ? "OK " : "!! ", ok ? green : red); add("] ", dim);
        const char* msgs[] = { "tensor allocated ", "node bound ", "silhouette locked ", "curl field step ",
            "mesh decimated ", "cluster merged ", "vector solved ", "frame committed " };
        add(msgs[seq % 8], green);
        char b[28]; snprintf(b, sizeof(b), "0x%04X dims=(%d,%d)", (int)ofRandom(0,0xFFFF), (int)ofRandom(8,64), (int)ofRandom(8,64));
        add(b, cyan);
        return L;
    }
    if (r < 66) {                                             // code snippet (syntax coloured)
        const char* code[] = {
            "for(int i=0;i<n;i++){ node[i]=hash(p*7.0); }",
            "vec2 curl(vec2 p){ return grad(noise(p)); }",
            "if(conf>0.45 && cls==15){ track(box); }",
            "mat[j][k] += w * phi(x - c[k]);",
            "while(!queue.empty()){ solve(queue.pop()); }",
            "float d = length(p - centroid) / scale;" };
        std::string s = code[seq % 6];
        for (char ch : s) {
            ofColor c = green;
            if (ch >= '0' && ch <= '9') c = yel; else if (isBr(ch)) c = dim; else if (isOp(ch)) c = mag;
            L.text += ch; L.col.push_back(c);
        }
        return L;
    }
    if (r < 80 && !cv.tracks.empty()) {                       // LIVE telemetry — the person's own data
        auto& tr = cv.tracks[(size_t)ofRandom(cv.tracks.size())];
        add("SUBJECT ", wht); add(ofToString(tr.id % 1000, 3, '0'), mag);
        add(" :: X", green); add(ofToString(std::max(0,(int)tr.c.x),4,'0'), cyan);
        add(" Y", green);    add(ofToString(std::max(0,(int)tr.c.y),4,'0'), cyan);
        add(" V", green);    add(ofToString(std::min(999,(int)tr.speed),3,'0'), yel);
        add(" C0.", green);  add(ofToString((int)(80+ofRandom(19)),2,'0'), termPal(5));
        return L;
    }
    int nbit = std::min(cols, 96);                            // binary matrix row
    for (int i = 0; i < nbit; i++) {
        char c = (ofRandom(1) < 0.5f) ? '1' : '0';
        ofColor col = (c == '1') ? green : dim;
        if (ofRandom(1) < 0.05f) col = termPal((int)ofRandom(8));
        L.text += c; L.col.push_back(col);
        if (i % 4 == 3) { L.text += ' '; L.col.push_back(dim); }
    }
    return L;
}

void ofApp::updateTerminal(float dt) {
    floorGlitch = std::max(0.0f, floorGlitch - dt * 2.2f);
    termGather  = std::max(0.0f, termGather  - dt * 0.9f);
    termHolo    = std::max(0.0f, termHolo    - dt / 6.5f);
    termShape   = std::max(0.0f, termShape   - dt / 5.2f);
    termBurst   = std::max(0.0f, termBurst   - dt / 2.2f);
    termUplink  = std::max(0.0f, termUplink  - dt / 5.5f);
    termAscii   = std::max(0.0f, termAscii   - dt / termAsciiDur);
    termEvtTagT = std::max(0.0f, termEvtTagT - dt);
    termHiT     = std::max(0.0f, termHiT - dt);
    termHiNext -= dt;
    if (termHiNext <= 0) {                                     // a random line suddenly gets highlighted
        termHiNext = ofRandom(3.0f, 7.0f);
        if (!termLines.empty()) {
            termHiRow = (int)ofRandom((int)termLines.size());
            termHiCol = termPal((int)ofRandom(8));
            termHiT = ofRandom(1.0f, 1.9f);
        }
    }
    if (termSpeedMode != 0) { termSpeedT -= dt; if (termSpeedT <= 0) termSpeedMode = 0; }
    float spdTarget = (termSpeedMode == 1) ? 7.0f : (termSpeedMode == 2) ? 0.15f : 1.0f;
    termSpeedMul += (spdTarget - termSpeedMul) * std::min(1.0f, dt * 9.0f);
    if (termSpeedMode == 1) floorGlitch = std::max(floorGlitch, 0.22f);   // overdrive shimmers

    termEvT += dt * (1.0f + 1.7f * ofClamp(energy, 0, 1));   // the visitors' motion accelerates the schedule
    if (termEvT > termEvNext) { termFireEvent(); termEvT = 0; termEvNext = ofRandom(4.5f, 9.0f); }
    if (ofRandom(1) < 0.02f + energy * 0.06f) floorGlitch = std::max(floorGlitch, ofRandom(0.3f, 0.7f));

    float speed = (42.0f + energy * 90.0f) * termSpeedMul * pTermSpeed;   // motion + clock-kicks + panel slider
    termAcc += dt * speed;
    int guard = 0;
    while (termAcc >= 1.0f && guard++ < 400) {
        if (termFull.block) {                                // data blocks dump in one shot
            termLines.push_back(termFull);
            if ((int)termLines.size() > termRows - 1) termLines.pop_front();
            termFull = termGenLine(); termPos = 0; termAcc -= 1.0f; continue;
        }
        if (termPos < termFull.text.size()) { termPos++; termAcc -= 1.0f; }
        else {
            termLines.push_back(termFull);
            if ((int)termLines.size() > termRows - 1) termLines.pop_front();
            termFull = termGenLine(); termPos = 0; termAcc -= 0.5f;
        }
    }
}

void ofApp::drawTermLine(const TermLine& L, float x, float y, int upto) {
    if (L.block) {                                           // a row of coloured data blocks
        for (size_t i = 0; i < L.col.size(); i++) {
            if (L.col[i].a == 0) continue;
            ofSetColor(L.col[i]);
            ofDrawRectangle(x + i * termCharW, y - termLineH * 0.72f, termCharW - 1.5f, termLineH * 0.62f);
        }
        return;
    }
    int n = (upto < 0) ? (int)L.text.size() : std::min(upto, (int)L.text.size());
    int i = 0;
    while (i < n) {                                          // draw runs of equal colour (mono keeps alignment)
        ofColor c = L.col[i]; int j = i; std::string run;
        while (j < n && L.col[j] == c) { run += L.text[j]; j++; }
        ofSetColor(c); fTerm.drawString(run, x + i * termCharW, y);
        i = j;
    }
}

void ofApp::drawTerminal() {
    ofClear(0, 0, 0, 255);
    float x0 = 24, y0 = termLineH + 6;
    float g = termGather;

    drawTermShapes();                                        // behind the text: sudden wireframe geometry

    ofPushMatrix();
    if (g > 0.001f) {                                        // GATHER/COLLAPSE — text clumps inward + wobbles
        float cx = FLOOR_W * 0.5f, cy = FLOOR_H * 0.5f;
        ofTranslate(cx, cy);
        ofScale(1.0f - 0.45f * g, 1.0f - 0.18f * g);
        ofRotateDeg(sinf(t * 3.2f) * 5.0f * g);
        ofTranslate(-cx, -cy);
    }
    int row = 0;
    float wg = floorGlitch;
    if (termHiT > 0 && termHiRow >= 0 && termHiRow < (int)termLines.size()) {   // random line HIGHLIGHT
        float hyy = y0 + termHiRow * termLineH;
        float ha = ofClamp(termHiT, 0, 1);
        ofFill(); ofSetColor(termHiCol, (int)(54 * ha));
        ofDrawRectangle(x0 - 6, hyy - termLineH * 0.78f, FLOOR_W - 44, termLineH * 0.98f);
        ofNoFill(); ofSetLineWidth(1.0f); ofSetColor(termHiCol, (int)(135 * ha));
        ofDrawRectangle(x0 - 6, hyy - termLineH * 0.78f, FLOOR_W - 44, termLineH * 0.98f);
        ofSetColor(termHiCol, (int)(225 * ha)); fTiny.drawString(">>", x0 - 22, hyy);
    }
    for (auto& L : termLines) {                              // glitch: rows slide like a data wave
        float xo = sinf(row * 0.55f + t * 9.0f) * 24.0f * wg;
        drawTermLine(L, x0 + xo, y0 + row * termLineH, -1); row++;
    }
    float cxo = sinf(row * 0.55f + t * 9.0f) * 24.0f * wg;
    drawTermLine(termFull, x0 + cxo, y0 + row * termLineH, (int)termPos);     // the line being typed
    if (!termFull.block && fmodf(t, 0.8f) < 0.45f) {                          // blinking cursor
        ofSetColor(termSpeedMode == 2 ? ofColor(0, 214, 224) : ofColor(120, 255, 150));
        ofDrawRectangle(x0 + cxo + termPos * termCharW + 1, y0 + row * termLineH - termLineH * 0.72f, termCharW * 0.85f, termLineH * 0.66f);
    }
    ofPopMatrix();

    drawTermUplink();                                        // the visitor's photo drops in + beams up
    drawTermAscii();                                         // ASCII torus / dot-matrix marquee
    drawTermBurst();                                         // expanding rings of data ticks
    drawTermHolo();                                          // the visitor rebuilt inside the terminal

    ofSetColor(0, 0, 0, 55);                                 // subtle scanlines (CRT ambiance)
    for (float yy = 0; yy < FLOOR_H; yy += 3) ofDrawRectangle(0, yy, FLOOR_W, 1);

    ofSetColor(70, 255, 140);                                // header chrome
    fLabel.drawString("JARYAN.SYS // TERMINAL", 24, 24);
    std::string tag; ofColor tc;
    if (termSpeedMode != 0) {                                // clock-kick tag stays while active
        tag = (termSpeedMode == 1) ? ">> OVERDRIVE x7.0" : ">> STASIS x0.15";
        tc = (termSpeedMode == 1) ? ofColor(255, 205, 0) : ofColor(0, 214, 224);
    } else if (termEvtTagT > 0 && !termEvtTag.empty()) {
        tag = ">> " + termEvtTag; tc = ofColor(240, 60, 200);
        tc.a = (int)(255 * ofClamp(termEvtTagT / 0.7f, 0, 1));
    }
    if (!tag.empty()) {
        ofRectangle bb = fLabel.getStringBoundingBox(tag, 0, 0);
        ofSetColor(tc); fLabel.drawString(tag, FLOOR_W - bb.width - 24, 24);
    }
    ofSetColor(40, 150, 90);                                 // footer: live clock-speed readout
    fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0')
                     + "   CLK x" + ofToString(termSpeedMul, 2)
                     + "   PID " + ofToString(ofGetFrameNum() % 1000000, 6, '0'), 24, FLOOR_H - 14);
}

void ofApp::termFireEvent() {
    // the machine's sudden happenings: glitch · gather · CLOCK KICK · geo-shape · hologram · UPLINK · data burst
    int r = (int)ofRandom(100);
    if      (r < 10) { floorGlitch = ofRandom(0.75f, 1.0f); termEvtTag = "GLITCH.WAVE"; }
    else if (r < 26) { termGather = 1.0f; termEvtTag = "GATHER.COLLAPSE"; }
    else if (r < 42) {                                     // CLOCK KICK — typing suddenly races or freezes
        if (ofRandom(1) < 0.6f) { termSpeedMode = 1; termSpeedT = ofRandom(1.4f, 2.8f); termEvtTag = "OVERDRIVE x7.0"; }
        else                    { termSpeedMode = 2; termSpeedT = ofRandom(1.8f, 3.4f); termEvtTag = "STASIS x0.15"; }
    }
    else if (r < 54) {
        termShape = 1.0f; termShapeKind = (int)ofRandom(3);
        termEvtTag = std::string("GEO.INJECT::") + (termShapeKind == 0 ? "CUBE.PRIME" : termShapeKind == 1 ? "OCTA.CORE" : "TORUS.FIELD");
    }
    else if (r < 64) {
        if (!cv.tracks.empty()) { termHolo = 1.0f; termEvtTag = "SUBJECT.HOLOGRAM"; }
        else { termShape = 1.0f; termShapeKind = (int)ofRandom(3); termEvtTag = "GEO.INJECT"; }
    }
    else if (r < 76) {
        if (!cv.tracks.empty()) { termUplink = 1.0f; termEvtTag = "SUBJECT.UPLINK"; }
        else { termBurst = 1.0f; termEvtTag = "DATA.BURST"; }
    }
    else if (r < 90) {                                     // ASCII.INJECT — the terminal shows pure ASCII art
        termAscii = 1.0f; termAsciiCol = termPal((int)ofRandom(8));
        if (ofRandom(1) < 0.55f) { termAsciiKind = 0; termAsciiDur = 6.5f; termEvtTag = "ASCII.INJECT::TORUS"; }
        else {
            termAsciiKind = 1; termAsciiDur = 5.5f;
            static const char* MSGS[] = { "DATA.JARYAN", "HUMAN?", "I SEE YOU", "SCAN COMPLETE",
                                          "SUBJECT LOCKED", "MACHINE VISION", "NO ESCAPE", "JARYAN.SYS" };
            termAsciiMsg = MSGS[(int)ofRandom(8)];
            termEvtTag = "ASCII.INJECT::MARQUEE";
        }
    }
    else {
        termBurst = 1.0f; termEvtTag = "DATA.BURST";
        if (!cv.tracks.empty()) {                          // the burst erupts from the visitor's projected spot
            const CvTrack& tr = cv.tracks[0];
            termBurstC = glm::vec2((1.0f - tr.c.x / cv.camW) * FLOOR_W, (tr.c.y / cv.camH) * FLOOR_H);
        } else termBurstC = glm::vec2(ofRandom(0.2f, 0.8f) * FLOOR_W, ofRandom(0.25f, 0.7f) * FLOOR_H);
    }
    termEvtTagT = 2.4f;
    floorGlitch = std::max(floorGlitch, 0.25f);            // every event lands with a visible kick
}

void ofApp::drawTermShapes() {
    if (termShape <= 0.002f) return;
    float env = powf(sinf(termShape * PI), 0.45f);           // fade in, stay bright, fade out
    float cx = FLOOR_W * 0.5f, cy = FLOOR_H * 0.47f;
    float R = 275.0f + 26.0f * sinf(t * 1.3f);
    float ang = t * (0.7f + 1.6f * fabsf(sinf(t * 0.47f)));  // spin speed keeps changing
    float cb = cosf(ang * 0.63f + 0.4f), sb = sinf(ang * 0.63f + 0.4f);
    auto P3 = [&](glm::vec3 v) {
        float ca = cosf(ang), sa = sinf(ang);
        float x = v.x * ca + v.z * sa, z = -v.x * sa + v.z * ca;
        float y = v.y * cb - z * sb; z = v.y * sb + z * cb;
        float pers = 1.0f / (1.0f - z * 0.35f);
        return glm::vec2(cx + x * pers * R, cy + y * pers * R);
    };
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofNoFill(); ofSetLineWidth(1.2f);
    ofColor c0(40, 230, 130), c1(0, 190, 220);
    if (termShapeKind == 0) {                                // CUBE.PRIME — 12 edges
        glm::vec3 V[8];
        int E[12][2] = {{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
        for (int i = 0; i < 8; i++)
            V[i] = glm::vec3((i & 1 ? 0.62f : -0.62f), (i & 2 ? 0.62f : -0.62f), (i & 4 ? 0.62f : -0.62f));
        for (int i = 0; i < 12; i++) {
            glm::vec2 a = P3(V[E[i][0]]), b = P3(V[E[i][1]]);
            ofSetColor(i % 2 ? c0 : c1, (int)(200 * env));
            ofDrawLine(a.x, a.y, b.x, b.y);
        }
    } else if (termShapeKind == 1) {                         // OCTA.CORE
        glm::vec3 V[6] = { {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
        for (int i = 0; i < 6; i++) for (int j = i + 1; j < 6; j++) {
            if (j == (i ^ 1)) continue;                      // skip opposite pairs
            glm::vec2 a = P3(V[i] * 0.85f), b = P3(V[j] * 0.85f);
            ofSetColor((i + j) % 2 ? c0 : c1, (int)(190 * env));
            ofDrawLine(a.x, a.y, b.x, b.y);
        }
    } else {                                                 // TORUS.FIELD — three rotating rings + spokes
        for (int k = 0; k < 3; k++) {
            float rr = 0.85f - 0.22f * k;
            ofBeginShape();
            for (int i = 0; i <= 48; i++) {
                float a2 = i / 48.0f * TWO_PI;
                glm::vec3 v = (k == 2) ? glm::vec3(cosf(a2) * rr, 0, sinf(a2) * rr)
                                       : glm::vec3(cosf(a2) * rr, sinf(a2) * rr * (k == 1 ? 0.25f : 1.0f), 0);
                glm::vec2 p = P3(v);
                ofVertex(p.x, p.y);
            }
            ofEndShape(false);
        }
        for (int i = 0; i < 24; i++) {
            float a2 = i / 24.0f * TWO_PI + t * 0.8f;
            glm::vec2 p1 = P3(glm::vec3(cosf(a2) * 0.85f, sinf(a2) * 0.85f, 0));
            glm::vec2 p2 = P3(glm::vec3(cosf(a2) * 0.95f, sinf(a2) * 0.95f, 0));
            ofSetColor(c1, (int)(120 * env));
            ofDrawLine(p1.x, p1.y, p2.x, p2.y);
        }
    }
    ofSetColor(70, 255, 140, (int)(200 * env));
    fTiny.drawString(std::string("GEO.INJECT :: ") +
                     (termShapeKind == 0 ? "CUBE.PRIME" : termShapeKind == 1 ? "OCTA.CORE" : "TORUS.FIELD"),
                     cx - 70, cy + R + 26);
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofPopStyle();
}

// ---- the visitor's silhouette rebuilt as a flickering character-hologram inside the terminal ----
void ofApp::drawTermHolo() {
    if (termHolo <= 0.004f) return;
    ofPixels& sil = cv.silhouettePixels();
    int cw = cv.camW, chh = cv.camH;
    bool ok = (sil.getData() != nullptr && (int)sil.getWidth() == cw && (int)sil.getHeight() == chh);
    if (!ok) return;
    float hw = 540.0f, hh = 320.0f, hx = (FLOOR_W - hw) * 0.5f, hy = FLOOR_H * 0.13f;
    // crop to the subjects' union bbox so the hologram is BIG — it is a hologram OF them
    float ux0 = 0, uy0 = 0, ux1 = 1, uy1 = 1;
    if (!cv.tracks.empty()) {
        ux0 = uy0 = 1; ux1 = uy1 = 0; bool any = false;
        for (auto& tr : cv.tracks) {
            float x0 = ofClamp(tr.bbox.x / cw, 0, 1),            y0 = ofClamp(tr.bbox.y / chh, 0, 1);
            float x1 = ofClamp((tr.bbox.x + tr.bbox.width) / cw, 0, 1), y1 = ofClamp((tr.bbox.y + tr.bbox.height) / chh, 0, 1);
            if (x1 - x0 < 0.03f || y1 - y0 < 0.03f) continue;
            ux0 = std::min(ux0, x0); uy0 = std::min(uy0, y0);
            ux1 = std::max(ux1, x1); uy1 = std::max(uy1, y1); any = true;
        }
        if (!any || ux1 - ux0 < 0.06f || uy1 - uy0 < 0.06f) { ux0 = 0; uy0 = 0; ux1 = 1; uy1 = 1; }
        float mx = (ux1 - ux0) * 0.10f, my = (uy1 - uy0) * 0.10f;   // breathing margin
        ux0 = std::max(0.0f, ux0 - mx); uy0 = std::max(0.0f, uy0 - my);
        ux1 = std::min(1.0f, ux1 + mx); uy1 = std::min(1.0f, uy1 + my);
    }
    float env = ofClamp(termHolo * 3.5f, 0, 1);                                  // holds bright, fades at the end
    float fl = 0.82f + 0.18f * sinf(t * 23.0f) * sinf(t * 7.3f);                 // hologram flicker
    float reveal = ofClamp((1.0f - termHolo) / 0.12f, 0, 1) * (hh + 60.0f);      // sweeps in top->bottom
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    int GX = 74, GY = 44;
    for (int gy = 0; gy < GY; gy++) for (int gx = 0; gx < GX; gx++) {
        float u = ux0 + (gx + 0.5f) / GX * (ux1 - ux0);
        float v = uy0 + (gy + 0.5f) / GY * (uy1 - uy0);
        float s = sil[(int)(v * chh) * cw + (int)(u * cw)] / 255.0f;
        if (s < 0.22f) continue;
        float x = hx + u * hw, y = hy + v * hh;
        if (y - hy > reveal) continue;
        if (ofRandom(1) < 0.05f) continue;                                       // dropout
        ofColor c = ofColor(80, 255, 150).getLerped(ofColor(0, 215, 255), s);
        ofSetColor(c, (int)(185 * sqrtf(s) * fl * env));
        ofDrawRectangle(x, y, hw / GX - 1.5f, hh / GY * 1.55f);
    }
    float syl = hy + fmodf(t * 150.0f, hh);                                      // scan line through the body
    ofSetColor(160, 255, 210, (int)(70 * env));
    ofDrawRectangle(hx, syl, hw, 3);
    ofNoFill(); ofSetLineWidth(1.2f); ofSetColor(70, 255, 140, (int)(190 * env));
    float L = 16;
    ofDrawLine(hx, hy, hx + L, hy);           ofDrawLine(hx, hy, hx, hy + L);
    ofDrawLine(hx + hw, hy, hx + hw - L, hy); ofDrawLine(hx + hw, hy, hx + hw, hy + L);
    ofDrawLine(hx, hy + hh, hx + L, hy + hh); ofDrawLine(hx, hy + hh, hx, hy + hh - L);
    ofDrawLine(hx + hw, hy + hh, hx + hw - L, hy + hh); ofDrawLine(hx + hw, hy + hh, hx + hw, hy + hh - L);
    std::string lab = "SUBJECT.HOLOGRAM";
    if (!cv.tracks.empty()) lab += "  ID:" + ofToString(cv.tracks[0].id % 1000, 3, '0');
    ofSetColor(70, 255, 140, (int)(220 * env));
    fTiny.drawString(lab, hx, hy + hh + 18);
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofPopStyle();
}

// ---- SUBJECT.UPLINK: the person's photo drops into the terminal, then beams a live link up ----
void ofApp::drawTermUplink() {
    if (termUplink <= 0.003f || cv.tracks.empty()) return;
    const CvTrack& tr = cv.tracks[0];
    float frac = ofClamp((1.0f - termUplink) / 0.16f, 0, 1);     // the fall takes the first ~0.9 s
    float e = 1.0f - powf(1.0f - frac, 3.0f);                    // ease-out landing
    float cw2 = 300.0f, ch2 = 380.0f, cx = FLOOR_W * 0.5f;
    float cy = ofLerp(-ch2 * 0.7f, FLOOR_H * 0.56f, e);
    glm::vec4 uv = camUv(tr);
    float v1 = uv.y + (uv.w - uv.y) * 0.62f;                     // head + torso crop — unmistakably THEM
    ofSetColor(6, 8, 10, 235);
    ofDrawRectangle(cx - cw2 * 0.5f - 8, cy - 26, cw2 + 16, ch2 + 40);
    drawMachine(ofRectangle(cx - cw2 * 0.5f, cy, cw2, ch2),
                glm::vec2(uv.x, uv.y), glm::vec2(uv.z, v1),
                glm::vec2(cw2 / 3.0f, ch2 / 3.0f), ofColor(255), 1.0f, 0.22f);
    ofNoFill(); ofSetLineWidth(1.3f); ofSetColor(70, 255, 140);
    ofDrawRectangle(cx - cw2 * 0.5f, cy, cw2, ch2);
    float L = 14, x0 = cx - cw2 * 0.5f, x1 = cx + cw2 * 0.5f;    // corner ticks
    ofDrawLine(x0, cy, x0 + L, cy); ofDrawLine(x0, cy, x0, cy + L);
    ofDrawLine(x1, cy, x1 - L, cy); ofDrawLine(x1, cy, x1, cy + L);
    ofSetColor(70, 255, 140);
    fTiny.drawString("SUBJECT.CAPTURED  ID:" + ofToString(tr.id % 1000, 3, '0') + "  UPLINK."
                     + std::string(frac >= 1.0f ? "ACTIVE" : "OPEN"),
                     x0, cy + ch2 + 20);
    if (frac > 0.9f && frac < 1.06f) {                           // landing flash
        float fa = 1.0f - fabsf(frac - 0.98f) * 9.0f;
        if (fa > 0) { ofEnableBlendMode(OF_BLENDMODE_ADD); ofSetColor(160, 255, 200, (int)(150 * fa));
                      ofDrawRectangle(x0 - 4, cy - 4, cw2 + 8, ch2 + 8); ofEnableBlendMode(OF_BLENDMODE_ALPHA); }
    }
    if (frac >= 1.0f) {                                          // slim beam from the photo up to the wall edge
        glm::vec2 base(cx, cy), top(cx, 0);
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        ofSetLineWidth(1.2f);  ofSetColor(150, 255, 195, 120); ofDrawLine(base.x, base.y, top.x, top.y);
        ofFill();
        for (int i = 0; i < 3; i++) {                            // small data packets rising
            float p = fmodf(t * 0.9f + i * 0.33f, 1.0f);
            float px = ofLerp(base.x, top.x, p), py = ofLerp(base.y, top.y, p);
            ofSetColor(195, 255, 220, 150); ofDrawRectangle(px - 2.0f, py - 5, 4, 10);
        }
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    }
}

// ---- ASCII.INJECT: a live spinning ASCII torus, or a dot-matrix marquee message ----
static const uint8_t* dotGlyph(char c) {                     // 5x5 dot-matrix font (5-bit rows, MSB left)
    static const uint8_t G[][5] = {
        {0x0E,0x11,0x1F,0x11,0x11},                          // A
        {0x1E,0x11,0x1E,0x11,0x1E},                          // B
        {0x0F,0x10,0x10,0x10,0x0F},                          // C
        {0x1E,0x11,0x11,0x11,0x1E},                          // D
        {0x1F,0x10,0x1E,0x10,0x1F},                          // E
        {0x11,0x11,0x1F,0x11,0x11},                          // H
        {0x1F,0x04,0x04,0x04,0x1F},                          // I
        {0x07,0x02,0x02,0x12,0x0C},                          // J
        {0x11,0x12,0x1C,0x12,0x11},                          // K
        {0x10,0x10,0x10,0x10,0x1F},                          // L
        {0x11,0x1B,0x15,0x11,0x11},                          // M
        {0x11,0x19,0x15,0x13,0x11},                          // N
        {0x0E,0x11,0x11,0x11,0x0E},                          // O
        {0x1E,0x11,0x1E,0x10,0x10},                          // P
        {0x1E,0x11,0x1E,0x12,0x11},                          // R
        {0x0F,0x10,0x0E,0x01,0x1E},                          // S
        {0x1F,0x04,0x04,0x04,0x04},                          // T
        {0x11,0x11,0x11,0x11,0x0E},                          // U
        {0x11,0x11,0x11,0x0A,0x04},                          // V
        {0x11,0x0A,0x04,0x04,0x04},                          // Y
        {0x0E,0x11,0x02,0x00,0x04},                          // ?
        {0x00,0x00,0x00,0x00,0x04},                          // .
        {0x04,0x04,0x04,0x00,0x04},                          // !
        {0x00,0x00,0x00,0x00,0x00} };                        // space
    c = (char)ofToUpper(std::string(1, c))[0];
    switch (c) {
        case 'A': return G[0];  case 'B': return G[1];  case 'C': return G[2];  case 'D': return G[3];
        case 'E': return G[4];  case 'H': return G[5];  case 'I': return G[6];  case 'J': return G[7];
        case 'K': return G[8];  case 'L': return G[9];  case 'M': return G[10]; case 'N': return G[11];
        case 'O': return G[12]; case 'P': return G[13]; case 'R': return G[14]; case 'S': return G[15];
        case 'T': return G[16]; case 'U': return G[17]; case 'V': return G[18]; case 'Y': return G[19];
        case '?': return G[20]; case '.': return G[21]; case '!': return G[22]; default: return G[23];
    }
}

void ofApp::drawTermAscii() {
    if (termAscii <= 0.004f) return;
    float elapsed = 1.0f - termAscii;                        // 0 -> 1 over the event
    float env = ofClamp(std::min(elapsed * 9.0f, termAscii * 4.0f), 0, 1);
    ofPushStyle();
    if (termAsciiKind == 0) {
        // ---- the legendary spinning ASCII torus, rendered live in characters ----
        const int COLS = 70, ROWS = 30;
        static char chars[70 * 30]; static float zbuf[70 * 30];
        memset(chars, 0, sizeof(chars)); memset(zbuf, 0, sizeof(zbuf));
        const float R1 = 1.0f, R2 = 2.0f, K2 = 5.0f, K1 = 44.0f;
        const float A = t * 1.15f, B = t * 0.62f;
        const float cA = cosf(A), sA = sinf(A), cB = cosf(B), sB = sinf(B);
        static const char* RAMP = ".,-~:;=!*#$@";
        for (float th = 0; th < TWO_PI; th += 0.07f) {
            float ct = cosf(th), st = sinf(th);
            float circlex = R2 + R1 * ct, circley = R1 * st;
            for (float ph = 0; ph < TWO_PI; ph += 0.02f) {
                float cp = cosf(ph), sp = sinf(ph);
                float x = circlex * (cB * cp + sA * st * sB) - circley * cA * sB;
                float y = circlex * (sB * cp - sA * st * cB) + circley * cA * cp;
                float z = K2 + cA * circlex * st + circley * sA;
                float ooz = 1.0f / z;
                int xp = (int)(COLS / 2.0f + K1 * ooz * x);
                int yp = (int)(ROWS / 2.0f - K1 * ooz * y * 0.5f);
                if (xp < 0 || xp >= COLS || yp < 0 || yp >= ROWS) continue;
                float L = cp * ct * sB - cA * ct * sp - sA * st + cB * (cA * st - ct * sA * sp);
                if (L > 0 && ooz > zbuf[yp * COLS + xp]) {
                    zbuf[yp * COLS + xp] = ooz;
                    chars[yp * COLS + xp] = RAMP[std::min(11, (int)(L * 8.0f))];
                }
            }
        }
        float cs = 5.4f, lh = 11.0f;
        float pw = COLS * cs + 40.0f, phh = ROWS * lh + 44.0f;
        float px = (FLOOR_W - pw) * 0.5f, py = (FLOOR_H - phh) * 0.42f;
        ofSetColor(4, 6, 8, (int)(225 * env)); ofFill();
        ofDrawRectangle(px, py, pw, phh);
        ofNoFill(); ofSetLineWidth(1.0f); ofSetColor(termAsciiCol, (int)(150 * env));
        ofDrawRectangle(px, py, pw, phh);
        ofFill();
        for (int rrow = 0; rrow < ROWS; rrow++) {            // draw colour-runs per row (cheap)
            int cc = 0;
            while (cc < COLS) {
                if (chars[rrow * COLS + cc] == 0) { cc++; continue; }
                char lvl = chars[rrow * COLS + cc];
                int start = cc;
                while (cc < COLS && chars[rrow * COLS + cc] == lvl) cc++;
                std::string run(chars + rrow * COLS + start, chars + rrow * COLS + cc);
                float b = 0.30f + 0.70f * (lvl / 11.0f);
                ofColor col = ofColor(40, 255, 140).getLerped(ofColor(220, 255, 235), b);
                ofSetColor(col, (int)(235 * env));
                fBin.drawString(run, px + 20 + start * cs, py + 26 + rrow * lh);
            }
        }
        ofSetColor(termAsciiCol, (int)(190 * env));
        fTiny.drawString("ASCII.INJECT::TORUS.FIELD  SPIN " + ofToString(fmodf(t * 18.3f, 360.0f), 3, '0') + "DEG", px + 20, py + phh - 8);
    } else {
        // ---- dot-matrix marquee: the machine posts a message in pure pixels ----
        int cell = 9, gap = 6;
        int msgCols = (int)termAsciiMsg.size() * gap - 1;
        float pw = msgCols * cell + 56.0f, phh = 5 * cell + 62.0f;
        float px = (FLOOR_W - pw) * 0.5f, py = (FLOOR_H - phh) * 0.40f;
        ofSetColor(4, 6, 8, (int)(228 * env)); ofFill();
        ofDrawRectangle(px, py, pw, phh);
        ofNoFill(); ofSetLineWidth(1.0f); ofSetColor(termAsciiCol, (int)(160 * env));
        ofDrawRectangle(px, py, pw, phh);
        ofFill();
        int revRows = (int)(ofClamp(elapsed * 5.0f, 0, 1) * 5.0f + 0.999f);   // rows type in fast
        for (int chI = 0; chI < (int)termAsciiMsg.size(); chI++) {
            const uint8_t* g = dotGlyph(termAsciiMsg[chI]);
            for (int rr = 0; rr < std::min(5, revRows); rr++) for (int cq = 0; cq < 5; cq++) {
                if (!((g[rr] >> (4 - cq)) & 1)) continue;
                if (ofRandom(1) < 0.025f) continue;                           // dead pixels
                float wav = 0.7f + 0.3f * sinf((chI * 5 + cq) * 0.5f - t * 7.0f);
                ofSetColor(termAsciiCol, (int)(225 * wav * env));
                ofDrawRectangle(px + 28 + (chI * gap + cq) * cell, py + 24 + rr * cell, cell - 1.6f, cell - 1.6f);
            }
        }
        ofSetColor(termAsciiCol, (int)(190 * env));
        fTiny.drawString("ASCII.INJECT::MARQUEE // " + termAsciiMsg, px + 28, py + phh - 10);
    }
    ofPopStyle();
}

// ---- expanding rings of data ticks (from the visitor's projected position or random) ----
void ofApp::drawTermBurst() {
    if (termBurst <= 0.004f) return;
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofSetLineWidth(1.3f);
    float cx = termBurstC.x, cy = termBurstC.y;
    for (int k = 0; k < 3; k++) {
        float rad = (1.0f - termBurst) * (170.0f + k * 130.0f);
        if (rad < 6) continue;
        float al = termBurst * (1.0f - k * 0.24f);
        int nt = 40 + k * 8;
        for (int i = 0; i < nt; i++) {
            float a = i * TWO_PI / nt + k * 0.3f + t * (0.4f + 0.2f * k);
            float len = 5.0f + 5.0f * ((i + k) % 4 == 0 ? 1.0f : 0.3f);
            float x0 = cx + cosf(a) * rad,         y0 = cy + sinf(a) * rad * 0.8f;
            float x1 = cx + cosf(a) * (rad + len), y1 = cy + sinf(a) * (rad + len) * 0.8f;
            ofSetColor(termPal((i + k * 3) % 8), (int)(235 * al));
            ofDrawLine(x0, y0, x1, y1);
        }
    }
    ofSetColor(255, (int)(160 * termBurst)); ofDrawCircle(cx, cy, 3);
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofPopStyle();
}

void ofApp::drawFloorTex(float x, float y, float w, float h) {
    float amt = ofClamp(floorGlitch + termGather * 0.7f + energy * 0.10f + pGlitch, 0.0f, 1.0f);
    glitchPost.begin();
    glitchPost.setUniformTexture("uCol", floorFbo.getTexture(), 0);
    glitchPost.setUniform2f("uRes", FLOOR_W, FLOOR_H);
    glitchPost.setUniform1f("uTime", t);
    glitchPost.setUniform1f("uAmt", amt);
    glitchPost.setUniform1f("uBright", pBright);
    ofSetColor(255);
    floorFbo.draw(x, y, w, h);
    glitchPost.end();
}

void ofApp::floorFlowFluid() {
    ofClear(4,5,9,255);
    ofSetColor(255); aInk[aInkCur].draw(0, 0, rw, rh);
    ofSetColor(235); fLabel.drawString("FLOW.FLUID // ACID", 70, 96);
    ofSetColor(90,94,100); fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0'), 70, 120);
}

void ofApp::floorParticles() {
    ofClear(6,7,11,255);
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    aPRender.begin();
    aPRender.setUniformTexture("uPos", aPos[aPcur].getTexture(), 0);
    aPRender.setUniformTexture("uCam", cv.cameraTexture(), 1);
    aPRender.setUniformTexture("uAtlas", glyphAtlas.getTexture(), 2);
    aPRender.setUniform1f("uPoint", 9.0f);
    aPRender.setUniform1f("uGrid", (float)glyphGrid);
    aPRender.setUniform1f("uBright", 1.0f);
    aPMesh.draw();
    aPRender.end();
    ofSetColor(235); fLabel.drawString("SWARM // ACID", 70, 96);
    ofSetColor(90,94,100); fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0'), 70, 120);
}

void ofApp::floorCodeRain() {
    ofClear(0,0,0,255);
    aCodeRain.begin();
    aCodeRain.setUniformTexture("uCam", cv.cameraTexture(), 0);
    aCodeRain.setUniformTexture("uSil", cv.silhouetteTexture(), 1);
    aCodeRain.setUniformTexture("uMotion", cv.motionTexture(), 2);
    aCodeRain.setUniformTexture("uAtlas", glyphAtlas.getTexture(), 3);
    aCodeRain.setUniform2f("uGridR", 60.0f, 100.0f);
    aCodeRain.setUniform1f("uTime", t);
    aCodeRain.setUniform1f("uGlyphGrid", (float)glyphGrid);
    aCodeRain.setUniform1f("uBright", 1.0f);
    ofSetColor(255); ofPushMatrix(); ofScale(rw, rh); unitQuad.draw(); ofPopMatrix();
    aCodeRain.end();
    ofSetColor(235); fLabel.drawString("CODE.RAIN // ACID", 70, 96);
    ofSetColor(90,94,100); fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0'), 70, 120);
}

void ofApp::floorFlow() {
    float hotX[8], hotY[8]; int hn = 0;
    for (auto& tr : cv.tracks) { if (hn >= 8) break; hotX[hn] = ofClamp(1.0f - tr.c.x / cv.camW, 0, 1); hotY[hn] = ofClamp(tr.c.y / cv.camH, 0, 1); hn++; }
    floorFlowShader.begin();
    floorFlowShader.setUniform2f("uRes", (float)rw, (float)rh);
    floorFlowShader.setUniform1f("uTime", t);
    if (hn > 0) { floorFlowShader.setUniform1fv("uHotX", hotX, hn); floorFlowShader.setUniform1fv("uHotY", hotY, hn); }
    floorFlowShader.setUniform1i("uHotN", hn);
    ofSetColor(255);
    ofPushMatrix(); ofScale(rw, rh); unitQuad.draw(); ofPopMatrix();
    floorFlowShader.end();
    ofSetColor(235); fLabel.drawString("FLUX.FIELD // ENERGY", 70, 96);
    ofSetColor(90, 94, 100); fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0'), 70, 120);
    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr);
        float sx = ofClamp(1.0f - tr.c.x / cv.camW, 0, 1) * rw, sy = ofClamp(tr.c.y / cv.camH, 0, 1) * rh;
        ofNoFill(); ofSetColor(255, 200); ofSetLineWidth(1.2f); ofDrawCircle(sx, sy, 15);
        ofFill(); ofSetColor(m.tint); ofDrawCircle(sx, sy, 3);
        ofSetColor(240); fTiny.drawString("SUBJ " + ofToString(tr.id % 1000, 3, '0'), sx + 20, sy - 4);
        ofSetColor(180, 205, 225); fBin.drawString("X" + ofToString(std::max(0, (int)tr.c.x), 4, '0') + " Y" + ofToString(std::max(0, (int)tr.c.y), 4, '0'), sx + 20, sy + 10);
    }
}

void ofApp::floorNodes3D() {
    // a luminous 3D constellation: a rotating web of lattice nodes; each visitor is a
    // glowing orb tethered into the field.
    ofSetColor(235); fLabel.drawString("NODE.FIELD // 3D", 70, 96);
    ofSetColor(90, 94, 100); fTiny.drawString("NODES " + ofToString((int)cv.tracks.size(), 2, '0'), 70, 120);

    float cx = rw * 0.5f, cy = rh * 0.5f, focal = rw * 1.1f, camZ = 3.2f;
    float ca = cosf(t * 0.22f), sa = sinf(t * 0.22f);
    auto proj = [&](glm::vec3 p, float& outZ) {
        float x = p.x * ca - p.z * sa, z = p.x * sa + p.z * ca + camZ, y = p.y;
        outZ = z; float s = focal / std::max(0.2f, z);
        return glm::vec2(cx + x * s, cy - y * s);
    };
    const int N = 300;
    std::vector<glm::vec3> P(N); std::vector<glm::vec2> S(N); std::vector<float> Z(N);
    for (int i = 0; i < N; i++) {
        float a = i * 2.399963f, rr = 0.45f + 0.9f * fabsf(sinf(i * 1.7f)), yy = (fmodf(i * 0.313f, 1.0f) - 0.5f) * 1.8f;
        P[i] = glm::vec3(cosf(a) * rr, yy, sinf(a * 1.13f) * rr);
        S[i] = proj(P[i], Z[i]);
    }
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    // glowing web
    ofSetLineWidth(1.0f);
    int offs[4] = { 1, 7, 17, 31 };
    for (int i = 0; i < N; i++) for (int k = 0; k < 4; k++) {
        int j = (i + offs[k]) % N; float d = glm::distance(P[i], P[j]);
        if (d < 0.6f) { float dep = ofClamp((4.6f - 0.5f * (Z[i] + Z[j])) / 3.6f, 0.1f, 1.0f);
            ofSetColor(40, 120, 155, (int)(60 * dep * (1.0f - d / 0.6f) * 2.0f)); ofDrawLine(S[i].x, S[i].y, S[j].x, S[j].y); }
    }
    // lattice nodes
    ofFill();
    for (int i = 0; i < N; i++) { float dep = ofClamp((4.6f - Z[i]) / 3.6f, 0.1f, 1.0f); ofSetColor(60, 155, 185, (int)(160 * dep)); ofDrawCircle(S[i].x, S[i].y, 0.8f + dep * 1.7f); }
    // subjects
    std::vector<glm::vec2> sp; std::vector<float> sz;
    for (auto& tr : cv.tracks) {
        float x = (0.5f - tr.c.x / cv.camW) * 1.6f, z = (tr.c.y / cv.camH - 0.5f) * 1.6f, y = 0.1f + 0.35f * sinf(t + tr.id);
        glm::vec3 w(x, y, z); float zz; sp.push_back(proj(w, zz)); sz.push_back(zz);
        // tether to nearest lattice nodes
        for (int k = 0; k < 3; k++) { int best = -1; float bd = 1e9f;
            for (int i = 0; i < N; i++) { float d = glm::distance(w, P[i]); if (d < bd && d > 0.05f) { bd = d; best = i; } }
            if (best >= 0) { ofSetColor(120, 200, 230, 130); ofDrawLine(sp.back().x, sp.back().y, S[best].x, S[best].y); }
        }
    }
    for (size_t i = 0; i < sp.size(); i++) for (size_t j = i + 1; j < sp.size(); j++) { ofSetColor(140, 210, 240, 190); ofSetLineWidth(1.4f); ofDrawLine(sp[i].x, sp[i].y, sp[j].x, sp[j].y); }
    int idx = 0;
    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr); glm::vec2 p = sp[idx]; float r = focal / std::max(0.2f, sz[idx]) * 0.06f;
        ofFill(); ofSetColor(m.tint, 70); ofDrawCircle(p.x, p.y, r * 2.8f);
        ofSetColor(m.tint, 130); ofDrawCircle(p.x, p.y, r * 1.7f);
        ofSetColor(255); ofDrawCircle(p.x, p.y, r * 0.7f);
        ofNoFill(); ofSetColor(m.tint, 180); ofDrawCircle(p.x, p.y, r * 2.0f + 4 * sinf(t * 3 + tr.id)); ofFill();
        idx++;
    }
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    idx = 0;
    for (auto& tr : cv.tracks) {
        glm::vec2 p = sp[idx];
        ofSetColor(235); fTiny.drawString("N" + ofToString(tr.id % 1000, 3, '0'), p.x + 16, p.y - 6);
        ofSetColor(150, 175, 185); fBin.drawString("X" + ofToString(std::max(0, (int)tr.c.x), 4, '0') + " Y" + ofToString(std::max(0, (int)tr.c.y), 4, '0'), p.x + 16, p.y + 8);
        idx++;
    }
}

void ofApp::renderFloor() {
    // FLOOR = live command-line TERMINAL (green code typed live + colour data blocks).
    floorFbo.begin();
    rw = FLOOR_W; rh = FLOOR_H;
    drawTerminal();
    floorFbo.end();
}

void ofApp::floorRadar(ofRectangle a) {
    ofPushStyle();
    ofSetColor(20, 40, 52); ofNoFill(); ofSetLineWidth(1); ofDrawRectangle(a);
    // grid + ticks
    ofSetColor(22, 34, 44);
    for (int i = 1; i < 10; i++) { float x = a.x + a.width * i / 10.0f; ofDrawLine(x, a.y, x, a.y + a.height); }
    for (int i = 1; i < 8; i++)  { float y = a.y + a.height * i / 8.0f;  ofDrawLine(a.x, y, a.x + a.width, y); }
    // sweeping radar line
    float sa = t * 0.6f;
    glm::vec2 cc(a.getCenter());
    ofSetColor(40, 120, 130, 120);
    ofDrawLine(cc.x, cc.y, cc.x + cosf(sa) * a.width * 0.6f, cc.y + sinf(sa) * a.height * 0.6f);

    auto mp = [&](glm::vec2 c) { return glm::vec2(a.x + (1.0f - c.x / cv.camW) * a.width, a.y + (c.y / cv.camH) * a.height); };

    // network between nodes
    std::vector<glm::vec2> pts;
    for (auto& tr : cv.tracks) pts.push_back(mp(glm::vec2(tr.c.x, tr.c.y)));
    ofSetColor(50, 90, 110, 150); ofSetLineWidth(1);
    for (size_t i = 0; i < pts.size(); i++) for (size_t j = i + 1; j < pts.size(); j++) {
        ofDrawLine(pts[i].x, pts[i].y, pts[j].x, pts[j].y);
        glm::vec2 mid = (pts[i] + pts[j]) * 0.5f;
        ofSetColor(70, 110, 130); fBin.drawString(ofToString((int)glm::distance(pts[i], pts[j])), mid.x, mid.y);
        ofSetColor(50, 90, 110, 150);
    }

    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr);
        glm::vec2 p = mp(glm::vec2(tr.c.x, tr.c.y));
        // trail
        ofNoFill(); ofSetColor(m.tint, 120); ofSetLineWidth(1.4f);
        ofBeginShape();
        for (auto& c : m.trail) { glm::vec2 tp = mp(c); ofVertex(tp.x, tp.y); }
        ofEndShape(false);
        // velocity vector (x mirrored)
        glm::vec2 v(-tr.vel.x, tr.vel.y);
        ofSetColor(m.tint); ofSetLineWidth(1.6f);
        ofDrawLine(p.x, p.y, p.x + v.x * 0.15f, p.y + v.y * 0.15f);
        // node + ring
        ofFill(); ofSetColor(m.tint); ofDrawCircle(p.x, p.y, 3.5f);
        ofNoFill(); ofSetColor(m.tint, 150); ofDrawCircle(p.x, p.y, 12 + 4 * sinf(t * 3 + tr.id));
        // crosshair to axes
        ofSetColor(m.tint, 60);
        ofDrawLine(a.x, p.y, a.x + a.width, p.y); ofDrawLine(p.x, a.y, p.x, a.y + a.height);
        // label
        ofSetColor(220, 235, 240);
        fTiny.drawString("N" + ofToString(tr.id % 1000, 3, '0'), p.x + 8, p.y - 6);
        ofSetColor(150, 175, 185);
        fBin.drawString("X" + ofToString((int)tr.c.x) + " Y" + ofToString((int)tr.c.y), p.x + 8, p.y + 8);
    }
    ofPopStyle();
}

void ofApp::floorRows(ofRectangle a) {
    ofPushStyle();
    ofSetColor(20, 40, 52); ofNoFill(); ofDrawRectangle(a);
    ofSetColor(60, 220, 235); fTiny.drawString("ID     X     Y    VEL   AREA   CONF", a.x + 8, a.y + 16);
    ofSetColor(30, 60, 74); ofDrawLine(a.x, a.y + 22, a.x + a.width, a.y + 22);
    int row = 0;
    for (auto& tr : cv.tracks) {
        Meta& m = metaFor(tr);
        float y = a.y + 40 + row * 22; if (y > a.y + a.height - 6) break;
        ofSetColor(m.tint); ofDrawRectangle(a.x + 6, y - 8, 4, 4);
        ofSetColor(210, 228, 234);
        char buf[128];
        snprintf(buf, sizeof(buf), "%03d  %4d  %4d  %4d  %5d   %.2f",
                 tr.id % 1000, (int)tr.c.x, (int)tr.c.y, (int)tr.speed, (int)tr.area,
                 0.80f + 0.18f * ofNoise(tr.id * 2.0f, t * 0.3f));
        fLabel.drawString(buf, a.x + 16, y);
        // velocity bar
        float bw = ofClamp(tr.speed / (cv.camW * 0.5f), 0, 1) * 120;
        ofSetColor(m.tint, 180); ofDrawRectangle(a.x + a.width - 130, y - 9, bw, 8);
        ofNoFill(); ofSetColor(60, 90, 100); ofDrawRectangle(a.x + a.width - 130, y - 9, 120, 8); ofFill();
        row++;
    }
    ofPopStyle();
}

void ofApp::floorWave(ofRectangle a) {
    ofPushStyle();
    ofSetColor(20, 40, 52); ofNoFill(); ofDrawRectangle(a);
    ofSetColor(60, 220, 235); fTiny.drawString("MOTION.ENERGY", a.x + 8, a.y + 14);
    // energy history
    ofSetColor(60, 230, 120); ofSetLineWidth(1.6f); ofNoFill();
    ofBeginShape();
    int nH = (int)energyHist.size();
    for (int i = 0; i < nH; i++) {
        float x = a.x + a.width * i / (float)std::max(1, nH - 1);
        float y = a.y + a.height - 8 - energyHist[i] * (a.height - 24);
        ofVertex(x, y);
    }
    ofEndShape(false);
    // signal traces
    ofSetColor(245, 190, 60, 160); ofSetLineWidth(1.0f);
    ofBeginShape();
    for (int x = 0; x < (int)a.width; x += 4) {
        float y = a.getCenter().y + sinf(x * 0.05f + t * 4) * (6 + 20 * energy);
        ofVertex(a.x + x, y);
    }
    ofEndShape(false);
    ofPopStyle();
}

void ofApp::floorStream(ofRectangle a) {
    ofPushStyle();
    ofSetColor(20, 40, 52); ofNoFill(); ofDrawRectangle(a);
    ofSetColor(60, 220, 235); fTiny.drawString("DATA.STREAM", a.x + 8, a.y + 14);
    int cols = 10;
    for (int c = 0; c < cols; c++) {
        float x = a.x + 10 + c * (a.width - 20) / cols;
        float off = fmodf(t * 60 + c * 37, 16.0f);
        for (int r = 0; r < 11; r++) {
            float y = a.y + 26 + r * 15 + off;
            if (y > a.y + a.height - 4) continue;
            int b = ((int)(x * 3 + y * 7 + t * 20 + c * 13)) & 1;
            float br = 60 + 160 * ofNoise(c * 2.1f, r * 0.7f + t);
            ofSetColor(60 + b * 40, (int)br, 120 + b * 60);
            fBin.drawString(b ? "1" : "0", x, y);
        }
    }
    ofPopStyle();
}

// ================================================================= draw
void ofApp::draw() {
    updateMotionTrail();
    stepSims();              // advance BOTH GPU sims (wall swarm + floor cascade) OUTSIDE their FBOs
    if (mode == 6 && !cv.tracks.empty()) renderCloudFbo();   // 3D point cloud into fbo3d, outside wallFbo
    renderWall();
    renderFloor();

    ofBackground(0);
    ofSetColor(255);
    if (previewMode == 0) {                 // WALL, pixel-perfect 1:1
        drawWallTex(0, 0, WALL_W, WALL_H);
    } else if (previewMode == 1) {          // FLOOR, pixel-perfect 1:1 (pan up/down)
        drawFloorTex(0, -panY, FLOOR_W, FLOOR_H);
    } else {                                // FIT-all — layout tuning with the panel
        float cw = canvasW.get(), ch = canvasH.get();
        float sc = std::min((float)ofGetWidth() / cw, (float)ofGetHeight() / ch);
        ofPushMatrix(); ofScale(sc, sc);
        ofNoFill(); ofSetColor(40); ofDrawRectangle(0, 0, cw, ch); ofFill();
        ofSetColor(255);
        drawWallTex(wallX.get(), wallY.get(), wallW.get(), wallH.get());
        drawFloorTex(floorX.get(), floorY.get(), floorW.get(), floorH.get());
        ofPopMatrix();
    }
    ofSetColor(120); fTiny.drawString(previewMode == 0 ? "[v] WALL 1:1" : previewMode == 1 ? "[v] FLOOR 1:1  (up/down pan)" : "[v] FIT / layout", 8, ofGetHeight() - 8);

    if (autoShot) {
        int fn = ofGetFrameNum();
        ofPixels px;
        if (fn == 60)  mode = 0;
        if (fn == 90)  { wallFbo.readToPixels(px);  ofSaveImage(px, "scan_w_m0_volumes.png"); }
        if (fn == 100) mode = 1;
        if (fn == 130) { wallFbo.readToPixels(px);  ofSaveImage(px, "scan_w_m1_cards.png"); }
        if (fn == 140) mode = 2;
        if (fn == 170) { wallFbo.readToPixels(px);  ofSaveImage(px, "scan_w_m2_mesh.png"); }
        if (fn == 180) mode = 6;
        if (fn == 210) { wallFbo.readToPixels(px);  ofSaveImage(px, "scan_w_m6_cloud.png"); }
        if (fn == 220) mode = 9;
        if (fn == 250) { wallFbo.readToPixels(px);  ofSaveImage(px, "scan_w_m9_swarm.png"); }
        if (fn == 300) { floorFbo.readToPixels(px); ofSaveImage(px, "scan_floor_term.png"); }        // normal terminal
        if (fn == 320) termGather = 1.0f;
        if (fn == 322) { floorFbo.readToPixels(px); ofSaveImage(px, "scan_floor_term_gather.png"); } // gather/collapse
        if (fn >= 340) ofExit();
    }

    if (showDbg) {                          // on-screen diagnostics (press 'i' to hide)
        std::string dbg = "SRC:" + cv.sourceLabel + std::string(cv.isConnected() ? "" : "(NO-SIGNAL)")
            + "  DNN:" + std::string(cv.detectorLoaded() ? "ON" : "OFF")
            + "  CAM:" + ofToString(cv.camW) + "x" + ofToString(cv.camH)
            + "  FRAME:" + ofToString((int)cv.haveFrame)
            + "  DET:" + ofToString(cv.numDetections())
            + "  TRK:" + ofToString((int)cv.tracks.size())
            + "  FPS:" + ofToString((int)ofGetFrameRate());
        ofSetColor(0, 0, 0, 200); ofDrawRectangle(4, 4, 620, 24);
        ofSetColor(0, 255, 130); fLabel.drawString(dbg, 10, 21);
    }

    if (showPanel) gui.draw();
}

void ofApp::keyPressed(int key) {
    if (key == 'i' || key == 'I') { pShowDbgP = !pShowDbgP; return; }
    if (key == 'n' || key == 'N') { pUseNDI = !pUseNDI; return; }        // toggle NDI source (TouchDesigner)
    if (key == 'p' || key == 'P') { showPanel = !showPanel; return; }
    if (key == 's' || key == 'S') { gui.saveToFile("layout.xml"); return; }
    if (key == 'f') { ofToggleFullscreen(); return; }
    if (key == 'w' || key == 'W') { ofSetWindowShape((int)canvasW.get(), (int)canvasH.get()); return; }  // set output to canvas resolution
    if (showPanel) return;                       // panel open -> let the number fields consume typing
    if (key == 'v' || key == 'V') previewMode = (previewMode + 1) % 3;
    else if (key == OF_KEY_DOWN) panY = ofClamp(panY + 60, 0, FLOOR_H - ofGetHeight());
    else if (key == OF_KEY_UP)   panY = ofClamp(panY - 60, 0, FLOOR_H - ofGetHeight());
    else if (key == ' ') cut();
    else if (key >= '1' && key <= '9') { mode = key - '1'; modeT = 0; }
}

// ================================================================= synth
void ofApp::blip(float freq, float amp, float decay, int type) {
    if (!soundOn) return;
    std::lock_guard<std::mutex> lk(audioMx);
    for (auto& v : voices) if (!v.on) { v.on = true; v.freq = freq; v.amp = amp; v.decay = decay; v.phase = 0; v.type = type; return; }
}
// Ikeda-minimal: soft pure-sine pings on detect / lose only (sparse). No per-cut clicks.
void ofApp::triggerDetect() {
    static const float sc[5] = { 880.0f, 987.77f, 1174.66f, 1318.51f, 1567.98f };  // pentatonic
    blip(sc[(int)ofRandom(5)], 0.10f, 0.99965f, 0);
}
void ofApp::triggerLost()   { blip(ofRandom(196, 261), 0.07f, 0.99955f, 0); }
void ofApp::triggerCut()    { /* silent — cuts are frequent; keep the sound sparse */ }

void ofApp::audioOut(ofSoundBuffer& buffer) {
    std::lock_guard<std::mutex> lk(audioMx);
    int n = (int)buffer.getNumFrames();
    float sr = (float)sampleRate;
    for (int i = 0; i < n; i++) {
        droneLvl += (droneTarget - droneLvl) * 0.0002f;
        dronePh1 += 48.0f / sr;  if (dronePh1 > 1) dronePh1 -= 1;            // very low, barely-there sub
        dronePh2 += 48.15f / sr; if (dronePh2 > 1) dronePh2 -= 1;
        float s = (sinf(dronePh1 * TWO_PI) + sinf(dronePh2 * TWO_PI)) * 0.5f * droneLvl * 0.5f;
        for (auto& v : voices) if (v.on) {
            v.phase += v.freq / sr; if (v.phase > 1) v.phase -= 1;
            float env = v.amp;
            s += sinf(v.phase * TWO_PI) * env;                              // pure sine only
            v.amp *= v.decay;
            if (v.amp < 0.0006f) v.on = false;
        }
        s = tanhf(s * 1.1f) * 0.6f;                                         // gentle, low master
        buffer[i * 2] = s; buffer[i * 2 + 1] = s;
    }
}

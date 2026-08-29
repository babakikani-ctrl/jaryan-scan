#pragma once
#include "ofMain.h"
#include "ofxGui.h"
#include "CvPipeline.h"
#include <mutex>
#include <deque>

// DATA.JARYAN — SCAN. ONE HDMI canvas (960 x 2304) driving two LED surfaces:
//   • WALL  (top, 960x768)  : visitors SEE THEMSELVES, augmented by machine vision —
//     segmentation volumes, surveillance crop-cards, wireframe meshes, photogrammetry.
//     Cuts fast & random between modes, with a reactive synth.
//   • FLOOR (bottom, 960x1536): the machine's COMPUTATION — tracking solver, coordinate
//     radar, data streams, matrices. Highly engineered data-art. No self-image.

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void audioOut(ofSoundBuffer& buffer) override;

    static const int WALL_W = 960, WALL_H = 768;
    static const int FLOOR_W = 960, FLOOR_H = 1536;
    static const int CANVAS_W = 960, CANVAS_H = WALL_H + FLOOR_H;   // 960 x 2304

private:
    CvPipeline cv;
    ofTrueTypeFont fLabel, fTiny, fBin, fBig;
    ofShader machine;
    ofVboMesh unitQuad;
    ofFbo wallFbo, floorFbo;
    float t = 0, energy = 0, rw = WALL_W, rh = WALL_H;
    bool autoShot = false;
    bool showDbg  = true;              // on-screen diagnostics ('i' toggles) — DNN / cam / tracks
    bool soundOn = true;               // refined minimal Ikeda-style sound
    int  previewMode = 2;              // default = FIT (whole single canvas). 0 WALL 1:1 · 1 FLOOR 1:1 · 2 FIT
    float panY = 0;

    // WALL director — cuts between machine modes: mostly medium, sometimes slow holds,
    // occasionally rapid erratic bursts.
    int   mode = 0, nEffects = 10;   // 9 machine-vision worlds + the acid code-glyph swarm
    float modeT = 0, nextCut = 1.2f;
    int   flickerLeft = 0;
    void cut();

    // 3D point-cloud (mode 6) + cyber glitch overlay + orbit dots (mode 7)
    ofShader  pcloud, glitchPost;
    ofVboMesh cloudMesh;
    ofCamera  cam3;
    ofFbo     fbo3d;
    float     cyber = 0;              // cyber-burst intensity (drives glitch), decays
    void modeCloud();
    void renderCloudFbo();   // render the 3D point cloud into fbo3d OUTSIDE wallFbo (nesting flips the wall)
    void modeOrbit();
    void drawWallTex(float x, float y, float w, float h);

    // code-rain (mode 4)
    ofShader coderain;
    ofFbo    glyphAtlas;
    ofTrueTypeFont fGlyph;
    int      glyphGrid = 8;
    void buildGlyphAtlas();
    void modeRain();

    // interactive motion light-trail (colourful glow that follows the visitor's hands)
    ofFbo motionFbo[2];
    int   trailCur = 0;
    void  updateMotionTrail();

    struct Meta { ofColor tint; std::string bits; float ox, oy; std::deque<glm::vec2> trail; };
    std::map<int, Meta> meta;
    std::set<int> seen;
    Meta& metaFor(const CvTrack& tr);
    std::vector<float> energyHist;

    // WALL modes
    void renderWall();
    void modeVolumes();
    void modeCards();
    void modeMesh();
    void modePhotogram();
    void modeParts();      // body-part decomposition (head / hands / legs)
    void modeScan();       // scan-line reconstruction (being 3D-scanned)
    void modeWallSwarm();  // acid code-glyph swarm world (person rebuilt from swirling code)
    void modeAttract();    // idle state when no one is detected
    void drawBox(const ofRectangle& r, const std::string& id, float conf, ofColor c);
    void drawDet(const CvTrack& tr, ofColor c);   // detection box with ACQUIRING -> LOCKED states
    void drawPersonReal(const CvTrack& tr);       // the person's actual recognizable video crop (base layer)
    ofShader personFg;                            // clear silhouette-masked real video shown IN FRONT of the effect
    void drawPersonFg();                          // draw all tracked people, clear + sharp, over the background effect
    void drawMotionSparks();                      // bright reactive sparks wherever the visitor moves (interactivity)
    void drawMotionCode();                         // clear, colourful CODE glyphs that bloom in the motion energy
    ofRectangle trackRect(const CvTrack& tr);
    glm::vec4   camUv(const CvTrack& tr);
    void drawMachine(ofRectangle dst, glm::vec2 uvMin, glm::vec2 uvMax, glm::vec2 cells, ofColor tint, float gray, float scan);
    void drawHUD(const std::string& tag);

    // FLOOR — ported acid effects: 0 flow-fluid · 1 particle-swarm · 2 colourful code-rain
    int   floorMode = 0, nFloor = 3;
    float floorT = 0, floorNext = 15.0f;
    ofShader floorRainShader, floorFlowShader;
    ofCamera camFloor;
    void renderFloor();
    void floorFlow();
    void floorRain();
    void floorNodes3D();
    void floorTelemetry(ofRectangle field);

    // WALL interactive centrepiece — the acid CODE-GLYPH SWARM: the visitor's live image
    // is rebuilt from thousands of coloured code characters that swirl when they move.
    ofShader  aFlow, aPInit, aPVel, aPPos, aPRender, aCodeRain;
    ofFbo     aInk[2]; int aInkCur = 0;
    ofFbo     aPos[2], aVel[2]; int aPcur = 0, apW = 200, apH = 150;   // landscape grid for the wall
    ofVboMesh aPMesh, aSimQuad;
    void initFloorFx();
    void stepSims();          // ping-pong the wall swarm + floor cascade OUTSIDE their FBOs (nesting flips them)
    void drawWallSwarm();     // render the code-glyph swarm into the wall
    void floorFlowFluid();
    void floorParticles();
    void floorCodeRain();

    // FLOOR code CASCADE — the wall's data rains down, pools & drifts on pure black (kept, unused)
    ofShader  cascInit, cascSim, cascRender;
    ofFbo     cascState[2]; int cascCur = 0;
    ofVboMesh cascMesh;
    int       cascW = 150, cascH = 190;
    void initCascade();
    void stepCascade();
    void drawCascade();

    // FLOOR = live TERMINAL — a command-line environment: green code TYPED live, colour
    // data blocks, real telemetry, with periodic glitch bursts + "gather / collapse" events.
    ofTrueTypeFont fTerm;
    struct TermLine { std::string text; std::vector<ofColor> col; bool block = false; };
    std::deque<TermLine> termLines;      // committed lines (top scrolls off)
    TermLine termFull;                   // the line currently being typed (target)
    size_t   termPos = 0;                // chars typed so far on termFull
    float    termAcc = 0, termCharW = 10, termLineH = 20;
    int      termRows = 74;
    float    floorGlitch = 0;            // floor glitch-burst intensity (decays)
    float    termEvT = 0, termEvNext = 7, termGather = 0;   // gather/collapse event
    int      termSeq = 0;                // line counter (drives content)
    void  initTerminal();
    void  updateTerminal(float dt);
    void  drawTerminal();
    void  drawTermLine(const TermLine& L, float x, float y, int upto);
    TermLine termGenLine();
    void  drawFloorTex(float x, float y, float w, float h);   // glitch composite for the floor
    void floorRadar(ofRectangle area);
    void floorRows(ofRectangle area);
    void floorStream(ofRectangle area);
    void floorWave(ofRectangle area);

    // ---- layout panel (P): hand-tune where each surface lands on the HDMI canvas ----
    ofxPanel gui;
    bool showPanel = false;
    ofParameter<float> canvasW, canvasH;
    ofParameter<float> wallX, wallY, wallW, wallH;
    ofParameter<float> floorX, floorY, floorW, floorH;
    ofParameter<bool>  showSeam;
    ofxFloatField ifCanvasW, ifCanvasH, ifWallX, ifWallY, ifWallW, ifWallH, ifFloorX, ifFloorY, ifFloorW, ifFloorH;  // typeable number fields

    // ---- live control panel (everything tunable on-site) ----
    ofParameter<bool>  pMirror, pAutoCycle, pShowPerson, pShowDbgP;
    ofParameter<int>   pManualMode;
    ofParameter<float> pDetConf, pCutSec, pBright, pGlitch, pEnergy, pRim, pTermSpeed;
    ofxButton btnReconnect, btnFullscreen, btnRelearnBg;
    void onReconnect();  void onFullscreen();  void onRelearnBg();

    // ---- synth ----
    ofSoundStream stream;
    std::mutex audioMx;
    struct Voice { float freq = 0, phase = 0, amp = 0, decay = 0.999f; int type = 0; bool on = false; };
    std::vector<Voice> voices;
    float dronePh1 = 0, dronePh2 = 0, droneLvl = 0, droneTarget = 0;
    int   sampleRate = 44100;
    void blip(float freq, float amp, float decay, int type);
    void triggerDetect();
    void triggerLost();
    void triggerCut();
};

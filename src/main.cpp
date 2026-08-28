#include "ofMain.h"
#include "ofApp.h"

// Front LED wall native resolution: 960 x 768 (2.5m x 2m).
// Override with JARYAN_W / JARYAN_H for preview.
int main() {
    int w = 480, h = 1152;               // portrait preview of the single 960x2304 canvas (wall over floor)
    if (const char* e = getenv("JARYAN_W")) w = ofToInt(e);
    if (const char* e = getenv("JARYAN_H")) h = ofToInt(e);

    ofGLWindowSettings s;
    s.setSize(w, h);
    s.setGLVersion(3, 2);
    s.windowMode = OF_WINDOW;
    auto win = ofCreateWindow(s);
    ofRunApp(win, std::make_shared<ofApp>());
    ofRunMainLoop();
}

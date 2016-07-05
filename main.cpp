#include <SFML/Audio/SoundRecorder.hpp>
#include <SFML/Audio/SoundBufferRecorder.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>
#include <vector>
#include <iostream>
#include "/usr/local/lib/keyfinder/keyfinder.h"
#include <thread>
#include <chrono>
#include <limits.h>
#include <mutex>

using namespace std;
using namespace sf;
using namespace KeyFinder;

const char* get_result(KeyFinder::key_t result) {
    const char *key;
    switch(result) {
        case A_MAJOR: key      = "A Major"; break;
        case A_MINOR: key      = "A Minor"; break;
        case B_FLAT_MAJOR: key = "B Flat Major"; break;
        case B_FLAT_MINOR: key = "B Flat Minor"; break;
        case B_MAJOR: key      = "B Major"; break;
        case B_MINOR: key      = "B Minor"; break;
        case C_MAJOR: key      = "C Major"; break;
        case C_MINOR: key      = "C Minor"; break;
        case D_FLAT_MAJOR: key = "D Flat Major"; break;
        case D_FLAT_MINOR: key = "D Flat Minor"; break;
        case D_MAJOR: key      = "D Major"; break;
        case D_MINOR: key      = "D Minor"; break;
        case E_FLAT_MAJOR: key = "E Flat Major"; break;
        case E_FLAT_MINOR: key = "E Flat Minor"; break;
        case E_MAJOR: key      = "E Major"; break;
        case E_MINOR: key      = "E Minor"; break;
        case F_MAJOR: key      = "F Major"; break;
        case F_MINOR: key      = "F Minor"; break;
        case G_FLAT_MAJOR: key = "G Flat Major"; break;
        case G_FLAT_MINOR: key = "G Flat Minor"; break;
        case G_MAJOR: key      = "G Major"; break;
        case G_MINOR: key      = "G Minor"; break;
        case A_FLAT_MAJOR: key = "A Flat Major"; break;
        case A_FLAT_MINOR: key = "A Flat Minor"; break;
        case SILENCE: key      = "(silence)"; break;
        default: key           = "No dice!";
    }
    return key;
}

static KeyFinder::KeyFinder k;
static AudioData a;
static Workspace w;

mutex mu;

void initWorkspace() {
    mu.lock();
    a = {};
    w = {};
    a.setFrameRate(44100);
    a.setChannels(1);
    mu.unlock();
}

class CustomRecorder : public SoundRecorder {
    bool onProcessSamples(const Int16* samples, size_t sampleCount) {
        mu.lock();
        a.addToSampleCount(sampleCount);
        for(int i = 0; i < sampleCount; i++) {
            double sample = (double)(samples[i] - SHRT_MIN)/(double)(SHRT_MAX - SHRT_MIN);
            try {
                a.setSample(i, sample);
            } catch(const Exception& e) {
                cerr << "Exception:" << e.what() << "\n";
                mu.unlock();
                return false;
            }
        }
        k.progressiveChromagram(a, w);
        cout << get_result(k.keyOfChromagram(w)) << "\n";
        mu.unlock();
        return true;
    }
};

int main() {
    cout << "getAvailableDevices:\n";    
    vector<string> devices = SoundRecorder::getAvailableDevices();
    for(string device : devices) {
        cout << device << "\n";
    }
    cout << "\ngetDefaultDevice:\n" << SoundRecorder::getDefaultDevice() << "\n";

    if(SoundRecorder::isAvailable()) {
        cout << "Can record!\n";
    } else {
        cout << "Can *NOT* record :(\n";
        return 1;
    }

    initWorkspace();

    CustomRecorder rec;
    rec.start();
    RenderWindow window(VideoMode(480, 480), "keyfinder_gui");
    Font font;
    font.loadFromFile("sfns.ttf");

    while(window.isOpen()) {
        sf::Event e;
        while(window.pollEvent(e)) {
            if(e.type == sf::Event::Closed) {
                window.close();
            }
        }
        window.setActive();

        mu.lock();
        const char* key = get_result(k.keyOfChromagram(w));
        mu.unlock();

        Text text(key, font);
        text.setCharacterSize(30);
        text.setColor(Color::White);

        window.clear();
        window.draw(text);
        window.display();
    }

    rec.stop();

    /* SoundBufferRecorder rec; */
    /* rec.start(); */
    /* this_thread::sleep_for(chrono::seconds(2)); */
    /* rec.stop(); */
    /* const SoundBuffer& buf = rec.getBuffer(); */
    /* buf.saveToFile("testout.ogg"); */

    return 0;
}

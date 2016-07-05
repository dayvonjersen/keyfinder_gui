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
#include <map>

#define WIDTH  320
#define HEIGHT 320

using namespace std;
using namespace sf;
using namespace KeyFinder;

typedef struct key {
    const char* text;
    const char* code;
    sf::Uint32  color;
} key;

static map<KeyFinder::key_t,key> KeySignature;
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

    KeySignature[A_FLAT_MINOR] = {"A Flat Minor", "1A", 0xb8ffe1ff};
    KeySignature[E_FLAT_MINOR] = {"E Flat Minor", "2A", 0xc2ffc6ff};
    KeySignature[B_FLAT_MINOR] = {"B Flat Minor", "3A", 0xd2f7a7ff};
    KeySignature[F_MINOR]      = {"F Minor",      "4A", 0xe4e2a9ff};
    KeySignature[C_MINOR]      = {"C Minor",      "5A", 0xf6c4abff};
    KeySignature[G_MINOR]      = {"G Minor",      "6A", 0xffafb8ff};
    KeySignature[D_MINOR]      = {"D Minor",      "7A", 0xf7aeccff};
    KeySignature[A_MINOR]      = {"A Minor",      "8A", 0xe2aeecff};
    KeySignature[E_MINOR]      = {"E Minor",      "9A", 0xd1aefeff};
    KeySignature[B_MINOR]      = {"B Minor",     "10A", 0xc5c1feff};
    KeySignature[G_FLAT_MINOR] = {"F Sharp Minor","11A",0xb6e5ffff};
    KeySignature[D_FLAT_MINOR] = {"D Flat Minor","12A", 0xaefefdff};

    KeySignature[B_MAJOR]      = {"B Major",      "1B", 0x8effd1ff};
    KeySignature[G_FLAT_MAJOR] = {"F Sharp Major","2B", 0x9fff9eff};
    KeySignature[D_FLAT_MAJOR] = {"D Flat Major", "3B", 0xbaf976ff};
    KeySignature[A_FLAT_MAJOR] = {"A Flat Major", "4B", 0xd5ce74ff};
    KeySignature[E_FLAT_MAJOR] = {"E Flat Major", "5B", 0xf3a47bff};
    KeySignature[B_FLAT_MAJOR] = {"B Flat Major", "6B", 0xff7988ff};
    KeySignature[F_MAJOR]      = {"F Major",      "7B", 0xf079b1ff};
    KeySignature[C_MAJOR]      = {"C Major",      "8B", 0xcf7fe2ff};
    KeySignature[G_MAJOR]      = {"G Major",      "9B", 0xb67fffff};
    KeySignature[D_MAJOR]      = {"D Major",     "10B", 0x9fa4ffff};
    KeySignature[A_MAJOR]      = {"A Major",     "11B", 0x82dfffff};
    KeySignature[E_MAJOR]      = {"E Major",     "12B", 0x7efffbff};

    KeySignature[SILENCE]      = {"(silence)",      "", 0xffffffff};

    initWorkspace();

    CustomRecorder rec;
    rec.start();
    RenderWindow window(VideoMode(WIDTH, HEIGHT), "keyfinder_gui");
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
        KeyFinder::key_t key = k.keyOfChromagram(w);
        mu.unlock();

        Text text(KeySignature[key].text, font);
        text.setCharacterSize(30);
        text.setColor(Color::Black);

        Text code(KeySignature[key].code, font);
        FloatRect rect = code.getLocalBounds();
        code.setOrigin(rect.left + rect.width/2.f, rect.top + rect.height/2.f);
        code.setPosition((float)WIDTH/2.f, (float)HEIGHT/2.f);
        code.setCharacterSize(48);
        code.setColor(Color::Black);

        window.clear(Color(KeySignature[key].color));
        window.draw(text);
        window.draw(code);
        window.display();
    }

    rec.stop();

    return 0;
}

#include <SFML/Audio/SoundRecorder.hpp>
#include <SFML/Audio/SoundBufferRecorder.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Window/Event.hpp>
#include <vector>
#include <iostream>
#include "/usr/local/lib/keyfinder/keyfinder.h"
#include <thread>
#include <chrono>
#include <limits.h>
#include <mutex>
#include <map>
#include "fftw3.h"

#define WIDTH  320.f
#define HEIGHT 320.f
#define INPUT_SIZE 1024

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

static KeyFinder::key_t latest_key;
double* INPUT_BUFFER;

void initWorkspace() {
    a = {};
    w = {};
    a.setFrameRate(44100);
    a.setChannels(2);
}

class CustomRecorder : public SoundRecorder {
    bool onProcessSamples(const Int16* samples, size_t sampleCount) {
        mu.lock();
        a.addToSampleCount(sampleCount);
        double bounded[sampleCount];
        for(int i = 0; i < sampleCount; i++) {
            double sample = (double)(samples[i] - SHRT_MIN)/(double)(SHRT_MAX - SHRT_MIN);
            bounded[i] = sample;
        }
        for(int i = 0; i < INPUT_SIZE; i++) {
            if(i >= sampleCount) {
                INPUT_BUFFER[i] = 0.0;
            } else {
                INPUT_BUFFER[i] = bounded[i];
            }
        }
        for(int i = 0; i < sampleCount; i++) {
            try {
                a.setSample(i, bounded[i]);
            } catch(const Exception& e) {
                cerr << "Exception:" << e.what() << "\n";
                mu.unlock();
                return false;
            }
        }
        k.progressiveChromagram(a, w);
        latest_key = k.keyOfChromagram(w);
        mu.unlock();
        return true;
    }
};

int main() {
    if(!SoundRecorder::isAvailable()) {
        cerr << "SoundRecorder::isAvailable() == false\n";
        return 1;
    }

start:
    vector<string> devices = SoundRecorder::getAvailableDevices();
    string default_device = SoundRecorder::getDefaultDevice();
    int default_device_index;
    int i = 0;
    for(string device : devices) {
        cout << "[" << i << "] " << device;
        if(device == default_device) {
            default_device_index = i;
            cout << " (default)";
        }
        i++;
        cout << "\n";
    }
    cout << "\nCHOOSE A DEVICE: ";
    int choice;
    cin >> noskipws >> choice;
    if(choice == -1) {
        choice = default_device_index;
    } else if(choice >= i) {
        cerr << "Bzzt.\n\n";
        goto start;
    }
    CustomRecorder rec;
    if(!rec.setDevice(devices[choice])) {
        cerr << "Device selection failed.\n";
        return 1;
    }
 
    KeySignature[A_FLAT_MINOR] = {"A Flat Minor",   "1A", 0xb8ffe1ff};
    KeySignature[E_FLAT_MINOR] = {"E Flat Minor",   "2A", 0xc2ffc6ff};
    KeySignature[B_FLAT_MINOR] = {"B Flat Minor",   "3A", 0xd2f7a7ff};
    KeySignature[F_MINOR]      = {"F Minor",        "4A", 0xe4e2a9ff};
    KeySignature[C_MINOR]      = {"C Minor",        "5A", 0xf6c4abff};
    KeySignature[G_MINOR]      = {"G Minor",        "6A", 0xffafb8ff};
    KeySignature[D_MINOR]      = {"D Minor",        "7A", 0xf7aeccff};
    KeySignature[A_MINOR]      = {"A Minor",        "8A", 0xe2aeecff};
    KeySignature[E_MINOR]      = {"E Minor",        "9A", 0xd1aefeff};
    KeySignature[B_MINOR]      = {"B Minor",       "10A", 0xc5c1feff};
    KeySignature[G_FLAT_MINOR] = {"F Sharp Minor", "11A", 0xb6e5ffff};
    KeySignature[D_FLAT_MINOR] = {"D Flat Minor",  "12A", 0xaefefdff};

    KeySignature[B_MAJOR]      = {"B Major",       "1B", 0x8effd1ff};
    KeySignature[G_FLAT_MAJOR] = {"F Sharp Major", "2B", 0x9fff9eff};
    KeySignature[D_FLAT_MAJOR] = {"D Flat Major",  "3B", 0xbaf976ff};
    KeySignature[A_FLAT_MAJOR] = {"A Flat Major",  "4B", 0xd5ce74ff};
    KeySignature[E_FLAT_MAJOR] = {"E Flat Major",  "5B", 0xf3a47bff};
    KeySignature[B_FLAT_MAJOR] = {"B Flat Major",  "6B", 0xff7988ff};
    KeySignature[F_MAJOR]      = {"F Major",       "7B", 0xf079b1ff};
    KeySignature[C_MAJOR]      = {"C Major",       "8B", 0xcf7fe2ff};
    KeySignature[G_MAJOR]      = {"G Major",       "9B", 0xb67fffff};
    KeySignature[D_MAJOR]      = {"D Major",      "10B", 0x9fa4ffff};
    KeySignature[A_MAJOR]      = {"A Major",      "11B", 0x82dfffff};
    KeySignature[E_MAJOR]      = {"E Major",      "12B", 0x7efffbff};

    KeySignature[SILENCE]      = {"(silence)",      "", 0xffffffff};

    int output_size = INPUT_SIZE/2+1;
    INPUT_BUFFER = static_cast<double*>(fftw_malloc(INPUT_SIZE*sizeof(double)));
    fftw_complex *output_buffer = static_cast<fftw_complex*>(fftw_malloc(output_size*sizeof(fftw_complex)));
    fftw_plan plan = fftw_plan_dft_r2c_1d(INPUT_SIZE, INPUT_BUFFER, output_buffer, FFTW_ESTIMATE);

    initWorkspace();
    rec.start();
    RenderWindow window(VideoMode(WIDTH, HEIGHT), "keyfinder_gui");
    Font font;
    font.loadFromFile("sfns.ttf");

    while(window.isOpen()) {
        sf::Event e;
        bool do_draw = true;
        while(window.pollEvent(e)) {
            if(e.type == sf::Event::Closed) {
                window.close();
            }
            if(e.type == sf::Event::KeyPressed) {
                switch(e.key.code) {
                case Keyboard::Q:
                    window.close();
                    do_draw = false;
                    break;
                case Keyboard::R:
                    mu.lock();
                    a = {};
                    w = {};
                    a.setFrameRate(44100);
                    a.setChannels(2);
                    mu.unlock();
                    break;
                }
            }
        }
        if(!do_draw) {
            continue;
        }
        window.setActive();

        struct key sig = KeySignature[latest_key];

        Text text(sig.text, font);
        text.setCharacterSize(30);
        text.setColor(Color::Black);
        FloatRect rect = text.getLocalBounds();
        text.setOrigin(rect.left + rect.width/2.f, 0);
        text.setPosition(WIDTH/2.f, 0);

        Text code(sig.code, font);
        code.setCharacterSize(48);
        code.setColor(Color::Black);
        rect = code.getLocalBounds();
        code.setOrigin(rect.left + rect.width/2.f, rect.top + rect.height/2.f);
        code.setPosition(WIDTH/2.f, HEIGHT/2.f);

        fftw_execute(plan);
        float *peaks = (float*)(malloc(output_size*sizeof(float)));
        for(int i = 0; i < output_size; i++) {
            peaks[i] = output_buffer[i][0]*output_buffer[i][0] + output_buffer[i][1]*output_buffer[i][1];
        }
        VertexArray lines(LinesStrip, output_size);
        for(int i = 0; i < output_size; i++) {
            lines[i].color = Color::Black;
            lines[i].position = Vector2f(HEIGHT, HEIGHT - peaks[i]*HEIGHT*20);
        }
        free(peaks);
        window.clear(Color(sig.color));
        window.draw(text);
        window.draw(code);
        /* window.draw(lines); */
        window.display();
    }

    rec.stop();
    fftw_free(INPUT_BUFFER);
    fftw_free(output_buffer);
    fftw_destroy_plan(plan);

    return 0;
}

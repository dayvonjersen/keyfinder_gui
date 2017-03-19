#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>
#include <exception>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include "keyfinder/keyfinder.h"
#include "./fft.cc"

#define WINDOW_WIDTH  320.f
#define WINDOW_HEIGHT 320.f
#define NUM_BANDS 32
#define DB_RANGE 40
static float logscale[NUM_BANDS + 1];
static float s_bars[NUM_BANDS];
#define OSC_SAMPLE_COUNT 512
static float s_osc[OSC_SAMPLE_COUNT];

static int visMode = 0; // odd bars, even osc

static void make_log_graph (const float* freq, float* graph) {
    for (int i = 0; i < NUM_BANDS; i++) {
        /* sum up values in freq array between logscale[i] and logscale[i + 1],
           including fractional parts */
        int a = ceilf (logscale[i]);
        int b = floorf (logscale[i + 1]);
        float sum = 0;

        if (b < a) {
            sum += freq[b] * (logscale[i + 1] - logscale[i]);
        } else {
            if (a > 0) {
                sum += freq[a - 1] * (a - logscale[i]);
            }
            for (; a < b; a ++) {
                sum += freq[a];
            }
            if (b < 256) {
                sum += freq[b] * (logscale[i + 1] - b);
            }
        }

        /* fudge factor to make the graph have the same overall height as a
           12-band one no matter how many bands there are */
        sum *= (float) NUM_BANDS / 12;

        /* convert to dB */
        float val = 20 * log10f (sum);

        /* scale (-DB_RANGE, 0.0) to (0.0, 1.0) */
        val = 1 + val / DB_RANGE;

        if(val < 0.0f) val = 0.0f;
        if(val > 1.0f) val = 1.0f;
        graph[i] = val; 
    }
}

struct KeySignature {
    const char*  text;
    const char*  code;
    unsigned int color;
};

static std::map<KeyFinder::key_t, KeySignature> KeySignatureMap;

void initKeySignatureMap() {
    KeySignatureMap[KeyFinder::A_FLAT_MINOR] = {"A Flat Minor",   "1A", 0xb8ffe1ff};
    KeySignatureMap[KeyFinder::E_FLAT_MINOR] = {"E Flat Minor",   "2A", 0xc2ffc6ff};
    KeySignatureMap[KeyFinder::B_FLAT_MINOR] = {"B Flat Minor",   "3A", 0xd2f7a7ff};
    KeySignatureMap[KeyFinder::F_MINOR]      = {"F Minor",        "4A", 0xe4e2a9ff};
    KeySignatureMap[KeyFinder::C_MINOR]      = {"C Minor",        "5A", 0xf6c4abff};
    KeySignatureMap[KeyFinder::G_MINOR]      = {"G Minor",        "6A", 0xffafb8ff};
    KeySignatureMap[KeyFinder::D_MINOR]      = {"D Minor",        "7A", 0xf7aeccff};
    KeySignatureMap[KeyFinder::A_MINOR]      = {"A Minor",        "8A", 0xe2aeecff};
    KeySignatureMap[KeyFinder::E_MINOR]      = {"E Minor",        "9A", 0xd1aefeff};
    KeySignatureMap[KeyFinder::B_MINOR]      = {"B Minor",       "10A", 0xc5c1feff};
    KeySignatureMap[KeyFinder::G_FLAT_MINOR] = {"F Sharp Minor", "11A", 0xb6e5ffff};
    KeySignatureMap[KeyFinder::D_FLAT_MINOR] = {"D Flat Minor",  "12A", 0xaefefdff};

    KeySignatureMap[KeyFinder::B_MAJOR]      = {"B Major",       "1B", 0x8effd1ff};
    KeySignatureMap[KeyFinder::G_FLAT_MAJOR] = {"F Sharp Major", "2B", 0x9fff9eff};
    KeySignatureMap[KeyFinder::D_FLAT_MAJOR] = {"D Flat Major",  "3B", 0xbaf976ff};
    KeySignatureMap[KeyFinder::A_FLAT_MAJOR] = {"A Flat Major",  "4B", 0xd5ce74ff};
    KeySignatureMap[KeyFinder::E_FLAT_MAJOR] = {"E Flat Major",  "5B", 0xf3a47bff};
    KeySignatureMap[KeyFinder::B_FLAT_MAJOR] = {"B Flat Major",  "6B", 0xff7988ff};
    KeySignatureMap[KeyFinder::F_MAJOR]      = {"F Major",       "7B", 0xf079b1ff};
    KeySignatureMap[KeyFinder::C_MAJOR]      = {"C Major",       "8B", 0xcf7fe2ff};
    KeySignatureMap[KeyFinder::G_MAJOR]      = {"G Major",       "9B", 0xb67fffff};
    KeySignatureMap[KeyFinder::D_MAJOR]      = {"D Major",      "10B", 0x9fa4ffff};
    KeySignatureMap[KeyFinder::A_MAJOR]      = {"A Major",      "11B", 0x82dfffff};
    KeySignatureMap[KeyFinder::E_MAJOR]      = {"E Major",      "12B", 0x7efffbff};

    KeySignatureMap[KeyFinder::SILENCE]      = {"(silence)",      "",  0x999999ff};
}

static KeyFinder::KeyFinder k;
static KeyFinder::AudioData a;
static KeyFinder::Workspace w;
static KeyFinder::key_t latest_key;

const static int SAMPLE_RATE = 44100;

const int packetSize = 44100*5;

void initWorkspace() {
    a = {};
    w = {};
    a.setFrameRate(SAMPLE_RATE);
    a.setChannels(2);
    a.addToSampleCount(packetSize);
}

void do_keyfind(float* bounded, size_t sampleCount) {
    try {
    for(int i = 0; i < sampleCount; i+=packetSize) {
        for(int j = 0; j < packetSize && i+j < sampleCount; j++) {
            a.setSample(j, bounded[i+j]);
        }
        k.progressiveChromagram(a, w);
        KeyFinder::key_t key = k.keyOfChromagram(w);
        if(latest_key != key) {
            latest_key = key;
            KeySignature sig = KeySignatureMap[latest_key];
            std::cout << a.getSampleCount() << " " << sig.text << std::endl;
        }
    }
    } catch(const KeyFinder::Exception& e) {
        std::cerr << "Exception:" << e.what() << "\n";
        return;
    }
    free(bounded);
}

void calc_osc(float *mono) {
    for(int i = 0; i < OSC_SAMPLE_COUNT; i++) {
        s_osc[i] = (mono[i] + 1.f / 2.f);
    }
}

void calc_bars(float *mono, float *freq) {
    calc_freq(mono, freq);
    make_log_graph(freq, s_bars);
}

void draw_osc(sf::RenderWindow& window, sf::Color color) {
    sf::VertexArray lines;
    sf::VertexArray lines2;
    sf::VertexArray lines3;
    lines = sf::VertexArray(sf::LineStrip, OSC_SAMPLE_COUNT);
    lines2 = sf::VertexArray(sf::LineStrip, OSC_SAMPLE_COUNT);
    lines3 = sf::VertexArray(sf::LineStrip, OSC_SAMPLE_COUNT);
    float sliceWidth = WINDOW_WIDTH / (float)OSC_SAMPLE_COUNT;
    float x = 0;
    for(int i = 0; i < OSC_SAMPLE_COUNT; i++) {
        float y = s_osc[i] * WINDOW_HEIGHT;
        lines[i].position = sf::Vector2f(x, y);
        lines[i].color = color;
        lines2[i].position = sf::Vector2f(x, y+1.f);
        lines2[i].color = color;
        lines3[i].position = sf::Vector2f(x, y-1.f);
        lines3[i].color = color;
        x += sliceWidth;
    }
    window.draw(lines);
    window.draw(lines2);
    window.draw(lines3);
}

void draw_bars(sf::RenderWindow& window, sf::Color color) {
    float padX = 1.f;
    float w = (WINDOW_WIDTH - padX*NUM_BANDS) / NUM_BANDS;
    for(int i = 0; i < NUM_BANDS; i++) {
        float x = i*w + i*padX;
        sf::RectangleShape bar;
        bar.setSize(sf::Vector2f(w, WINDOW_HEIGHT*s_bars[i]));
        bar.setFillColor(color);
        bar.setPosition(x, WINDOW_HEIGHT - WINDOW_HEIGHT*s_bars[i]);
        window.draw(bar);
    }
}

class CustomRecorder : public sf::SoundRecorder {
    public:
        bool onProcessSamples(const sf::Int16* samples, size_t sampleCount);
};

bool CustomRecorder::onProcessSamples(const sf::Int16* samples, size_t sampleCount) {
    float *bounded = (float*)malloc(sampleCount*sizeof(float));
    for(int i = 0; i < sampleCount; i++) {
        float samp = ((float)samples[i] / 32768.0);
        if(samp > 1)  samp = 1;
        if(samp < -1) samp = -1;
        bounded[i] = samp;
    }
    
    float *mono = (float*)malloc(512*sizeof(float));
    float *freq = (float*)malloc(256*sizeof(float));
    for(int i = 0; i < sampleCount; i+=512) {
        for(int j = 0; j < 512; j++) {
            if(i+j >= sampleCount) goto done;
            mono[j] = bounded[i+j];
        }
        if(visMode&1) {
            calc_bars(mono, freq);
        } else {
            calc_osc(mono);
        }    

    }    
done:
    free(mono);
    free(freq);

    std::thread t(do_keyfind, std::ref(bounded), sampleCount);
    t.detach();
    return true;
}


int main() {
    for(int i = 0; i <= NUM_BANDS; i++) {
        logscale[i] = powf(256, (float) i /NUM_BANDS) - 0.5f;
    }
    std::memset(s_bars, 0, sizeof(s_bars));

    /* {{{ */
    if(!sf::SoundRecorder::isAvailable()) {
        std::cerr << "sf::SoundRecorder::isAvailable() == false\n";
        return 1;
    }
start:
    std::vector<std::string> devices = sf::SoundRecorder::getAvailableDevices();
    std::string default_device = sf::SoundRecorder::getDefaultDevice();
    int default_device_index;
    int i = 0;
    for(std::string device : devices) {
        std::cout << "[" << i << "] " << device;
        if(device == default_device) {
            default_device_index = i;
            std::cout << " (default)";
        }
        i++;
        std::cout << "\n";
    }
    std::cout << "\nCHOOSE A DEVICE: ";
    int choice;
    std::cin >> std::noskipws >> choice;
    if(choice == -1) {
        choice = default_device_index;
    } else if(choice >= i) {
        std::cerr << "Bzzt.\n\n";
        goto start;
    }
    CustomRecorder rec;
    if(!rec.setDevice(devices[choice])) {
        std::cerr << "Device selection failed.\n";
        return 1;
    }
    /* 
     * }}} */
 
    initKeySignatureMap();
    initWorkspace();
    rec.start();
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "fixing fft");
    window.setFramerateLimit(60);

    sf::Font font;
    font.loadFromFile("sfns.ttf");

    sf::Texture bgTexture;
    bgTexture.loadFromFile("./camelotHarmonicMixing--clean.png");
    auto dim = bgTexture.getSize();
    sf::Sprite bgSprite;
    bgSprite.setTexture(bgTexture);
    bgSprite.setScale(WINDOW_WIDTH / dim.x, WINDOW_HEIGHT / dim.y);

    KeyFinder::key_t lastKey;

    while(window.isOpen()) {
        sf::Event e;
        while(window.pollEvent(e)) {
            if(e.type == sf::Event::Closed || (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Q)) {
                window.close();
            }
            if(e.type == sf::Event::KeyPressed) {
                if(e.key.code == sf::Keyboard::R) {
                    initWorkspace();
                }
                else if(e.key.code == sf::Keyboard::O) {
                    visMode++;
                }
            }
        }
        window.setActive();

        KeySignature sig = KeySignatureMap[latest_key];
        
        if(lastKey != latest_key) {
            window.setTitle(sf::String(sig.text) + " - " + sf::String(sig.code));
            lastKey = latest_key;
        }

        sf::Text text(sig.text, font);
        text.setCharacterSize(12);
        text.setFillColor(sf::Color::Black);
        sf::FloatRect rect = text.getLocalBounds();
        text.setOrigin(rect.left + rect.width/2.f, 0);
        text.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT/2.f - rect.height*2.f);

        sf::Text code(sig.code, font);
        code.setCharacterSize(28);
        code.setFillColor(sf::Color::Black);
        rect = code.getLocalBounds();
        code.setOrigin(rect.left + rect.width/2.f, rect.top - rect.height/2.f);
        code.setPosition(WINDOW_WIDTH/2.f, WINDOW_HEIGHT/2.f);

        window.clear(sf::Color::White);

        window.draw(bgSprite);

        sf::Color color = sf::Color(sig.color & 0xffffff00 | 0xe5);
        if(visMode&1) {
            draw_bars(window, color);
        } else {
            draw_osc(window, color);
        }

        window.draw(text);
        window.draw(code);

        window.display();
    }
    rec.stop();

    return 0;
}

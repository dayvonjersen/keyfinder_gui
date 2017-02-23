#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>

#include <SFML/Audio/SoundBufferRecorder.hpp>
#include <SFML/Audio/SoundRecorder.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Event.hpp>

#include "./fft.cc"

#define WINDOW_WIDTH  320.f
#define WINDOW_HEIGHT 320.f
#define NUM_BANDS 32
#define DB_RANGE 40
static float logscale[NUM_BANDS + 1];
static float s_bars[NUM_BANDS];

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

class CustomRecorder : public sf::SoundRecorder {
    public:
        bool onProcessSamples(const sf::Int16* samples, size_t sampleCount);
};

bool CustomRecorder::onProcessSamples(const sf::Int16* samples, size_t sampleCount) {
    /* std::memset(s_bars, 0, sizeof(s_bars)); */

    float *bounded = (float*)malloc(sampleCount*sizeof(float));
    for(int i = 0; i < sampleCount; i++) {
        float samp = ((float)samples[i] / 32768.0);
        if(samp > 1)  samp = 1;
        if(samp < -1) samp = -1;
        bounded[i] = samp;
    }

    float *mono = (float*)malloc(512*sizeof(float));
    float *freq = (float*)malloc(256*sizeof(float));
    for(int i = 0; i < sampleCount; i+=256) {

        for(int j = 0; j < 512; j++) {
            if(i+j >= sampleCount) goto done;
            mono[j] = i+j < sampleCount ? bounded[i+j] : 0.0f;
        }

        calc_freq(mono, freq);
        make_log_graph(freq, s_bars);

    }    
done:
    free(mono);
    free(freq);
    free(bounded);
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
 
    rec.start();
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "fixing fft");
    window.setFramerateLimit(60);

    while(window.isOpen()) {
        sf::Event e;
        while(window.pollEvent(e)) {
            if(e.type == sf::Event::Closed || (e.type == sf::Event::KeyPressed && e.key.code == sf::Keyboard::Q)) {
                window.close();
            }
        }
        window.setActive();
        window.clear(sf::Color::White);            
        for(int i = 0; i < NUM_BANDS; i++) {
            sf::RectangleShape bar;
            bar.setSize(sf::Vector2f(WINDOW_WIDTH/NUM_BANDS, WINDOW_HEIGHT*s_bars[i]));
            bar.setFillColor(sf::Color::Black);
            bar.setPosition(i*WINDOW_WIDTH/NUM_BANDS, WINDOW_HEIGHT - WINDOW_HEIGHT*s_bars[i]);
            window.draw(bar);
        }
        window.display();
    }
    rec.stop();

    return 0;
}

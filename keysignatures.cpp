typedef struct key {
    const char* text;
    const char* code;
    sf::Uint32  color;
} key;

/* static const struct KeySignature G_MINOR = {"G Minor", "6A", 0xffafb800}; */

static std::map<KeyFinder::key_t,key> KeySignature;
KeySignature[KeyFinder::G_MINOR] = {"G Minor", "6A", 0xffafb800};

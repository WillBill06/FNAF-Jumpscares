#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/loader/Setting.hpp>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <queue>

using namespace geode::prelude;

// ========== Global state ==========
struct ScareRequest {
    std::string type;
    bool isRandom;
};
static std::queue<ScareRequest> s_scareQueue;
static bool s_isProcessing = false;

// ========== Forward declarations ==========
void queueScare(std::string scareType, bool isRandom);
void processNextScare();
void completeScare();
void resetAll();

// ========== Reset everything ==========
void resetAll() {
    FMODAudioEngine::sharedEngine()->stopAllEffects();
    s_isProcessing = false;
    while (!s_scareQueue.empty()) s_scareQueue.pop();

    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (scene) {
        auto children = scene->getChildren();
        for (unsigned int i = 0; i < children->count(); i++) {
            auto node = dynamic_cast<CCNode*>(children->objectAtIndex(i));
            if (node && (node->getTag() == 99999 || node->getTag() == 99998)) {
                node->removeFromParentAndCleanup(true);
            }
        }
    }
}

// ========== Complete callback ==========
void completeScare() {
    FMODAudioEngine::sharedEngine()->stopAllEffects();
    s_isProcessing = false;
    processNextScare();
}

// ========== CCCallFunc helper ==========
class CompleteHelper : public cocos2d::CCObject {
public:
    void complete() { completeScare(); }
    static CompleteHelper* create() { return new CompleteHelper(); }
};

// ========== Dice roll ==========
bool rollDiceCheck(int chance) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(1, 100);
    return distr(gen) <= chance;
}

// ========== Preload assets ==========
void preloadAssets() {
    auto configPath = Mod::get()->getConfigDir();
    auto texCache = CCTextureCache::sharedTextureCache();

    std::vector<std::string> imageNames = {
        "freddy", "bonnie", "chica", "foxy1", "foxy", "toyfreddy",
        "witheredbonnie", "toychica", "mangled", "toybonnie",
        "witheredfreddy", "witheredchica", "puppet", "springtrap",
        "nightmarebonnie", "nightmarefreddy", "nightmarechica",
        "nightmarefoxy", "nightmarefredbear", "nightmaremangled",
        "plushtrap", "nightmarebb", "baby", "ballora", "funtimefoxy",
        "funtimefreddy", "ennard", "bonbon", "lefty", "moltenfreddy",
        "mrhippo", "rockstarchica", "scrapbaby", "afton",
        "nightmare", "nightmarionne", "phantomchica", "balloonboy",
        "phantomfoxy", "phantomfreddy", "goldenfreddy"
    };

    for (const auto& name : imageNames) {
        for (const auto& ext : {".gif", ".png", ".jpg"}) {
            std::string path = (configPath / (name + ext)).string();
            if (std::filesystem::exists(path)) {
                texCache->addImage(path.c_str(), true);
                break;
            }
        }
    }
    log::info("Preloaded all assets (texture cache)");
}

// ========== Execute a single scare ==========
void executeScare(std::string scareType, bool /*isRandom*/) {
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto configPath = Mod::get()->getConfigDir();

    std::string chosenChar = "";
    std::string sfxFileName = "fnaf1.mp3";
    std::string fileExtension = ".gif";
    float cooldown = 1.325f;
    float targetVolume = 0.35f;

    // Character pools (same as AHK)
    std::vector<std::string> fnaf1 = {"freddy", "bonnie", "chica", "foxy1"};
    std::vector<std::string> fnaf2 = {"foxy", "toyfreddy", "witheredbonnie", "toychica", "mangled", "toybonnie", "witheredfreddy", "witheredchica", "puppet"};
    std::vector<std::string> fnaf3 = {"springtrap"};
    std::vector<std::string> fnaf4 = {"nightmarebonnie", "nightmarefreddy", "nightmarechica", "nightmarefoxy", "nightmarefredbear", "nightmaremangled", "plushtrap", "nightmarebb"};
    std::vector<std::string> fnaf5 = {"baby", "ballora", "funtimefoxy", "funtimefreddy", "ennard", "bonbon"};
    std::vector<std::string> fnaf6 = {"lefty", "moltenfreddy", "mrhippo", "rockstarchica", "scrapbaby", "afton"};

    std::vector<std::string> all_characters = {"freddy", "bonnie", "chica", "foxy1", "foxy", "toyfreddy", "witheredbonnie", "toychica", "mangled", "toybonnie", "witheredfreddy", "witheredchica", "puppet", "springtrap", "nightmarebonnie", "nightmarefreddy", "nightmarechica", "nightmarefoxy", "nightmarefredbear", "nightmaremangled", "plushtrap", "nightmarebb", "baby", "ballora", "funtimefoxy", "funtimefreddy", "ennard", "bonbon", "lefty", "moltenfreddy", "mrhippo", "rockstarchica", "scrapbaby", "afton"};
    std::vector<std::string> nightmare_pool = {"nightmare", "nightmarionne"};
    std::vector<std::string> phantom_pool = {"phantomchica", "balloonboy", "phantomfoxy", "phantomfreddy"};

    static std::random_device rd;
    static std::mt19937 gen(rd());

    // Select character & audio, set volume
    if (scareType == "Standard") {
        std::uniform_int_distribution<> distr(0, all_characters.size() - 1);
        chosenChar = all_characters[distr(gen)];
        if (std::find(fnaf1.begin(), fnaf1.end(), chosenChar) != fnaf1.end()) {
            sfxFileName = "fnaf1.mp3";
            targetVolume = 0.35f;
        } else if (std::find(fnaf2.begin(), fnaf2.end(), chosenChar) != fnaf2.end()) {
            sfxFileName = "fnaf2.mp3";
            targetVolume = 0.30f;
        } else if (std::find(fnaf3.begin(), fnaf3.end(), chosenChar) != fnaf3.end()) {
            sfxFileName = "phantom.mp3";
            targetVolume = 0.55f;
        } else if (std::find(fnaf4.begin(), fnaf4.end(), chosenChar) != fnaf4.end()) {
            sfxFileName = "fnaf4.mp3";
            targetVolume = 0.70f;
        } else if (std::find(fnaf5.begin(), fnaf5.end(), chosenChar) != fnaf5.end()) {
            sfxFileName = "fnaf5.mp3";
            targetVolume = 0.35f;
        } else if (std::find(fnaf6.begin(), fnaf6.end(), chosenChar) != fnaf6.end()) {
            sfxFileName = "fnaf6.mp3";
            targetVolume = 0.55f;
        }
        cooldown = 1.325f;
    }
    else if (scareType == "Nightmare") {
        std::uniform_int_distribution<> distr(0, nightmare_pool.size() - 1);
        chosenChar = nightmare_pool[distr(gen)];
        sfxFileName = "Distortion.mp3";
        fileExtension = ".png";
        targetVolume = 1.0f;
        cooldown = 4.64f;
    }
    else if (scareType == "Phantom") {
        std::uniform_int_distribution<> distr(0, phantom_pool.size() - 1);
        chosenChar = phantom_pool[distr(gen)];
        sfxFileName = "phantom.mp3";
        targetVolume = 0.70f;
        cooldown = 7.37f;
    }
    else if (scareType == "Golden") {
        chosenChar = "goldenfreddy";
        sfxFileName = "goldenfreddy.mp3";
        fileExtension = std::filesystem::exists(configPath / "goldenfreddy.png") ? ".png" : ".jpg";
        targetVolume = 0.70f;
        cooldown = 17.825f;
    }

    std::string fullImgPath = (configPath / (chosenChar + fileExtension)).string();
    std::string fullSfxPath = (configPath / sfxFileName).string();

    if (!std::filesystem::exists(fullImgPath)) {
        log::warn("Image file not found: {}", fullImgPath);
        s_isProcessing = false;
        processNextScare();
        return;
    }

    auto scareSprite = CCSprite::create(fullImgPath.c_str());
    if (!scareSprite) {
        log::warn("Failed to create sprite from: {}", fullImgPath);
        s_isProcessing = false;
        processNextScare();
        return;
    }

    scareSprite->setPosition({winSize.width / 2, winSize.height / 2});
    scareSprite->setScaleX(winSize.width / scareSprite->getContentSize().width);
    scareSprite->setScaleY(winSize.height / scareSprite->getContentSize().height);
    scareSprite->setVisible(false);
    scareSprite->setTag(99999);

    if (scareType == "Standard") {
        scareSprite->setOpacity(240);
    } else if (scareType == "Nightmare") {
        scareSprite->setOpacity(255);
    } else if (scareType == "Phantom") {
        scareSprite->setOpacity(255);
    }

    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) {
        s_isProcessing = false;
        processNextScare();
        return;
    }

    auto* audio = FMODAudioEngine::sharedEngine();
    audio->playEffect(fullSfxPath.c_str(), 1.0f, 0.0f, targetVolume);

    CCArray* scareActions = CCArray::create();

    if (scareType == "Standard") {
        auto flickerCycle = CCSequence::create(
            CCShow::create(), CCDelayTime::create(0.140f),
            CCHide::create(), CCDelayTime::create(0.025f), nullptr
        );
        auto repeatAction = CCRepeat::create(flickerCycle, 5);
        scareActions->addObject(repeatAction);
    }
    else if (scareType == "Nightmare") {
        auto flickerCycle = CCSequence::create(
            CCShow::create(), CCDelayTime::create(0.210f),
            CCHide::create(), CCDelayTime::create(0.020f), nullptr
        );
        auto repeatAction = CCRepeat::create(flickerCycle, 18);
        scareActions->addObject(repeatAction);
    }
    else if (scareType == "Phantom") {
        auto flickerCycle = CCSequence::create(
            CCShow::create(), CCDelayTime::create(0.140f),
            CCHide::create(), CCDelayTime::create(0.020f), nullptr
        );
        auto repeatAction = CCRepeat::create(flickerCycle, 7);
        scareActions->addObject(repeatAction);

        auto blackLayer = CCLayerColor::create({0, 0, 0, 255});
        blackLayer->setOpacity(0);
        blackLayer->setTag(99998);
        scene->addChild(blackLayer, 99998);

        CCArray* pulseActions = CCArray::create();
        for (int i = 0; i < 5; i++) {
            pulseActions->addObject(CCFadeTo::create(0.3f, 255));
            pulseActions->addObject(CCDelayTime::create(0.45f));
            pulseActions->addObject(CCFadeTo::create(0.3f, 0));
            pulseActions->addObject(CCDelayTime::create(0.1f));
        }
        pulseActions->addObject(cocos2d::CCRemoveSelf::create());
        blackLayer->runAction(CCSequence::create(pulseActions));

        std::string breathingPath = (configPath / "breathing.wav").string();
        audio->playEffect(breathingPath.c_str(), 1.0f, 0.0f, 0.60f);
    }
    else if (scareType == "Golden") {
        for (int i = 0; i < 33; i++) {
            GLubyte opacity = (i % 2 == 0) ? 255 : 245;
            scareActions->addObject(CCFadeTo::create(0.0f, opacity));
            scareActions->addObject(CCShow::create());
            scareActions->addObject(CCDelayTime::create(0.500f));
            scareActions->addObject(CCHide::create());
            scareActions->addObject(CCDelayTime::create(0.025f));
        }
    }

    scareActions->addObject(cocos2d::CCRemoveSelf::create());
    scareSprite->runAction(CCSequence::create(scareActions));
    scene->addChild(scareSprite, 99999);

    auto resetNode = CCNode::create();
    scene->addChild(resetNode);
    auto resetSeq = CCSequence::create(
        CCDelayTime::create(cooldown),
        CCCallFunc::create(CompleteHelper::create(), callfunc_selector(CompleteHelper::complete)),
        cocos2d::CCRemoveSelf::create(),
        nullptr
    );
    resetNode->runAction(resetSeq);
}

// ========== Queue management ==========
void queueScare(std::string scareType, bool isRandom) {
    s_scareQueue.push({scareType, isRandom});
    processNextScare();
}

void processNextScare() {
    if (s_isProcessing) return;
    if (s_scareQueue.empty()) return;

    s_isProcessing = true;
    ScareRequest req = s_scareQueue.front();
    s_scareQueue.pop();

    executeScare(req.type, req.isRandom);
}

// ========== PlayLayer modification (random timers + level reset) ==========
class $modify(MyPlayLayer, PlayLayer) {
    void evaluateStandardTimer(float dt) {
        int chance = static_cast<int>(Mod::get()->getSettingValue<int64_t>("pct-standard"));
        if (rollDiceCheck(chance)) queueScare("Standard", true);
    }
    void evaluateNightmareTimer(float dt) {
        int chance = static_cast<int>(Mod::get()->getSettingValue<int64_t>("pct-nightmare"));
        if (rollDiceCheck(chance)) queueScare("Nightmare", true);
    }
    void evaluatePhantomTimer(float dt) {
        int chance = static_cast<int>(Mod::get()->getSettingValue<int64_t>("pct-phantom"));
        if (rollDiceCheck(chance)) queueScare("Phantom", true);
    }
    void evaluateGoldenTimer(float dt) {
        int chance = static_cast<int>(Mod::get()->getSettingValue<int64_t>("pct-golden"));
        if (rollDiceCheck(chance)) queueScare("Golden", true);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontRunOnDeath) {
        if (!PlayLayer::init(level, useReplay, dontRunOnDeath)) return false;

        if (Mod::get()->getSettingValue<bool>("enable-random")) {
            float tStandard = static_cast<float>(Mod::get()->getSettingValue<int64_t>("int-standard"));
            float tNightmare = static_cast<float>(Mod::get()->getSettingValue<int64_t>("int-nightmare"));
            float tPhantom = static_cast<float>(Mod::get()->getSettingValue<int64_t>("int-phantom"));
            float tGolden = static_cast<float>(Mod::get()->getSettingValue<int64_t>("int-golden"));

            this->schedule(schedule_selector(MyPlayLayer::evaluateStandardTimer), tStandard);
            this->schedule(schedule_selector(MyPlayLayer::evaluateNightmareTimer), tNightmare);
            this->schedule(schedule_selector(MyPlayLayer::evaluatePhantomTimer), tPhantom);
            this->schedule(schedule_selector(MyPlayLayer::evaluateGoldenTimer), tGolden);
        }
        return true;
    }

    void onQuit() {
        resetAll();
        PlayLayer::onQuit();
    }

    void resetLevel() {
        resetAll();
        PlayLayer::resetLevel();
    }
};

// ========== Mod initialization – preload + keybind listeners ==========
$on_mod(Loaded) {
    preloadAssets();

    auto mod = Mod::get();

    // Listen for each keybind setting – keep alive with .leak()
    auto addListener = [mod](const std::string& settingID, const std::string& scareType) {
        KeybindSettingPressedEventV3(mod, settingID).listen([scareType](Keybind const&, bool down, bool repeat, double) -> ListenerResult {
            if (!repeat && down && Mod::get()->getSettingValue<bool>("enable-keybinds")) {
                log::info("Keybind pressed: {}", scareType);
                queueScare(scareType, false);
                return ListenerResult::Stop;
            }
            return ListenerResult::Propagate;
        }).leak(); // prevents the listener from being destroyed
    };

    addListener("hk-standard", "Standard");
    addListener("hk-nightmare", "Nightmare");
    addListener("hk-phantom", "Phantom");
    addListener("hk-golden", "Golden");

    log::info("Keybind listeners set up.");
}
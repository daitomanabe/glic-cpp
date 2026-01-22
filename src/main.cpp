#include "glic.hpp"
#include "effects.hpp"
#include "colorspaces.hpp"
#include "preset_loader.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <sstream>
#include <optional>

using namespace glic;

// Safe string to int conversion with default value
std::optional<int> safeStoi(const std::string& str) {
    try {
        size_t pos;
        int result = std::stoi(str, &pos);
        if (pos != str.size()) return std::nullopt;
        return result;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Safe string to float conversion with default value
std::optional<float> safeStof(const std::string& str) {
    try {
        size_t pos;
        float result = std::stof(str, &pos);
        if (pos != str.size()) return std::nullopt;
        return result;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// Parse comma-separated RGB values (e.g., "128,128,128")
bool parseRGB(const std::string& str, int& r, int& g, int& b) {
    std::istringstream ss(str);
    std::string token;
    std::vector<int> values;

    while (std::getline(ss, token, ',')) {
        auto val = safeStoi(token);
        if (!val.has_value()) return false;
        values.push_back(val.value());
    }

    if (values.size() != 3) return false;
    r = values[0];
    g = values[1];
    b = values[2];
    return true;
}

// Parse comma-separated XY values (e.g., "2,0")
bool parseXY(const std::string& str, int& x, int& y) {
    std::istringstream ss(str);
    std::string token;
    std::vector<int> values;

    while (std::getline(ss, token, ',')) {
        auto val = safeStoi(token);
        if (!val.has_value()) return false;
        values.push_back(val.value());
    }

    if (values.size() < 1) return false;
    x = values[0];
    y = (values.size() >= 2) ? values[1] : 0;
    return true;
}

// Get default presets directory (relative to executable or current directory)
std::string getDefaultPresetsDir(const char* programPath) {
    std::filesystem::path execPath(programPath);
    std::filesystem::path presetsPath = execPath.parent_path() / "presets";
    if (std::filesystem::exists(presetsPath)) {
        return presetsPath.string();
    }
    // Try current directory
    if (std::filesystem::exists("presets")) {
        return "presets";
    }
    // Try parent directory
    if (std::filesystem::exists("../presets")) {
        return "../presets";
    }
    return "presets";
}

void printUsage(const char* programName) {
    std::cout << "GLIC - GLitch Image Codec (C++ Version)\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " encode <input.png> <output.glic> [options]\n";
    std::cout << "  " << programName << " decode <input.glic> <output.png> [options]\n";
    std::cout << "  " << programName << " --list-presets [--presets-dir <path>]\n\n";
    std::cout << "Preset Options:\n";
    std::cout << "  --preset <name>          Load preset by name (e.g., 'default', 'colour_waves')\n";
    std::cout << "  --presets-dir <path>     Directory containing presets (default: ./presets)\n";
    std::cout << "  --list-presets           List all available presets\n\n";
    std::cout << "Encode Options:\n";
    std::cout << "  --colorspace <name>      Color space (default: HWB)\n";
    std::cout << "                           Options: RGB, HSB, HWB, OHTA, CMY, XYZ, YXY, LAB, LUV,\n";
    std::cout << "                                    HCL, YUV, YPbPr, YCbCr, YDbDr, GS, R-GGB-G\n";
    std::cout << "  --min-block <size>       Min block size (default: 2)\n";
    std::cout << "  --max-block <size>       Max block size (default: 256)\n";
    std::cout << "  --threshold <value>      Segmentation threshold (default: 15)\n";
    std::cout << "  --prediction <method>    Prediction method (default: PAETH)\n";
    std::cout << "                           Options: NONE, CORNER, H, V, DC, DCMEDIAN, MEDIAN, AVG,\n";
    std::cout << "                                    TRUEMOTION, PAETH, LDIAG, HV, JPEGLS, DIFF,\n";
    std::cout << "                                    REF, ANGLE, SAD, BSAD, RANDOM,\n";
    std::cout << "                                    SPIRAL, NOISE, GRADIENT, MIRROR, WAVE,\n";
    std::cout << "                                    CHECKERBOARD, RADIAL, EDGE\n";
    std::cout << "  --quantization <value>   Quantization value 0-255 (default: 110)\n";
    std::cout << "  --clamp <method>         Clamp method: none, mod256 (default: none)\n";
    std::cout << "  --wavelet <name>         Wavelet type (default: SYMLET8)\n";
    std::cout << "                           Options: NONE, HAAR, DB2-DB10, SYM2-SYM10, COIF1-COIF5\n";
    std::cout << "  --transform <type>       Transform type: fwt, wpt (default: fwt)\n";
    std::cout << "  --scale <value>          Transform scale (default: 20)\n";
    std::cout << "  --encoding <method>      Encoding method (default: packed)\n";
    std::cout << "                           Options: raw, packed, rle, delta, xor, zigzag\n";
    std::cout << "  --border <r,g,b>         Border color RGB (default: 128,128,128)\n";
    std::cout << "\nDecode Options (Post-Effects):\n";
    std::cout << "  --effect <name>          Apply post effect (can be used multiple times)\n";
    std::cout << "                           Basic: pixelate, scanline, chromatic, dither, posterize, glitch\n";
    std::cout << "                           Advanced: dct, sort, leak\n";
    std::cout << "  --effect-intensity <n>   Effect intensity 0-100 (default: 50)\n";
    std::cout << "  --effect-blocksize <n>   Block size for pixelate/glitch/dct/leak (default: 8)\n";
    std::cout << "  --effect-offset <x,y>    Chromatic aberration offset (default: 2,0)\n";
    std::cout << "  --effect-levels <n>      Posterize levels (default: 4)\n";
    std::cout << "  --effect-threshold <n>   Pixel sort threshold 0-255 (default: 50)\n";
    std::cout << "  --effect-sortmode <m>    Sort mode: brightness, hue, saturation, red, green, blue\n";
    std::cout << "  --effect-vertical        Enable vertical sorting (default: horizontal)\n";
    std::cout << "  --effect-leak <f>        Prediction leak amount 0.0-1.0 (default: 0.5)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << programName << " encode photo.png glitched.glic\n";
    std::cout << "  " << programName << " encode photo.png glitched.glic --colorspace HWB --prediction SPIRAL\n";
    std::cout << "  " << programName << " decode glitched.glic result.png --effect scanline --effect chromatic\n";
}

bool parseArgs(int argc, char* argv[], std::string& command, std::string& input,
               std::string& output, CodecConfig& config, PostEffectsConfig& postEffects,
               std::string& presetsDir, std::string& presetName) {
    if (argc < 2) {
        return false;
    }

    // Check for --list-presets first
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--list-presets") {
            command = "list-presets";
            // Look for --presets-dir
            for (int j = 1; j < argc; j++) {
                if (std::string(argv[j]) == "--presets-dir" && j + 1 < argc) {
                    presetsDir = argv[j + 1];
                }
            }
            return true;
        }
    }

    if (argc < 4) {
        return false;
    }

    command = argv[1];
    input = argv[2];
    output = argv[3];

    // Default config
    config = CodecConfig();
    postEffects = PostEffectsConfig();

    // Default effect config for new effects
    EffectConfig currentEffect;
    currentEffect.intensity = 50;
    currentEffect.blockSize = 8;
    currentEffect.offsetX = 2;
    currentEffect.offsetY = 0;
    currentEffect.levels = 4;
    currentEffect.seed = 12345;
    currentEffect.sortMode = PixelSortMode::BRIGHTNESS;
    currentEffect.threshold = 50;
    currentEffect.sortVertical = false;
    currentEffect.leakAmount = 0.5f;

    // Parse options
    for (int i = 4; i < argc; i++) {
        std::string arg = argv[i];

        // Preset options
        if (arg == "--preset" && i + 1 < argc) {
            presetName = argv[++i];
        }
        else if (arg == "--presets-dir" && i + 1 < argc) {
            presetsDir = argv[++i];
        }
        else if (arg == "--colorspace" && i + 1 < argc) {
            config.colorSpace = colorSpaceFromName(argv[++i]);
        }
        else if (arg == "--min-block" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                for (auto& ch : config.channels) ch.minBlockSize = val.value();
            }
        }
        else if (arg == "--max-block" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                for (auto& ch : config.channels) ch.maxBlockSize = val.value();
            }
        }
        else if (arg == "--threshold" && i + 1 < argc) {
            auto val = safeStof(argv[++i]);
            if (val.has_value()) {
                for (auto& ch : config.channels) ch.segmentationPrecision = val.value();
            }
        }
        else if (arg == "--prediction" && i + 1 < argc) {
            auto val = predictionFromName(argv[++i]);
            for (auto& ch : config.channels) ch.predictionMethod = val;
        }
        else if (arg == "--quantization" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                for (auto& ch : config.channels) ch.quantizationValue = val.value();
            }
        }
        else if (arg == "--clamp" && i + 1 < argc) {
            std::string val = argv[++i];
            ClampMethod cm = (val == "mod256") ? ClampMethod::MOD256 : ClampMethod::NONE;
            for (auto& ch : config.channels) ch.clampMethod = cm;
        }
        else if (arg == "--wavelet" && i + 1 < argc) {
            auto val = waveletFromName(argv[++i]);
            for (auto& ch : config.channels) ch.waveletType = val;
        }
        else if (arg == "--transform" && i + 1 < argc) {
            std::string val = argv[++i];
            TransformType tt = (val == "wpt") ? TransformType::WPT : TransformType::FWT;
            for (auto& ch : config.channels) ch.transformType = tt;
        }
        else if (arg == "--scale" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                for (auto& ch : config.channels) ch.transformScale = val.value();
            }
        }
        else if (arg == "--encoding" && i + 1 < argc) {
            auto val = encodingFromName(argv[++i]);
            for (auto& ch : config.channels) ch.encodingMethod = val;
        }
        else if (arg == "--border" && i + 1 < argc) {
            std::string colorStr = argv[++i];
            int r = 128, g = 128, b = 128;
            if (parseRGB(colorStr, r, g, b)) {
                config.borderColorR = static_cast<uint8_t>(std::clamp(r, 0, 255));
                config.borderColorG = static_cast<uint8_t>(std::clamp(g, 0, 255));
                config.borderColorB = static_cast<uint8_t>(std::clamp(b, 0, 255));
            }
        }
        // Post-effect options
        else if (arg == "--effect" && i + 1 < argc) {
            std::string effectName = argv[++i];
            EffectConfig effect = currentEffect;
            effect.type = effectFromName(effectName);
            if (effect.type != EffectType::NONE) {
                postEffects.effects.push_back(effect);
                postEffects.enabled = true;
            }
        }
        else if (arg == "--effect-intensity" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                currentEffect.intensity = val.value();
            }
        }
        else if (arg == "--effect-blocksize" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                currentEffect.blockSize = val.value();
            }
        }
        else if (arg == "--effect-offset" && i + 1 < argc) {
            std::string offsetStr = argv[++i];
            int x = 2, y = 0;
            if (parseXY(offsetStr, x, y)) {
                currentEffect.offsetX = x;
                currentEffect.offsetY = y;
            }
        }
        else if (arg == "--effect-levels" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                currentEffect.levels = val.value();
            }
        }
        else if (arg == "--effect-threshold" && i + 1 < argc) {
            auto val = safeStoi(argv[++i]);
            if (val.has_value()) {
                currentEffect.threshold = std::clamp(val.value(), 0, 255);
            }
        }
        else if (arg == "--effect-sortmode" && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "brightness") currentEffect.sortMode = PixelSortMode::BRIGHTNESS;
            else if (mode == "hue") currentEffect.sortMode = PixelSortMode::HUE;
            else if (mode == "saturation") currentEffect.sortMode = PixelSortMode::SATURATION;
            else if (mode == "red") currentEffect.sortMode = PixelSortMode::RED;
            else if (mode == "green") currentEffect.sortMode = PixelSortMode::GREEN;
            else if (mode == "blue") currentEffect.sortMode = PixelSortMode::BLUE;
        }
        else if (arg == "--effect-vertical") {
            currentEffect.sortVertical = true;
        }
        else if (arg == "--effect-leak" && i + 1 < argc) {
            auto val = safeStof(argv[++i]);
            if (val.has_value()) {
                currentEffect.leakAmount = std::clamp(val.value(), 0.0f, 1.0f);
            }
        }
        else if (arg == "--help" || arg == "-h") {
            return false;
        }
    }

    return true;
}

int main(int argc, char* argv[]) {
    std::string command, input, output;
    std::string presetsDir, presetName;
    CodecConfig config;
    PostEffectsConfig postEffects;

    // Get default presets directory
    presetsDir = getDefaultPresetsDir(argv[0]);

    if (!parseArgs(argc, argv, command, input, output, config, postEffects, presetsDir, presetName)) {
        printUsage(argv[0]);
        return 1;
    }

    std::transform(command.begin(), command.end(), command.begin(), ::tolower);

    // Handle list-presets command
    if (command == "list-presets") {
        auto presets = PresetLoader::listPresets(presetsDir);
        if (presets.empty()) {
            std::cout << "No presets found in: " << presetsDir << std::endl;
            return 1;
        }
        std::cout << "Available presets (" << presets.size() << "):\n";
        for (const auto& p : presets) {
            std::cout << "  " << p << "\n";
        }
        return 0;
    }

    // Load preset if specified
    if (!presetName.empty()) {
        std::cout << "Loading preset: " << presetName << std::endl;
        if (!PresetLoader::loadPresetByName(presetsDir, presetName, config)) {
            std::cerr << "Warning: Failed to load preset '" << presetName << "' from " << presetsDir << std::endl;
            std::cerr << "Continuing with default settings..." << std::endl;
        } else {
            std::cout << "Preset loaded successfully" << std::endl;
        }
    }

    GlicCodec codec(config);
    codec.setPostEffects(postEffects);

    if (command == "encode") {
        // Load input image
        std::vector<Color> pixels;
        int width, height;

        if (!loadImage(input, pixels, width, height)) {
            std::cerr << "Error: Failed to load image: " << input << std::endl;
            return 1;
        }

        std::cout << "Loaded image: " << width << "x" << height << std::endl;

        // Encode
        auto result = codec.encode(pixels.data(), width, height, output);

        if (!result.success) {
            std::cerr << "Error: " << result.error << std::endl;
            return 1;
        }

        std::cout << "Encoded to: " << output << std::endl;
    }
    else if (command == "decode") {
        // Decode
        auto result = codec.decode(input);

        if (!result.success) {
            std::cerr << "Error: " << result.error << std::endl;
            return 1;
        }

        // Save output image
        if (!saveImage(output, result.pixels, result.width, result.height)) {
            std::cerr << "Error: Failed to save image: " << output << std::endl;
            return 1;
        }

        std::cout << "Decoded to: " << output << " (" << result.width << "x" << result.height << ")" << std::endl;
    }
    else {
        std::cerr << "Error: Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}

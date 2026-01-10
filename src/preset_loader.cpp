#include "preset_loader.hpp"
#include <fstream>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <cmath>

namespace glic {

// Known preset keys
static const std::vector<std::string> FLOAT_KEYS = {
    "ch0trans", "ch1trans", "ch2trans",
    "ch0pred", "ch1pred", "ch2pred",
    "ch0min", "ch1min", "ch2min",
    "ch0max", "ch1max", "ch2max",
    "ch0quant", "ch1quant", "ch2quant",
    "ch0scale", "ch1scale", "ch2scale",
    "ch0compress", "ch1compress", "ch2compress",
    "ch0encoding", "ch1encoding", "ch2encoding",
    "ch0thr", "ch1thr", "ch2thr",
    "colorspace",
    "color_outside_r", "color_outside_g", "color_outside_b"
};

static const std::vector<std::string> FLOAT_ARRAY_KEYS = {
    "ch0clamp", "ch1clamp", "ch2clamp",
    "ch0transtype", "ch1transtype", "ch2transtype"
};

float PresetLoader::readJavaFloat(const uint8_t* data) {
    // Java floats are stored in big-endian format
    uint32_t bits = (static_cast<uint32_t>(data[0]) << 24) |
                    (static_cast<uint32_t>(data[1]) << 16) |
                    (static_cast<uint32_t>(data[2]) << 8) |
                    static_cast<uint32_t>(data[3]);

    float result;
    std::memcpy(&result, &bits, sizeof(float));
    return result;
}

int PresetLoader::findString(const std::vector<uint8_t>& data, const std::string& str, int startPos) {
    if (str.empty() || data.size() < str.size()) return -1;

    for (size_t i = startPos; i <= data.size() - str.size(); i++) {
        bool found = true;
        for (size_t j = 0; j < str.size(); j++) {
            if (data[i + j] != static_cast<uint8_t>(str[j])) {
                found = false;
                break;
            }
        }
        if (found) return static_cast<int>(i);
    }
    return -1;
}

bool PresetLoader::parseJavaHashMap(const std::vector<uint8_t>& data, PresetData& preset) {
    // Check Java serialization magic number (0xACED)
    if (data.size() < 4 || data[0] != 0xAC || data[1] != 0xED) {
        std::cerr << "Warning: Not a valid Java serialized file" << std::endl;
        return false;
    }

    // Java serialization format for HashMap entries:
    // TC_STRING (0x74) + length (2 bytes) + string data
    // followed by either:
    // - TC_OBJECT (0x73) + TC_REFERENCE (0x71) + reference handle for Float
    // - or first occurrence has full class descriptor
    // Then 4 bytes of float value

    // Parse float values
    for (const auto& key : FLOAT_KEYS) {
        int pos = findString(data, key);
        if (pos != -1) {
            // After the key string, look for the pattern:
            // "sq" (0x73 0x71) followed by reference handle, then float value
            // or "sr" for first Float definition followed by class data
            size_t searchStart = pos + key.length();
            size_t searchEnd = std::min(searchStart + 50, data.size() - 4);

            for (size_t i = searchStart; i < searchEnd; i++) {
                // Pattern 1: Reference to Float class (sq ~)
                // 0x73 0x71 0x00 0x7e 0x00 0x03 (or similar reference) followed by 4 bytes float
                if (i + 6 <= data.size() && data[i] == 0x73 && data[i + 1] == 0x71) {
                    // Skip the reference handle (4 bytes typically)
                    float val = readJavaFloat(&data[i + 6]);
                    if (!std::isnan(val) && !std::isinf(val) && val >= -10000.0f && val <= 10000.0f) {
                        preset.floatValues[key] = val;
                        break;
                    }
                }
                // Pattern 2: Direct Float object with class descriptor
                // After "xp" (end of class data) comes the float value
                if (i + 5 <= data.size() && data[i] == 'x' && data[i + 1] == 'p') {
                    float val = readJavaFloat(&data[i + 2]);
                    if (!std::isnan(val) && !std::isinf(val) && val >= -10000.0f && val <= 10000.0f) {
                        preset.floatValues[key] = val;
                        break;
                    }
                }
            }
        }
    }

    // Parse float array values (clamp and transtype)
    for (const auto& key : FLOAT_ARRAY_KEYS) {
        int pos = findString(data, key);
        if (pos != -1) {
            size_t searchStart = pos + key.length();
            size_t searchEnd = std::min(searchStart + 60, data.size() - 12);

            for (size_t i = searchStart; i < searchEnd; i++) {
                // Look for float array pattern: "ur" or "uq" followed by "[F" marker
                if (i + 2 < data.size() && (data[i] == 'u') && (data[i + 1] == 'r' || data[i + 1] == 'q')) {
                    // Find "[F" marker
                    for (size_t j = i + 2; j < i + 20 && j + 1 < data.size(); j++) {
                        if (data[j] == '[' && data[j + 1] == 'F') {
                            // Skip class descriptor, find "xp" + array length + data
                            for (size_t k = j + 2; k < j + 30 && k + 4 < data.size(); k++) {
                                if (data[k] == 'x' && data[k + 1] == 'p') {
                                    // Next 4 bytes are array length (big endian int)
                                    size_t arrLen = (static_cast<size_t>(data[k + 2]) << 24) |
                                                   (static_cast<size_t>(data[k + 3]) << 16) |
                                                   (static_cast<size_t>(data[k + 4]) << 8) |
                                                   static_cast<size_t>(data[k + 5]);
                                    if (arrLen > 0 && arrLen <= 10) {
                                        std::vector<float> arr;
                                        size_t arrStart = k + 6;
                                        for (size_t a = 0; a < arrLen && arrStart + 4 <= data.size(); a++) {
                                            float val = readJavaFloat(&data[arrStart]);
                                            arr.push_back(val);
                                            arrStart += 4;
                                        }
                                        if (!arr.empty()) {
                                            preset.floatArrayValues[key] = arr;
                                        }
                                    }
                                    goto next_array_key;
                                }
                            }
                        }
                    }
                }
            }
            next_array_key:;
        }
    }

    return !preset.floatValues.empty();
}

std::optional<PresetData> PresetLoader::parsePreset(const std::string& presetPath) {
    std::ifstream file(presetPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open preset file: " << presetPath << std::endl;
        return std::nullopt;
    }

    // Read entire file
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    file.close();

    PresetData preset;
    if (!parseJavaHashMap(data, preset)) {
        return std::nullopt;
    }

    return preset;
}

void PresetLoader::applyPresetToConfig(const PresetData& preset, CodecConfig& config) {
    // Apply colorspace
    auto it = preset.floatValues.find("colorspace");
    if (it != preset.floatValues.end()) {
        int cs = static_cast<int>(it->second);
        // Map Java GLIC colorspace indices to our ColorSpace enum
        // Java GLIC uses: 0=OHTA, 1=RGB, 2=CMY, etc.
        if (cs >= 0 && cs < static_cast<int>(ColorSpace::COUNT)) {
            config.colorSpace = static_cast<ColorSpace>(cs);
        }
    }

    // Apply border colors
    it = preset.floatValues.find("color_outside_r");
    if (it != preset.floatValues.end()) {
        config.borderColorR = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, it->second)));
    }
    it = preset.floatValues.find("color_outside_g");
    if (it != preset.floatValues.end()) {
        config.borderColorG = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, it->second)));
    }
    it = preset.floatValues.find("color_outside_b");
    if (it != preset.floatValues.end()) {
        config.borderColorB = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, it->second)));
    }

    // Apply channel-specific settings
    const char* channels[] = {"ch0", "ch1", "ch2"};
    for (int i = 0; i < 3; i++) {
        std::string prefix = channels[i];
        auto& ch = config.channels[i];

        // Min block size
        it = preset.floatValues.find(prefix + "min");
        if (it != preset.floatValues.end()) {
            ch.minBlockSize = static_cast<int>(std::max(1.0f, it->second));
        }

        // Max block size
        it = preset.floatValues.find(prefix + "max");
        if (it != preset.floatValues.end()) {
            ch.maxBlockSize = static_cast<int>(std::max(1.0f, it->second));
        }

        // Prediction method
        it = preset.floatValues.find(prefix + "pred");
        if (it != preset.floatValues.end()) {
            int pm = static_cast<int>(it->second);
            // Map Java prediction method indices
            if (pm >= -3 && pm < static_cast<int>(PredictionMethod::COUNT)) {
                ch.predictionMethod = static_cast<PredictionMethod>(pm);
            }
        }

        // Quantization
        it = preset.floatValues.find(prefix + "quant");
        if (it != preset.floatValues.end()) {
            ch.quantizationValue = static_cast<int>(std::max(0.0f, std::min(255.0f, it->second)));
        }

        // Transform scale
        it = preset.floatValues.find(prefix + "scale");
        if (it != preset.floatValues.end()) {
            ch.transformScale = static_cast<int>(it->second);
        }

        // Transform compress
        it = preset.floatValues.find(prefix + "compress");
        if (it != preset.floatValues.end()) {
            ch.transformCompress = it->second;
        }

        // Threshold (segmentation precision)
        it = preset.floatValues.find(prefix + "thr");
        if (it != preset.floatValues.end()) {
            ch.segmentationPrecision = it->second;
        }

        // Wavelet type (trans)
        it = preset.floatValues.find(prefix + "trans");
        if (it != preset.floatValues.end()) {
            int wt = static_cast<int>(it->second);
            if (wt >= 0 && wt < static_cast<int>(WaveletType::COUNT)) {
                ch.waveletType = static_cast<WaveletType>(wt);
            }
        }

        // Encoding method
        it = preset.floatValues.find(prefix + "encoding");
        if (it != preset.floatValues.end()) {
            // Map encoding: 0.5 seems to be PACKED in Java GLIC
            if (it->second < 0.25f) {
                ch.encodingMethod = EncodingMethod::RAW;
            } else if (it->second < 0.75f) {
                ch.encodingMethod = EncodingMethod::PACKED;
            } else {
                ch.encodingMethod = EncodingMethod::RLE;
            }
        }

        // Clamp
        auto arrIt = preset.floatArrayValues.find(prefix + "clamp");
        if (arrIt != preset.floatArrayValues.end() && !arrIt->second.empty()) {
            // If first value is > 0.5, use MOD256
            if (arrIt->second[0] > 0.5f) {
                ch.clampMethod = ClampMethod::MOD256;
            } else {
                ch.clampMethod = ClampMethod::NONE;
            }
        }

        // Transform type
        arrIt = preset.floatArrayValues.find(prefix + "transtype");
        if (arrIt != preset.floatArrayValues.end() && !arrIt->second.empty()) {
            if (arrIt->second[0] > 0.5f) {
                ch.transformType = TransformType::WPT;
            } else {
                ch.transformType = TransformType::FWT;
            }
        }
    }
}

bool PresetLoader::loadPreset(const std::string& presetPath, CodecConfig& config) {
    auto preset = parsePreset(presetPath);
    if (!preset.has_value()) {
        return false;
    }

    applyPresetToConfig(preset.value(), config);
    return true;
}

bool PresetLoader::loadPresetByName(const std::string& presetsDir, const std::string& presetName, CodecConfig& config) {
    std::string presetPath = presetsDir;
    if (!presetPath.empty() && presetPath.back() != '/' && presetPath.back() != '\\') {
        presetPath += '/';
    }
    presetPath += presetName;

    return loadPreset(presetPath, config);
}

std::vector<std::string> PresetLoader::listPresets(const std::string& presetsDir) {
    std::vector<std::string> presets;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(presetsDir)) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();
                // Skip hidden files
                if (!filename.empty() && filename[0] != '.') {
                    presets.push_back(filename);
                }
            }
        }
        std::sort(presets.begin(), presets.end());
    } catch (const std::exception& e) {
        std::cerr << "Error listing presets: " << e.what() << std::endl;
    }

    return presets;
}

} // namespace glic

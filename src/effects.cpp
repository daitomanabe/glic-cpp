#include "effects.hpp"
#include <cmath>
#include <algorithm>
#include <random>

namespace glic {

std::string effectName(EffectType et) {
    switch (et) {
        case EffectType::NONE: return "NONE";
        case EffectType::PIXELATE: return "PIXELATE";
        case EffectType::SCANLINE: return "SCANLINE";
        case EffectType::CHROMATIC_ABERRATION: return "CHROMATIC_ABERRATION";
        case EffectType::DITHER: return "DITHER";
        case EffectType::POSTERIZE: return "POSTERIZE";
        case EffectType::GLITCH_SHIFT: return "GLITCH_SHIFT";
        case EffectType::DCT_CORRUPT: return "DCT_CORRUPT";
        case EffectType::PIXEL_SORT: return "PIXEL_SORT";
        case EffectType::PREDICTION_LEAK: return "PREDICTION_LEAK";
        default: return "NONE";
    }
}

EffectType effectFromName(const std::string& name) {
    if (name == "PIXELATE" || name == "pixelate") return EffectType::PIXELATE;
    if (name == "SCANLINE" || name == "scanline") return EffectType::SCANLINE;
    if (name == "CHROMATIC_ABERRATION" || name == "CHROMATIC" || name == "chromatic") return EffectType::CHROMATIC_ABERRATION;
    if (name == "DITHER" || name == "dither") return EffectType::DITHER;
    if (name == "POSTERIZE" || name == "posterize") return EffectType::POSTERIZE;
    if (name == "GLITCH_SHIFT" || name == "GLITCH" || name == "glitch") return EffectType::GLITCH_SHIFT;
    if (name == "DCT_CORRUPT" || name == "DCT" || name == "dct") return EffectType::DCT_CORRUPT;
    if (name == "PIXEL_SORT" || name == "SORT" || name == "sort") return EffectType::PIXEL_SORT;
    if (name == "PREDICTION_LEAK" || name == "LEAK" || name == "leak") return EffectType::PREDICTION_LEAK;
    return EffectType::NONE;
}

void applyEffect(
    std::vector<Color>& pixels,
    int width,
    int height,
    const EffectConfig& config
) {
    switch (config.type) {
        case EffectType::PIXELATE:
            effectPixelate(pixels, width, height, config.blockSize);
            break;
        case EffectType::SCANLINE:
            effectScanline(pixels, width, height, config.intensity);
            break;
        case EffectType::CHROMATIC_ABERRATION:
            effectChromaticAberration(pixels, width, height, config.offsetX, config.offsetY);
            break;
        case EffectType::DITHER:
            effectDither(pixels, width, height, config.intensity);
            break;
        case EffectType::POSTERIZE:
            effectPosterize(pixels, width, height, config.levels);
            break;
        case EffectType::GLITCH_SHIFT:
            effectGlitchShift(pixels, width, height, config.blockSize, config.seed);
            break;
        case EffectType::DCT_CORRUPT:
            effectDctCorrupt(pixels, width, height, config.blockSize, config.intensity, config.seed);
            break;
        case EffectType::PIXEL_SORT:
            effectPixelSort(pixels, width, height, config.sortMode, config.threshold, config.sortVertical);
            break;
        case EffectType::PREDICTION_LEAK:
            effectPredictionLeak(pixels, width, height, config.blockSize, config.leakAmount, config.seed);
            break;
        default:
            break;
    }
}

void applyEffects(
    std::vector<Color>& pixels,
    int width,
    int height,
    const std::vector<EffectConfig>& effects
) {
    for (const auto& effect : effects) {
        applyEffect(pixels, width, height, effect);
    }
}

// PIXELATE - Reduce resolution in blocks
void effectPixelate(std::vector<Color>& pixels, int w, int h, int blockSize) {
    if (blockSize < 2) return;

    for (int by = 0; by < h; by += blockSize) {
        for (int bx = 0; bx < w; bx += blockSize) {
            // Calculate average color for block
            int sumR = 0, sumG = 0, sumB = 0, sumA = 0;
            int count = 0;

            for (int y = by; y < std::min(by + blockSize, h); y++) {
                for (int x = bx; x < std::min(bx + blockSize, w); x++) {
                    Color c = pixels[static_cast<size_t>(y * w + x)];
                    sumR += getR(c);
                    sumG += getG(c);
                    sumB += getB(c);
                    sumA += getA(c);
                    count++;
                }
            }

            Color avg = makeColor(
                static_cast<uint8_t>(sumR / count),
                static_cast<uint8_t>(sumG / count),
                static_cast<uint8_t>(sumB / count),
                static_cast<uint8_t>(sumA / count)
            );

            // Fill block with average
            for (int y = by; y < std::min(by + blockSize, h); y++) {
                for (int x = bx; x < std::min(bx + blockSize, w); x++) {
                    pixels[static_cast<size_t>(y * w + x)] = avg;
                }
            }
        }
    }
}

// SCANLINE - Add scanline effect
void effectScanline(std::vector<Color>& pixels, int w, int h, int intensity) {
    float factor = 1.0f - (static_cast<float>(intensity) / 100.0f) * 0.5f;

    for (int y = 0; y < h; y++) {
        if (y % 2 == 1) {  // Darken odd lines
            for (int x = 0; x < w; x++) {
                Color c = pixels[static_cast<size_t>(y * w + x)];
                pixels[static_cast<size_t>(y * w + x)] = makeColor(
                    static_cast<uint8_t>(getR(c) * factor),
                    static_cast<uint8_t>(getG(c) * factor),
                    static_cast<uint8_t>(getB(c) * factor),
                    getA(c)
                );
            }
        }
    }
}

// CHROMATIC_ABERRATION - Offset color channels
void effectChromaticAberration(std::vector<Color>& pixels, int w, int h, int offsetX, int offsetY) {
    std::vector<Color> result = pixels;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            // Red channel - offset in one direction
            int rxr = std::max(0, std::min(w - 1, x - offsetX));
            int ryr = std::max(0, std::min(h - 1, y - offsetY));
            uint8_t r = getR(pixels[static_cast<size_t>(ryr * w + rxr)]);

            // Green channel - no offset
            uint8_t g = getG(pixels[static_cast<size_t>(y * w + x)]);

            // Blue channel - offset in opposite direction
            int rxb = std::max(0, std::min(w - 1, x + offsetX));
            int ryb = std::max(0, std::min(h - 1, y + offsetY));
            uint8_t b = getB(pixels[static_cast<size_t>(ryb * w + rxb)]);

            result[static_cast<size_t>(y * w + x)] = makeColor(r, g, b, getA(pixels[static_cast<size_t>(y * w + x)]));
        }
    }

    pixels = std::move(result);
}

// DITHER - Add ordered dithering pattern
void effectDither(std::vector<Color>& pixels, int w, int h, int intensity) {
    // 4x4 Bayer matrix
    static const int bayer[4][4] = {
        {  0,  8,  2, 10 },
        { 12,  4, 14,  6 },
        {  3, 11,  1,  9 },
        { 15,  7, 13,  5 }
    };

    float scale = (static_cast<float>(intensity) / 100.0f) * 32.0f;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Color c = pixels[static_cast<size_t>(y * w + x)];
            float threshold = static_cast<float>(bayer[y % 4][x % 4] - 8) * scale / 16.0f;

            int r = std::max(0, std::min(255, static_cast<int>(getR(c) + threshold)));
            int g = std::max(0, std::min(255, static_cast<int>(getG(c) + threshold)));
            int b = std::max(0, std::min(255, static_cast<int>(getB(c) + threshold)));

            pixels[static_cast<size_t>(y * w + x)] = makeColor(
                static_cast<uint8_t>(r),
                static_cast<uint8_t>(g),
                static_cast<uint8_t>(b),
                getA(c)
            );
        }
    }
}

// POSTERIZE - Reduce color levels
void effectPosterize(std::vector<Color>& pixels, int w, int h, int levels) {
    if (levels < 2) levels = 2;
    if (levels > 256) levels = 256;

    float step = 255.0f / static_cast<float>(levels - 1);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Color c = pixels[static_cast<size_t>(y * w + x)];

            int r = static_cast<int>(std::round(getR(c) / step) * step);
            int g = static_cast<int>(std::round(getG(c) / step) * step);
            int b = static_cast<int>(std::round(getB(c) / step) * step);

            pixels[static_cast<size_t>(y * w + x)] = makeColor(
                static_cast<uint8_t>(std::max(0, std::min(255, r))),
                static_cast<uint8_t>(std::max(0, std::min(255, g))),
                static_cast<uint8_t>(std::max(0, std::min(255, b))),
                getA(c)
            );
        }
    }
}

// GLITCH_SHIFT - Random block displacement
void effectGlitchShift(std::vector<Color>& pixels, int w, int h, int blockSize, uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> shiftDist(-blockSize * 2, blockSize * 2);
    std::uniform_int_distribution<int> probDist(0, 100);

    std::vector<Color> result = pixels;

    for (int by = 0; by < h; by += blockSize) {
        if (probDist(rng) < 30) {  // 30% chance to glitch this row
            int shift = shiftDist(rng);

            for (int y = by; y < std::min(by + blockSize, h); y++) {
                for (int x = 0; x < w; x++) {
                    int srcX = ((x - shift) % w + w) % w;
                    result[static_cast<size_t>(y * w + x)] = pixels[static_cast<size_t>(y * w + srcX)];
                }
            }
        }
    }

    pixels = std::move(result);
}

// DCT_CORRUPT - Rosa Menkman inspired DCT block corruption
// Simulates JPEG compression artifacts by manipulating 8x8 blocks
void effectDctCorrupt(std::vector<Color>& pixels, int w, int h, int blockSize, int intensity, uint32_t seed) {
    if (blockSize < 2) blockSize = 8;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> probDist(0, 100);
    std::uniform_int_distribution<int> corruptTypeDist(0, 5);

    float scale = static_cast<float>(intensity) / 100.0f;

    for (int by = 0; by < h; by += blockSize) {
        for (int bx = 0; bx < w; bx += blockSize) {
            // Decide if this block should be corrupted
            if (probDist(rng) > static_cast<int>(scale * 50)) continue;

            int corruptType = corruptTypeDist(rng);

            // Calculate block average for some effects
            int avgR = 0, avgG = 0, avgB = 0;
            int count = 0;
            for (int y = by; y < std::min(by + blockSize, h); y++) {
                for (int x = bx; x < std::min(bx + blockSize, w); x++) {
                    Color c = pixels[static_cast<size_t>(y * w + x)];
                    avgR += getR(c);
                    avgG += getG(c);
                    avgB += getB(c);
                    count++;
                }
            }
            if (count > 0) {
                avgR /= count;
                avgG /= count;
                avgB /= count;
            }

            // Apply corruption to block
            for (int y = by; y < std::min(by + blockSize, h); y++) {
                for (int x = bx; x < std::min(bx + blockSize, w); x++) {
                    Color c = pixels[static_cast<size_t>(y * w + x)];
                    int r = getR(c);
                    int g = getG(c);
                    int b = getB(c);

                    // DCT basis function approximation (simplified cosine pattern)
                    int lx = x - bx;
                    int ly = y - by;
                    float basis = std::cos(static_cast<float>(lx * ly) * 0.5f);

                    switch (corruptType) {
                        case 0: // DC coefficient shift (uniform color shift)
                            r = std::clamp(r + static_cast<int>(avgR * 0.3f * scale), 0, 255);
                            g = std::clamp(g + static_cast<int>(avgG * 0.3f * scale), 0, 255);
                            b = std::clamp(b + static_cast<int>(avgB * 0.3f * scale), 0, 255);
                            break;
                        case 1: // AC coefficient boost (edge enhancement)
                            r = static_cast<int>(r + (r - avgR) * scale * basis);
                            g = static_cast<int>(g + (g - avgG) * scale * basis);
                            b = static_cast<int>(b + (b - avgB) * scale * basis);
                            break;
                        case 2: // Quantize heavily (banding)
                            r = (r / 32) * 32;
                            g = (g / 32) * 32;
                            b = (b / 32) * 32;
                            break;
                        case 3: // Shift block color (green unchanged)
                            r = (r + static_cast<int>(basis * 64 * scale)) % 256;
                            b = (b - static_cast<int>(basis * 64 * scale) + 256) % 256;
                            break;
                        case 4: // Posterize with DCT pattern
                            {
                                int levels = 4 + static_cast<int>(basis * 4);
                                float step = 255.0f / levels;
                                r = static_cast<int>(std::round(r / step) * step);
                                g = static_cast<int>(std::round(g / step) * step);
                                b = static_cast<int>(std::round(b / step) * step);
                            }
                            break;
                        case 5: // Complete block replacement (macroblock error)
                            r = avgR;
                            g = avgG;
                            b = avgB;
                            break;
                    }

                    pixels[static_cast<size_t>(y * w + x)] = makeColor(
                        static_cast<uint8_t>(std::clamp(r, 0, 255)),
                        static_cast<uint8_t>(std::clamp(g, 0, 255)),
                        static_cast<uint8_t>(std::clamp(b, 0, 255)),
                        getA(c)
                    );
                }
            }
        }
    }
}

// Helper functions for pixel sorting
namespace {

float getPixelBrightness(Color c) {
    return (getR(c) * 0.299f + getG(c) * 0.587f + getB(c) * 0.114f) / 255.0f;
}

float getPixelHue(Color c) {
    float r = getR(c) / 255.0f;
    float g = getG(c) / 255.0f;
    float b = getB(c) / 255.0f;

    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});
    float delta = maxVal - minVal;

    if (delta < 0.00001f) return 0.0f;

    float hue = 0.0f;
    if (maxVal == r) {
        hue = 60.0f * std::fmod((g - b) / delta, 6.0f);
    } else if (maxVal == g) {
        hue = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        hue = 60.0f * ((r - g) / delta + 4.0f);
    }

    if (hue < 0) hue += 360.0f;
    return hue / 360.0f;
}

float getPixelSaturation(Color c) {
    float r = getR(c) / 255.0f;
    float g = getG(c) / 255.0f;
    float b = getB(c) / 255.0f;

    float maxVal = std::max({r, g, b});
    float minVal = std::min({r, g, b});

    if (maxVal < 0.00001f) return 0.0f;
    return (maxVal - minVal) / maxVal;
}

float getPixelSortValue(Color c, PixelSortMode mode) {
    switch (mode) {
        case PixelSortMode::BRIGHTNESS:
            return getPixelBrightness(c);
        case PixelSortMode::HUE:
            return getPixelHue(c);
        case PixelSortMode::SATURATION:
            return getPixelSaturation(c);
        case PixelSortMode::RED:
            return getR(c) / 255.0f;
        case PixelSortMode::GREEN:
            return getG(c) / 255.0f;
        case PixelSortMode::BLUE:
            return getB(c) / 255.0f;
        default:
            return getPixelBrightness(c);
    }
}

} // anonymous namespace

// PIXEL_SORT - Kim Asendorf inspired pixel sorting
void effectPixelSort(std::vector<Color>& pixels, int w, int h, PixelSortMode mode, int threshold, bool vertical) {
    float thresholdNorm = static_cast<float>(threshold) / 255.0f;

    if (vertical) {
        // Sort columns
        for (int x = 0; x < w; x++) {
            int sortStart = -1;

            for (int y = 0; y <= h; y++) {
                bool inInterval = false;
                if (y < h) {
                    Color c = pixels[static_cast<size_t>(y * w + x)];
                    float val = getPixelBrightness(c);
                    inInterval = (val > thresholdNorm && val < (1.0f - thresholdNorm * 0.5f));
                }

                if (inInterval && sortStart == -1) {
                    sortStart = y;
                } else if (!inInterval && sortStart != -1) {
                    // Sort the interval
                    std::vector<Color> interval;
                    for (int sy = sortStart; sy < y; sy++) {
                        interval.push_back(pixels[static_cast<size_t>(sy * w + x)]);
                    }

                    std::sort(interval.begin(), interval.end(),
                        [mode](Color a, Color b) {
                            return getPixelSortValue(a, mode) < getPixelSortValue(b, mode);
                        });

                    for (int sy = sortStart; sy < y; sy++) {
                        pixels[static_cast<size_t>(sy * w + x)] = interval[static_cast<size_t>(sy - sortStart)];
                    }

                    sortStart = -1;
                }
            }
        }
    } else {
        // Sort rows
        for (int y = 0; y < h; y++) {
            int sortStart = -1;

            for (int x = 0; x <= w; x++) {
                bool inInterval = false;
                if (x < w) {
                    Color c = pixels[static_cast<size_t>(y * w + x)];
                    float val = getPixelBrightness(c);
                    inInterval = (val > thresholdNorm && val < (1.0f - thresholdNorm * 0.5f));
                }

                if (inInterval && sortStart == -1) {
                    sortStart = x;
                } else if (!inInterval && sortStart != -1) {
                    // Sort the interval
                    std::vector<Color> interval;
                    for (int sx = sortStart; sx < x; sx++) {
                        interval.push_back(pixels[static_cast<size_t>(y * w + sx)]);
                    }

                    std::sort(interval.begin(), interval.end(),
                        [mode](Color a, Color b) {
                            return getPixelSortValue(a, mode) < getPixelSortValue(b, mode);
                        });

                    for (int sx = sortStart; sx < x; sx++) {
                        pixels[static_cast<size_t>(y * w + sx)] = interval[static_cast<size_t>(sx - sortStart)];
                    }

                    sortStart = -1;
                }
            }
        }
    }
}

// PREDICTION_LEAK - Datamoshing inspired motion vector leaking
// Simulates P-frame prediction errors where motion vectors "leak" between blocks
void effectPredictionLeak(std::vector<Color>& pixels, int w, int h, int blockSize, float leakAmount, uint32_t seed) {
    if (blockSize < 2) blockSize = 16;

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> mvDist(-blockSize, blockSize);
    std::uniform_real_distribution<float> probDist(0.0f, 1.0f);

    std::vector<Color> result = pixels;

    // Store motion vectors for each block
    int blocksX = (w + blockSize - 1) / blockSize;
    int blocksY = (h + blockSize - 1) / blockSize;

    std::vector<std::pair<int, int>> motionVectors(static_cast<size_t>(blocksX * blocksY));

    // Generate initial motion vectors
    for (auto& mv : motionVectors) {
        mv.first = mvDist(rng);
        mv.second = mvDist(rng);
    }

    // Apply motion vectors with leaking
    for (int by = 0; by < blocksY; by++) {
        for (int bx = 0; bx < blocksX; bx++) {
            int blockIdx = by * blocksX + bx;

            // Get motion vector (potentially leaked from neighbor)
            int mvX = motionVectors[static_cast<size_t>(blockIdx)].first;
            int mvY = motionVectors[static_cast<size_t>(blockIdx)].second;

            // Leak from previous block
            if (probDist(rng) < leakAmount) {
                int leakSource = -1;
                int leakDir = static_cast<int>(probDist(rng) * 4);

                switch (leakDir) {
                    case 0: // Left
                        if (bx > 0) leakSource = by * blocksX + (bx - 1);
                        break;
                    case 1: // Right
                        if (bx < blocksX - 1) leakSource = by * blocksX + (bx + 1);
                        break;
                    case 2: // Up
                        if (by > 0) leakSource = (by - 1) * blocksX + bx;
                        break;
                    case 3: // Down
                        if (by < blocksY - 1) leakSource = (by + 1) * blocksX + bx;
                        break;
                }

                if (leakSource >= 0) {
                    mvX = motionVectors[static_cast<size_t>(leakSource)].first;
                    mvY = motionVectors[static_cast<size_t>(leakSource)].second;
                }
            }

            // Apply motion vector to block
            for (int ly = 0; ly < blockSize; ly++) {
                for (int lx = 0; lx < blockSize; lx++) {
                    int destX = bx * blockSize + lx;
                    int destY = by * blockSize + ly;

                    if (destX >= w || destY >= h) continue;

                    int srcX = destX + mvX;
                    int srcY = destY + mvY;

                    // Clamp source coordinates
                    srcX = std::clamp(srcX, 0, w - 1);
                    srcY = std::clamp(srcY, 0, h - 1);

                    result[static_cast<size_t>(destY * w + destX)] =
                        pixels[static_cast<size_t>(srcY * w + srcX)];
                }
            }
        }
    }

    pixels = std::move(result);
}

} // namespace glic

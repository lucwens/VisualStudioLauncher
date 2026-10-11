#include "Core.h"
#include <filesystem>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <fstream>

namespace launcher
{
namespace
{
void Word(std::vector<unsigned char> &bytes, unsigned value)
{
    bytes.push_back(static_cast<unsigned char>(value));
    bytes.push_back(static_cast<unsigned char>(value >> 8));
}

void Dword(std::vector<unsigned char> &bytes, unsigned value)
{
    Word(bytes, value);
    Word(bytes, value >> 16);
}

double Segment(double x, double y, double ax, double ay, double bx, double by)
{
    const double dx = bx - ax;
    const double dy = by - ay;
    const double t = (std::max)(0.0, (std::min)(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)));
    return std::hypot(x - ax - t * dx, y - ay - t * dy);
}

std::vector<unsigned char> Image(unsigned size, unsigned rgb)
{
    std::vector<unsigned char> bytes;
    const unsigned maskStride = ((size + 31) / 32) * 4;
    Dword(bytes, 40);
    Dword(bytes, size);
    Dword(bytes, size * 2);
    Word(bytes, 1);
    Word(bytes, 32);
    Dword(bytes, 0);
    Dword(bytes, size * size * 4 + maskStride * size);
    Dword(bytes, 0);
    Dword(bytes, 0);
    Dword(bytes, 0);
    Dword(bytes, 0);
    for (int y = static_cast<int>(size) - 1; y >= 0; --y)
    {
        for (unsigned x = 0; x < size; ++x)
        {
            double alpha = 0;
            double white = 0;
            for (int sy = 0; sy < 4; ++sy)
            {
                for (int sx = 0; sx < 4; ++sx)
                {
                    const double px = (x + (sx + 0.5) / 4) / size;
                    const double py = (y + (sy + 0.5) / 4) / size;
                    const double qx = (std::max)(std::abs(px - 0.5) - 0.28, 0.0);
                    const double qy = (std::max)(std::abs(py - 0.5) - 0.28, 0.0);
                    if (std::hypot(qx, qy) > 0.16)
                    {
                        continue;
                    }
                    alpha += 1;
                    const auto distance =
                        (std::min)({Segment(px, py, 0.39, 0.31, 0.23, 0.5), Segment(px, py, 0.23, 0.5, 0.39, 0.69),
                                    Segment(px, py, 0.61, 0.31, 0.77, 0.5), Segment(px, py, 0.77, 0.5, 0.61, 0.69)});
                    if (distance < 0.033)
                    {
                        white += 1;
                    }
                }
            }
            const double mix = alpha > 0 ? white / alpha : 0;
            for (int shift : {0, 8, 16})
            {
                const auto color = (rgb >> shift) & 255;
                bytes.push_back(static_cast<unsigned char>(color * (1 - mix) + 255 * mix));
            }
            bytes.push_back(static_cast<unsigned char>(alpha / 16 * 255));
        }
    }
    // AND mask accompanies the 32-bit alpha bitmap; transparent pixels are masked.
    for (unsigned row = 0; row < size; ++row)
    {
        for (unsigned block = 0; block < maskStride; ++block)
        {
            unsigned char mask = 0;
            for (unsigned bit = 0; bit < 8; ++bit)
            {
                const unsigned x = block * 8 + bit;
                if (x < size && bytes[40 + (row * size + x) * 4 + 3] == 0)
                {
                    mask |= static_cast<unsigned char>(0x80 >> bit);
                }
            }
            bytes.push_back(mask);
        }
    }
    return bytes;
}
} // namespace

void WriteColorIcon(const std::filesystem::path &path, const std::string &color)
{
    if (!IsValidColor(color))
    {
        throw std::runtime_error("Invalid icon color");
    }
    const auto rgb = static_cast<unsigned>(std::stoul(color.substr(1), nullptr, 16));
    const unsigned sizes[] = {16, 24, 32, 48, 64};
    std::vector<std::vector<unsigned char>> images;
    for (const auto size : sizes)
    {
        images.push_back(Image(size, rgb));
    }
    std::vector<unsigned char> bytes;
    Word(bytes, 0);
    Word(bytes, 1);
    Word(bytes, 5);
    unsigned offset = 6 + 5 * 16;
    for (size_t i = 0; i < 5; ++i)
    {
        bytes.push_back(static_cast<unsigned char>(sizes[i]));
        bytes.push_back(static_cast<unsigned char>(sizes[i]));
        Word(bytes, 0);
        Word(bytes, 1);
        Word(bytes, 32);
        Dword(bytes, static_cast<unsigned>(images[i].size()));
        Dword(bytes, offset);
        offset += static_cast<unsigned>(images[i].size());
    }
    for (const auto &image : images)
    {
        bytes.insert(bytes.end(), image.begin(), image.end());
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.close();
    if (!output)
    {
        throw std::runtime_error("Cannot write colored icon");
    }
}
} // namespace launcher

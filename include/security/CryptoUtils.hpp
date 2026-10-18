#pragma once

#include <string>
#include <vector>

namespace voting::security {

class CryptoUtils {
public:
    static std::string toHex(const unsigned char* data, size_t length);
    static std::vector<unsigned char> fromHex(const std::string& hex);
    static std::string randomHex(size_t byteCount);
    static std::string sha256Hex(const std::string& input);
    static std::string hmacSha256Hex(const std::string& key, const std::string& data);
    static bool constantTimeEquals(const std::string& a, const std::string& b);
};

} // namespace voting::security

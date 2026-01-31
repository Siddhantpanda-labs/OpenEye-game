/**
 * @file security.hpp
 * @brief Crypto pipeline (AES-GCM) and per-channel ACL/auth
 */

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <optional>

namespace tienet {
namespace core {

enum class CryptoAlgorithm {
    None,
    AES_GCM_128,
    AES_GCM_256
};

struct CryptoConfig {
    bool enableEncryption;
    CryptoAlgorithm algorithm;
    std::vector<uint8_t> key;
    std::vector<uint8_t> salt;
    
    CryptoConfig() : enableEncryption(false), algorithm(CryptoAlgorithm::None) {}
};

class ICipher {
public:
    virtual ~ICipher() = default;
    virtual bool initialize(const CryptoConfig& config) = 0;
    virtual bool encrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out, 
                         const uint8_t* aad, size_t aadLen, uint64_t nonce) = 0;
    virtual bool decrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out,
                         const uint8_t* aad, size_t aadLen, uint64_t nonce) = 0;
};

class NullCipher : public ICipher {
public:
    bool initialize(const CryptoConfig& config) override { (void)config; return true; }
    bool encrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out, 
                 const uint8_t* aad, size_t aadLen, uint64_t nonce) override {
        (void)aad; (void)aadLen; (void)nonce; out.assign(in, in + inLen); return true; }
    bool decrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out,
                 const uint8_t* aad, size_t aadLen, uint64_t nonce) override {
        (void)aad; (void)aadLen; (void)nonce; out.assign(in, in + inLen); return true; }
};

class AesGcmCipher : public ICipher {
public:
    AesGcmCipher();
    ~AesGcmCipher();
    bool initialize(const CryptoConfig& config) override;
    bool encrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out,
                 const uint8_t* aad, size_t aadLen, uint64_t nonce) override;
    bool decrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out,
                 const uint8_t* aad, size_t aadLen, uint64_t nonce) override;
private:
    CryptoAlgorithm m_alg;
    std::vector<uint8_t> m_key;
    std::vector<uint8_t> m_salt;
};

class CryptoFactory {
public:
    static std::unique_ptr<ICipher> create(const CryptoConfig& cfg);
};

// Authentication and ACL

enum class Permission : uint32_t {
    None = 0,
    Send = 1 << 0,
    Receive = 1 << 1,
    Admin = 1 << 2
};

inline Permission operator|(Permission a, Permission b) {
    return static_cast<Permission>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool hasPermission(Permission mask, Permission p) {
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(p)) != 0;
}

struct AccessControlEntry {
    uint8_t channelId;
    std::string principal; // user/device id
    Permission permissions;
};

class ACLManager {
public:
    void grant(uint8_t channelId, const std::string& principal, Permission perms);
    void revoke(uint8_t channelId, const std::string& principal, Permission perms);
    bool check(uint8_t channelId, const std::string& principal, Permission required) const;
private:
    // key: channelId|principal
    std::unordered_map<std::string, Permission> m_acl;
    static std::string makeKey(uint8_t channelId, const std::string& principal);
};

class AuthTokenManager {
public:
    void setTokenForPrincipal(const std::string& principal, const std::string& token);
    std::optional<std::string> getToken(const std::string& principal) const;
    bool validate(const std::string& principal, const std::string& token) const;
private:
    std::unordered_map<std::string, std::string> m_tokens;
};

} // namespace core
} // namespace tienet



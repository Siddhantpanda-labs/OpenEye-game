/**
 * @file security.cpp
 */

#include "tienet/core/security.hpp"
#include <cstring>

#ifdef TIENET_HAS_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace tienet {
namespace core {

AesGcmCipher::AesGcmCipher() : m_alg(CryptoAlgorithm::None) {}
AesGcmCipher::~AesGcmCipher() {}

bool AesGcmCipher::initialize(const CryptoConfig& config) {
    m_alg = config.algorithm;
    m_key = config.key;
    m_salt = config.salt;
    if (m_alg == CryptoAlgorithm::AES_GCM_128) {
        if (m_key.size() != 16) return false;
    } else if (m_alg == CryptoAlgorithm::AES_GCM_256) {
        if (m_key.size() != 32) return false;
    }
    return true;
}

bool AesGcmCipher::encrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out,
                           const uint8_t* aad, size_t aadLen, uint64_t nonce) {
#ifndef TIENET_HAS_OPENSSL
    // Fallback XOR (development only). Not secure.
    out.resize(inLen);
    for (size_t i = 0; i < inLen; ++i) out[i] = in[i] ^ 0xAA;
    (void)aad; (void)aadLen; (void)nonce; return true;
#else
    const size_t tagLen = 16;
    std::vector<uint8_t> iv(12);
    std::memcpy(iv.data(), &nonce, std::min<size_t>(8, iv.size()));
    if (!m_salt.empty()) {
        size_t c = std::min<size_t>(iv.size() - 8, m_salt.size());
        std::memcpy(iv.data() + 8, m_salt.data(), c);
    }

    const EVP_CIPHER* cipher = (m_alg == CryptoAlgorithm::AES_GCM_256) ? EVP_aes_256_gcm() : EVP_aes_128_gcm();
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool ok = true;
    int rc = EVP_EncryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr);
    ok &= (rc == 1);
    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr);
    ok &= (rc == 1);
    rc = EVP_EncryptInit_ex(ctx, nullptr, nullptr, m_key.data(), iv.data());
    ok &= (rc == 1);

    int outLen = 0;
    if (aad && aadLen > 0) {
        rc = EVP_EncryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLen));
        ok &= (rc == 1);
    }

    out.resize(inLen + tagLen);
    int len = 0;
    rc = EVP_EncryptUpdate(ctx, out.data(), &len, in, static_cast<int>(inLen));
    ok &= (rc == 1);
    int totalLen = len;
    rc = EVP_EncryptFinal_ex(ctx, out.data() + totalLen, &len);
    ok &= (rc == 1);
    totalLen += len;

    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tagLen), out.data() + totalLen);
    ok &= (rc == 1);
    totalLen += static_cast<int>(tagLen);
    out.resize(totalLen);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
#endif
}

bool AesGcmCipher::decrypt(const uint8_t* in, size_t inLen, std::vector<uint8_t>& out,
                           const uint8_t* aad, size_t aadLen, uint64_t nonce) {
#ifndef TIENET_HAS_OPENSSL
    out.resize(inLen);
    for (size_t i = 0; i < inLen; ++i) out[i] = in[i] ^ 0xAA;
    (void)aad; (void)aadLen; (void)nonce; return true;
#else
    if (inLen < 16) return false;
    const size_t tagLen = 16;
    size_t ctLen = inLen - tagLen;

    std::vector<uint8_t> iv(12);
    std::memcpy(iv.data(), &nonce, std::min<size_t>(8, iv.size()));
    if (!m_salt.empty()) {
        size_t c = std::min<size_t>(iv.size() - 8, m_salt.size());
        std::memcpy(iv.data() + 8, m_salt.data(), c);
    }

    const EVP_CIPHER* cipher = (m_alg == CryptoAlgorithm::AES_GCM_256) ? EVP_aes_256_gcm() : EVP_aes_128_gcm();
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    bool ok = true;
    int rc = EVP_DecryptInit_ex(ctx, cipher, nullptr, nullptr, nullptr);
    ok &= (rc == 1);
    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr);
    ok &= (rc == 1);
    rc = EVP_DecryptInit_ex(ctx, nullptr, nullptr, m_key.data(), iv.data());
    ok &= (rc == 1);

    int outLen = 0;
    if (aad && aadLen > 0) {
        rc = EVP_DecryptUpdate(ctx, nullptr, &outLen, aad, static_cast<int>(aadLen));
        ok &= (rc == 1);
    }

    out.resize(ctLen);
    int len = 0;
    rc = EVP_DecryptUpdate(ctx, out.data(), &len, in, static_cast<int>(ctLen));
    ok &= (rc == 1);
    int totalLen = len;

    rc = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tagLen), (void*)(in + ctLen));
    ok &= (rc == 1);
    rc = EVP_DecryptFinal_ex(ctx, out.data() + totalLen, &len);
    ok &= (rc == 1);
    totalLen += len;
    out.resize(totalLen);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
#endif
}

std::unique_ptr<ICipher> CryptoFactory::create(const CryptoConfig& cfg) {
    if (!cfg.enableEncryption || cfg.algorithm == CryptoAlgorithm::None) {
        return std::make_unique<NullCipher>();
    }
    return std::make_unique<AesGcmCipher>();
}

void ACLManager::grant(uint8_t channelId, const std::string& principal, Permission perms) {
    m_acl[makeKey(channelId, principal)] = static_cast<Permission>(static_cast<uint32_t>(perms));
}

void ACLManager::revoke(uint8_t channelId, const std::string& principal, Permission perms) {
    auto key = makeKey(channelId, principal);
    auto it = m_acl.find(key);
    if (it == m_acl.end()) return;
    auto current = static_cast<uint32_t>(it->second);
    auto remove = static_cast<uint32_t>(perms);
    current &= ~remove;
    it->second = static_cast<Permission>(current);
}

bool ACLManager::check(uint8_t channelId, const std::string& principal, Permission required) const {
    auto it = m_acl.find(makeKey(channelId, principal));
    if (it == m_acl.end()) return false;
    return hasPermission(it->second, required);
}

std::string ACLManager::makeKey(uint8_t channelId, const std::string& principal) {
    return std::to_string(channelId) + ":" + principal;
}

void AuthTokenManager::setTokenForPrincipal(const std::string& principal, const std::string& token) {
    m_tokens[principal] = token;
}

std::optional<std::string> AuthTokenManager::getToken(const std::string& principal) const {
    auto it = m_tokens.find(principal);
    if (it == m_tokens.end()) return std::nullopt;
    return it->second;
}

bool AuthTokenManager::validate(const std::string& principal, const std::string& token) const {
    auto it = m_tokens.find(principal);
    if (it == m_tokens.end()) return false;
    return it->second == token;
}

} // namespace core
} // namespace tienet



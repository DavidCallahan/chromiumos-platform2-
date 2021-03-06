// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/cryptolib.h"

#include <limits>
#include <vector>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <unistd.h>

#include <base/files/file_util.h>
#include <base/logging.h>
#include <base/stl_util.h>
#include <brillo/secure_blob.h>
#include <crypto/scoped_openssl_types.h>
extern "C" {
#include <scrypt/crypto_scrypt.h>
#include <scrypt/scryptenc.h>
}

#include "cryptohome/platform.h"

using brillo::SecureBlob;

namespace cryptohome {

// The well-known exponent used when generating RSA keys.  Cryptohome only
// generates one RSA key, which is the system-wide cryptohome key.  This is the
// common public exponent.
const unsigned int kWellKnownExponent = 65537;

// The current number of hash rounds we use.  Large enough to be a measurable
// amount of time, but not add too much overhead to login (around 10ms).
const unsigned int kDefaultPasswordRounds = 1337;

// AES block size in bytes.
const unsigned int  kAesBlockSize = 16;

void CryptoLib::GetSecureRandom(unsigned char* buf, size_t length) {
  // OpenSSL takes a signed integer.  On the off chance that the user requests
  // something too large, truncate it.
  if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
    length = std::numeric_limits<int>::max();
  }
  RAND_bytes(buf, length);
}

bool CryptoLib::CreateRsaKey(size_t key_bits,
                             SecureBlob* n,
                          SecureBlob* p) {
  crypto::ScopedRSA rsa(RSA_generate_key(key_bits,
                                         kWellKnownExponent,
                                         NULL,
                                         NULL));
  if (rsa.get() == NULL) {
    LOG(ERROR) << "RSA key generation failed.";
    return false;
  }

  SecureBlob local_n(BN_num_bytes(rsa.get()->n));
  if (BN_bn2bin(rsa.get()->n, local_n.data()) <= 0) {
    LOG(ERROR) << "Unable to get modulus from RSA key.";
    return false;
  }

  SecureBlob local_p(BN_num_bytes(rsa.get()->p));
  if (BN_bn2bin(rsa.get()->p, local_p.data()) <= 0) {
    LOG(ERROR) << "Unable to get private key from RSA key.";
    return false;
  }

  n->swap(local_n);
  p->swap(local_p);
  return true;
}

SecureBlob CryptoLib::Sha1(const brillo::Blob& data) {
  SHA_CTX sha_context;
  unsigned char md_value[SHA_DIGEST_LENGTH];
  SecureBlob hash;

  SHA1_Init(&sha_context);
  SHA1_Update(&sha_context, data.data(), data.size());
  SHA1_Final(md_value, &sha_context);
  hash.resize(sizeof(md_value));
  memcpy(hash.data(), md_value, sizeof(md_value));
  // Zero the stack to match expectations set by SecureBlob.
  brillo::SecureMemset(md_value, 0, sizeof(md_value));
  return hash;
}

SecureBlob CryptoLib::Sha256(const brillo::Blob& data) {
  SHA256_CTX sha_context;
  unsigned char md_value[SHA256_DIGEST_LENGTH];
  SecureBlob hash;

  SHA256_Init(&sha_context);
  SHA256_Update(&sha_context, data.data(), data.size());
  SHA256_Final(md_value, &sha_context);
  hash.resize(sizeof(md_value));
  memcpy(hash.data(), md_value, sizeof(md_value));
  // Zero the stack to match expectations set by SecureBlob.
  brillo::SecureMemset(md_value, 0, sizeof(md_value));
  return hash;
}

brillo::SecureBlob CryptoLib::HmacSha512(const brillo::SecureBlob& key,
                                           const brillo::Blob& data) {
  const int kSha512OutputSize = 64;
  unsigned char mac[kSha512OutputSize];
  HMAC(EVP_sha512(),
       key.data(), key.size(),
       data.data(), data.size(),
       mac, NULL);
  return brillo::SecureBlob(std::begin(mac), std::end(mac));
}

brillo::SecureBlob CryptoLib::HmacSha256(const brillo::SecureBlob& key,
                                           const brillo::Blob& data) {
  const int kSha256OutputSize = 32;
  unsigned char mac[kSha256OutputSize];
  HMAC(EVP_sha256(),
       key.data(), key.size(),
       data.data(), data.size(),
       mac, NULL);
  return brillo::SecureBlob(std::begin(mac), std::end(mac));
}

size_t CryptoLib::GetAesBlockSize() {
  return EVP_CIPHER_block_size(EVP_aes_256_cbc());
}

bool CryptoLib::PasskeyToAesKey(const brillo::Blob& passkey,
                                const brillo::Blob& salt, unsigned int rounds,
                                SecureBlob* key, SecureBlob* iv) {
  if (salt.size() != PKCS5_SALT_LEN) {
    LOG(ERROR) << "Bad salt size.";
    return false;
  }

  const EVP_CIPHER* cipher = EVP_aes_256_cbc();
  SecureBlob aes_key(EVP_CIPHER_key_length(cipher));
  SecureBlob local_iv(EVP_CIPHER_iv_length(cipher));

  // Convert the passkey to a key
  if (!EVP_BytesToKey(cipher,
                      EVP_sha1(),
                      salt.data(),
                      passkey.data(),
                      passkey.size(),
                      rounds,
                      aes_key.data(),
                      local_iv.data())) {
    LOG(ERROR) << "Failure converting bytes to key";
    return false;
  }

  key->swap(aes_key);
  if (iv) {
    iv->swap(local_iv);
  }

  return true;
}

bool CryptoLib::AesEncrypt(const brillo::Blob& plaintext,
                           const SecureBlob& key,
                           const SecureBlob& iv,
                           SecureBlob* ciphertext) {
  return AesEncryptSpecifyBlockMode(plaintext, 0, plaintext.size(), key, iv,
                                    kPaddingCryptohomeDefault, kCbc,
                                    ciphertext);
}

bool CryptoLib::AesDecrypt(const brillo::Blob& ciphertext,
                           const SecureBlob& key,
                           const SecureBlob& iv,
                           SecureBlob* plaintext) {
  return AesDecryptSpecifyBlockMode(ciphertext, 0, ciphertext.size(), key, iv,
                                    kPaddingCryptohomeDefault, kCbc, plaintext);
}

// This is the reverse operation of AesEncryptSpecifyBlockMode above.  See that
// method for a description of how padding and block_mode affect the crypto
// operations.  This method automatically removes and verifies the padding, so
// plain_text (on success) will contain the original data.
//
// Note that a call to AesDecryptSpecifyBlockMode needs to have the same padding
// and block_mode as the corresponding encrypt call.  Changing the block mode
// will drastically alter the decryption.  And an incorrect PaddingScheme will
// result in the padding verification failing, for which the method call fails,
// even if the key and initialization vector were correct.
bool CryptoLib::AesDecryptSpecifyBlockMode(const brillo::Blob& encrypted,
                                           unsigned int start,
                                           unsigned int count,
                                           const SecureBlob& key,
                                           const SecureBlob& iv,
                                           PaddingScheme padding,
                                           BlockMode block_mode,
                                           SecureBlob* plain_text) {
  if ((start > encrypted.size()) ||
      ((start + count) > encrypted.size()) ||
      ((start + count) < start)) {
    return false;
  }
  SecureBlob local_plain_text(count);

  if (local_plain_text.size() >
      static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    // EVP_DecryptUpdate takes a signed int
    return false;
  }
  int final_size = 0;
  int decrypt_size = local_plain_text.size();

  const EVP_CIPHER* cipher;
  switch (block_mode) {
    case kCbc:
      cipher = EVP_aes_256_cbc();
      break;
    case kEcb:
      cipher = EVP_aes_256_ecb();
      break;
    default:
      LOG(ERROR) << "Invalid block mode specified: " << block_mode;
      return false;
  }
  if (key.size() != static_cast<unsigned int>(EVP_CIPHER_key_length(cipher))) {
    LOG(ERROR) << "Invalid key length of " << key.size()
               << ", expected " << EVP_CIPHER_key_length(cipher);
    return false;
  }
  // ECB ignores the IV, so only check the IV length if we are using a different
  // block mode.
  if ((block_mode != kEcb) &&
      (iv.size() != static_cast<unsigned int>(EVP_CIPHER_iv_length(cipher)))) {
    LOG(ERROR) << "Invalid iv length of " << iv.size()
               << ", expected " << EVP_CIPHER_iv_length(cipher);
    return false;
  }

  EVP_CIPHER_CTX decryption_context;
  EVP_CIPHER_CTX_init(&decryption_context);
  EVP_DecryptInit_ex(&decryption_context, cipher, NULL, key.data(), iv.data());
  if (padding == kPaddingNone) {
    EVP_CIPHER_CTX_set_padding(&decryption_context, 0);
  }

  // Make sure we're not pointing into an empty buffer or past the end.
  const unsigned char *encrypted_buf = NULL;
  if (start < encrypted.size())
    encrypted_buf = &encrypted[start];

  if (!EVP_DecryptUpdate(&decryption_context, local_plain_text.data(),
                         &decrypt_size, encrypted_buf, count)) {
    LOG(ERROR) << "DecryptUpdate failed";
    EVP_CIPHER_CTX_cleanup(&decryption_context);
    return false;
  }

  // In the case of local_plain_text being full, we must avoid trying to
  // point past the end of the buffer when calling EVP_DecryptFinal_ex().
  unsigned char *final_buf = NULL;
  if (static_cast<unsigned int>(decrypt_size) < local_plain_text.size())
    final_buf = &local_plain_text[decrypt_size];

  if (!EVP_DecryptFinal_ex(&decryption_context, final_buf, &final_size)) {
    unsigned long err = ERR_get_error(); // NOLINT openssl types
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();

    LOG(ERROR) << "DecryptFinal Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);

    EVP_CIPHER_CTX_cleanup(&decryption_context);
    return false;
  }
  final_size += decrypt_size;

  if (padding == kPaddingCryptohomeDefault) {
    if (final_size < SHA_DIGEST_LENGTH) {
      LOG(ERROR) << "Plain text was too small.";
      EVP_CIPHER_CTX_cleanup(&decryption_context);
      return false;
    }

    final_size -= SHA_DIGEST_LENGTH;

    SHA_CTX sha_context;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_context);
    SHA1_Update(&sha_context, local_plain_text.data(), final_size);
    SHA1_Final(md_value, &sha_context);

    const unsigned char* md_ptr = local_plain_text.data();
    md_ptr += final_size;
    if (brillo::SecureMemcmp(md_ptr, md_value, SHA_DIGEST_LENGTH)) {
      LOG(ERROR) << "Digest verification failed.";
      EVP_CIPHER_CTX_cleanup(&decryption_context);
      return false;
    }
  }

  local_plain_text.resize(final_size);
  plain_text->swap(local_plain_text);
  EVP_CIPHER_CTX_cleanup(&decryption_context);
  return true;
}

// AesEncryptSpecifyBlockMode encrypts the bytes in plain_text using AES,
// placing the output into encrypted.  Aside from range constraints (start and
// count) and the key and initialization vector, this method has two parameters
// that control how the ciphertext is generated and are useful in encrypting
// specific types of data in cryptohome.
//
// First, padding specifies whether and how the plaintext is padded before
// encryption.  The three options, described in the PaddingScheme enumeration
// are used as such:
//   - kPaddingNone is used to mix the user's passkey (derived from the
//     password) into the encrypted blob storing the vault keyset when the TPM
//     is used.  This is described in more detail in the README file.  There is
//     no padding in this case, and the size of plain_text needs to be a
//     multiple of the AES block size (16 bytes).
//   - kPaddingStandard uses standard PKCS padding, which is the default for
//     OpenSSL.
//   - kPaddingCryptohomeDefault appends a SHA1 hash of the plaintext in
//     plain_text before passing it to OpenSSL, which still uses PKCS padding
//     so that we do not have to re-implement block-multiple padding ourselves.
//     This padding scheme allows us to strongly verify the plaintext on
//     decryption, which is essential when, for example, test decrypting a nonce
//     to test whether a password was correct (we do this in user_session.cc).
//
// The block mode switches between ECB and CBC.  Generally, CBC is used for most
// AES crypto that we perform, since it is a better mode for us for data that is
// larger than the block size.  We use ECB only when mixing the user passkey
// into the TPM-encrypted blob, since we only encrypt a single block of that
// data.
bool CryptoLib::AesEncryptSpecifyBlockMode(const brillo::Blob& plain_text,
                                           unsigned int start,
                                           unsigned int count,
                                           const SecureBlob& key,
                                           const SecureBlob& iv,
                                           PaddingScheme padding,
                                           BlockMode block_mode,
                                           SecureBlob* encrypted) {
  // Verify that the range is within the data passed
  if ((start > plain_text.size()) ||
      ((start + count) > plain_text.size()) ||
      ((start + count) < start)) {
    return false;
  }
  if (count > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
    // EVP_EncryptUpdate takes a signed int
    return false;
  }

  // First set the output size based on the padding scheme.  No padding means
  // that the input needs to be a multiple of the block size, and the output
  // size is equal to the input size.  Standard padding means we should allocate
  // up to a full block additional for the PKCS padding.  Cryptohome default
  // means we should allocate a full block additional for the PKCS padding and
  // enough for a SHA1 hash.
  unsigned int block_size = GetAesBlockSize();
  unsigned int needed_size = count;
  switch (padding) {
    case kPaddingCryptohomeDefault:
      // The AES block size and SHA digest length are not enough for this to
      // overflow, as needed_size is initialized to count, which must be <=
      // INT_MAX, but needed_size is itself an unsigned.  The block size and
      // digest length are fixed by the algorithm.
      needed_size += block_size + SHA_DIGEST_LENGTH;
      break;
    case kPaddingStandard:
      needed_size += block_size;
      break;
    case kPaddingNone:
      if (count % block_size) {
        LOG(ERROR) << "Data size (" << count << ") was not a multiple "
                   << "of the block size (" << block_size << ")";
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Invalid padding specified";
      return false;
      break;
  }
  SecureBlob cipher_text(needed_size);

  // Set the block mode
  const EVP_CIPHER* cipher;
  switch (block_mode) {
    case kCbc:
      cipher = EVP_aes_256_cbc();
      break;
    case kEcb:
      cipher = EVP_aes_256_ecb();
      break;
    default:
      LOG(ERROR) << "Invalid block mode specified";
      return false;
  }
  if (key.size() != static_cast<unsigned int>(EVP_CIPHER_key_length(cipher))) {
    LOG(ERROR) << "Invalid key length of " << key.size()
               << ", expected " << EVP_CIPHER_key_length(cipher);
    return false;
  }

  // ECB ignores the IV, so only check the IV length if we are using a different
  // block mode.
  if ((block_mode != kEcb) &&
      (iv.size() != static_cast<unsigned int>(EVP_CIPHER_iv_length(cipher)))) {
    LOG(ERROR) << "Invalid iv length of " << iv.size()
               << ", expected " << EVP_CIPHER_iv_length(cipher);
    return false;
  }

  // Initialize the OpenSSL crypto context
  EVP_CIPHER_CTX encryption_context;
  EVP_CIPHER_CTX_init(&encryption_context);
  EVP_EncryptInit_ex(&encryption_context, cipher, NULL, key.data(), iv.data());
  if (padding == kPaddingNone) {
    EVP_CIPHER_CTX_set_padding(&encryption_context, 0);
  }

  // First, encrypt the plain_text data
  unsigned int current_size = 0;
  int encrypt_size = 0;

  // Make sure we're not pointing into an empty buffer or past the end.
  const unsigned char *plain_buf = NULL;
  if (start < plain_text.size())
    plain_buf = &plain_text[start];

  if (!EVP_EncryptUpdate(&encryption_context, &cipher_text[current_size],
                         &encrypt_size, plain_buf, count)) {
    LOG(ERROR) << "EncryptUpdate failed";
    EVP_CIPHER_CTX_cleanup(&encryption_context);
    return false;
  }
  current_size += encrypt_size;
  encrypt_size = 0;

  // Next, if the padding uses the cryptohome default scheme, encrypt a SHA1
  // hash of the preceding plain_text into the output data
  if (padding == kPaddingCryptohomeDefault) {
    SHA_CTX sha_context;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&sha_context);
    SHA1_Update(&sha_context, &plain_text[start], count);
    SHA1_Final(md_value, &sha_context);
    if (!EVP_EncryptUpdate(&encryption_context, &cipher_text[current_size],
                           &encrypt_size, md_value, sizeof(md_value))) {
      LOG(ERROR) << "EncryptUpdate failed";
      EVP_CIPHER_CTX_cleanup(&encryption_context);
      return false;
    }
    current_size += encrypt_size;
    encrypt_size = 0;
  }

  // In the case of cipher_text being full, we must avoid trying to
  // point past the end of the buffer when calling EVP_EncryptFinal_ex().
  unsigned char *final_buf = NULL;
  if (static_cast<unsigned int>(current_size) < cipher_text.size())
    final_buf = &cipher_text[current_size];

  // Finally, finish the encryption
  if (!EVP_EncryptFinal_ex(&encryption_context, final_buf, &encrypt_size)) {
    LOG(ERROR) << "EncryptFinal failed";
    EVP_CIPHER_CTX_cleanup(&encryption_context);
    return false;
  }
  current_size += encrypt_size;
  cipher_text.resize(current_size);

  encrypted->swap(cipher_text);
  EVP_CIPHER_CTX_cleanup(&encryption_context);
  return true;
}

// Obscure (and Unobscure) RSA messages.
// Let k be a key derived from the user passphrase. On disk, we store
// m = ObscureRSAMessage(RSA-on-TPM(random-data), k). The reason for this
// function is the existence of an ambiguity in the TPM spec: the format of data
// returned by Tspi_Data_Bind is unspecified, so it's _possible_ (although does
// not happen in practice) that RSA-on-TPM(random-data) could start with some
// kind of ASN.1 header or whatever (some known data). If this was true, and we
// encrypted all of RSA-on-TPM(random-data), then one could test values of k by
// decrypting RSA-on-TPM(random-data) and looking for the known header, which
// would allow brute-forcing the user passphrase without talking to the TPM.
//
// Therefore, we instead encrypt _one block_ of RSA-on-TPM(random-data) with AES
// in ECB mode; we pick the last AES block, in the hope that that block will be
// part of the RSA message. TODO(ellyjones): why? if the TPM could add a header,
// it could also add a footer, and we'd be just as sunk.
//
// If we do encrypt part of the RSA message, the entirety of
// RSA-on-TPM(random-data) should be impossible to decrypt, without encrypting
// any known plaintext. This approach also requires brute-force attempts on k to
// go through the TPM, since there's no way to test a potential decryption
// without doing UnRSA-on-TPM() to see if the message is valid now.
bool CryptoLib::ObscureRSAMessage(const SecureBlob& plaintext,
                                  const SecureBlob& key,
                                  SecureBlob* ciphertext) {
  unsigned int aes_block_size = GetAesBlockSize();
  if (plaintext.size() < aes_block_size * 2) {
    LOG(ERROR) << "Plaintext is too small.";
    return false;
  }
  unsigned int offset = plaintext.size() - aes_block_size;

  SecureBlob obscured_chunk;
  if (!AesEncryptSpecifyBlockMode(plaintext, offset, aes_block_size, key,
                                  SecureBlob(0), kPaddingNone, kEcb,
                                  &obscured_chunk)) {
    LOG(ERROR) << "AES encryption failed.";
    return false;
  }
  ciphertext->resize(plaintext.size());
  char *data = reinterpret_cast<char*>(ciphertext->data());
  memcpy(data, plaintext.data(), plaintext.size());
  memcpy(data + offset, obscured_chunk.data(), obscured_chunk.size());
  return true;
}

bool CryptoLib::UnobscureRSAMessage(const SecureBlob& ciphertext,
                                    const SecureBlob& key,
                                    SecureBlob* plaintext) {
  unsigned int aes_block_size = GetAesBlockSize();
  if (ciphertext.size() < aes_block_size * 2) {
    LOG(ERROR) << "Ciphertext is is too small.";
    return false;
  }
  unsigned int offset = ciphertext.size() - aes_block_size;

  SecureBlob unobscured_chunk;
  if (!AesDecryptSpecifyBlockMode(ciphertext, offset, aes_block_size, key,
                                  SecureBlob(0), kPaddingNone, kEcb,
                                  &unobscured_chunk)) {
    LOG(ERROR) << "AES decryption failed.";
    return false;
  }
  plaintext->resize(ciphertext.size());
  char *data = reinterpret_cast<char*>(plaintext->data());
  memcpy(data, ciphertext.data(), ciphertext.size());
  memcpy(data + offset, unobscured_chunk.data(),
         unobscured_chunk.size());
  return true;
}

std::string CryptoLib::BlobToHex(const brillo::Blob& blob) {
  std::string buffer(blob.size() * 2, '\x00');
  BlobToHexToBuffer(blob, &buffer[0], buffer.size());
  return buffer;
}

void CryptoLib::BlobToHexToBuffer(const brillo::Blob& blob,
                                  void* buffer,
                                  size_t buffer_length) {
  static const char table[] = "0123456789abcdef";
  char* char_buffer = reinterpret_cast<char*>(buffer);
  char* char_buffer_end = char_buffer + buffer_length;
  for (uint8_t byte : blob) {
    if (char_buffer == char_buffer_end)
      break;
    *char_buffer++ = table[(byte >> 4) & 0x0f];
    if (char_buffer == char_buffer_end)
      break;
    *char_buffer++ = table[byte & 0x0f];
  }
  if (char_buffer != char_buffer_end)
    *char_buffer = '\x00';
}

std::string CryptoLib::ComputeEncryptedDataHMAC(
    const EncryptedData& encrypted_data, const SecureBlob& hmac_key) {
  SecureBlob blob1(encrypted_data.iv().begin(), encrypted_data.iv().end());
  SecureBlob blob2(encrypted_data.encrypted_data().begin(),
                   encrypted_data.encrypted_data().end());
  SecureBlob result = SecureBlob::Combine(blob1, blob2);
  SecureBlob hmac = HmacSha512(hmac_key, result);
  return hmac.to_string();
}

}  // namespace cryptohome

#pragma once
// C++ port of Security.Encryption (Encryption.cs) and the C# getUserandPassword().
//
// The C# class uses .NET Aes defaults: AES-128 (the key "wsproundtheworld" is 16 bytes),
// CBC mode, all-zero IV, PKCS7 padding, Base64 text. This is a self-contained AES so it
// builds with MSVC or MinGW without OpenSSL, and reads/writes the same ~/DBcredential file.
#include <QByteArray>
#include <QString>
#include <optional>

namespace Security {

// Same key the C# app uses for ~/DBcredential
inline const QString kCredentialKey = QStringLiteral("wsproundtheworld");

// Encryption.Encrypt / Encryption.Decrypt. Key must be 16 UTF-8 bytes (AES-128).
std::optional<QString> encrypt(const QString &plainText, const QString &key = kCredentialKey);
std::optional<QString> decrypt(const QString &base64Cipher, const QString &key = kCredentialKey);

// Raw AES-128-CBC with PKCS7 (exposed for tests)
QByteArray aes128CbcEncrypt(const QByteArray &plain, const QByteArray &key16, const QByteArray &iv16);
std::optional<QByteArray> aes128CbcDecrypt(const QByteArray &cipher, const QByteArray &key16, const QByteArray &iv16);

// C#: getUserandPassword - reads "db_pass: <base64>" from the first line of ~/DBcredential.
// Returns nullopt if the file is missing or unreadable. homeDir defaults to the user's home.
std::optional<QString> readDbPassword(const QString &homeDir = QString());

// Writes the file in the same format the C# app writes ("db_user: <user> db_pass: <cipher>")
// (and "db_call: <base call>" on a second line, as the C# app does, when dbCall is given)
bool writeDbCredential(const QString &user, const QString &password, const QString &homeDir = QString(),
                       const QString &dbCall = QString());

} // namespace Security

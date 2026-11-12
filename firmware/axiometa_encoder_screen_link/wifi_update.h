#pragma once
// Optional LAN updater. Disabled until USB provisioning; no secret in source or
// logs.
#include <esp_app_desc.h>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <mbedtls/md.h>
#include <nvs.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
namespace WifiUpdate {
static uint8_t key[32];
static bool provisioned = false;
static httpd_handle_t server = nullptr;
static char nonce[33];
static int64_t expires = 0;
static const char *build_id = "unknown";
static void hex(const uint8_t *data, size_t size, char *out) {
  const char *h = "0123456789abcdef";
  for (size_t i = 0; i < size; i++) {
    out[i * 2] = h[data[i] >> 4];
    out[i * 2 + 1] = h[data[i] & 15];
  }
  out[size * 2] = 0;
}
static bool unhex(const char *text, uint8_t *out, size_t size) {
  if (strlen(text) != size * 2)
    return false;
  for (size_t i = 0; i < size; i++) {
    unsigned v = 0;
    for (unsigned j = 0; j < 2; j++) {
      char c = text[i * 2 + j];
      unsigned x = c >= '0' && c <= '9'   ? c - '0'
                   : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                          : 16;
      if (x > 15)
        return false;
      v = v * 16 + x;
    }
    out[i] = v;
  }
  return true;
}
static bool equal(const uint8_t *a, const uint8_t *b, size_t n) {
  uint8_t diff = 0;
  for (size_t i = 0; i < n; i++)
    diff |= a[i] ^ b[i];
  return diff == 0;
}
static esp_err_t info(httpd_req_t *r) {
  uint8_t mac[6], ch = 0;
  wifi_second_chan_t second;
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  esp_wifi_get_channel(&ch, &second);
  char m[13], body[320];
  hex(mac, 6, m);
  snprintf(body, sizeof body,
           "{\"build\":\"%s\",\"mac_hex\":\"%s\",\"channel\":%u,"
           "\"partition\":\"%s\",\"project\":\"%s\",\"uptime_ms\":%llu,"
           "\"authenticated_update\":true}",
           build_id, m, ch, esp_ota_get_running_partition()->label,
           esp_app_get_description()->project_name,
           (unsigned long long)(esp_timer_get_time()/1000));
  httpd_resp_set_type(r, "application/json");
  return httpd_resp_sendstr(r, body);
}
static esp_err_t challenge(httpd_req_t *r) {
  uint8_t random[16];
  esp_fill_random(random, sizeof random);
  hex(random, sizeof random, nonce);
  expires = esp_timer_get_time() + 30000000;
  httpd_resp_set_hdr(r, "Cache-Control", "no-store");
  return httpd_resp_sendstr(r, nonce);
}
static esp_err_t update(httpd_req_t *r) {
  char n[33], sha[65], auth[65];
  uint8_t expected_hash[32], provided_mac[32], expected_mac[32];
  if (!provisioned || esp_timer_get_time() > expires || !expires ||
      httpd_req_get_hdr_value_str(r, "X-OD-Nonce", n, sizeof n) != ESP_OK ||
      httpd_req_get_hdr_value_str(r, "X-OD-SHA256", sha, sizeof sha) !=
          ESP_OK ||
      httpd_req_get_hdr_value_str(r, "X-OD-MAC", auth, sizeof auth) != ESP_OK ||
      strcmp(n, nonce) || !unhex(sha, expected_hash, 32) ||
      !unhex(auth, provided_mac, 32)) {
    httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Invalid authorization");
    return ESP_FAIL;
  }
  char message[98];
  snprintf(message, sizeof message, "%s %s", n, sha);
  const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (mbedtls_md_hmac(md, key, 32, (const uint8_t *)message, strlen(message),
                      expected_mac) ||
      !equal(expected_mac, provided_mac, 32)) {
    httpd_resp_send_err(r, HTTPD_403_FORBIDDEN, "Invalid authorization");
    return ESP_FAIL;
  }
  expires = 0; // One authenticated request consumes the challenge, including
               // failures.
  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part || r->content_len <= 0 || (size_t)r->content_len > part->size) {
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Image does not fit");
    return ESP_FAIL;
  }
  esp_ota_handle_t handle;
  esp_err_t err = esp_ota_begin(part, r->content_len, &handle);
  if (err != ESP_OK) {
    httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Cannot start update");
    return ESP_FAIL;
  }
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, md, 0) || mbedtls_md_starts(&ctx)) {
    mbedtls_md_free(&ctx);
    esp_ota_abort(handle);
    httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Hash initialization failed");
    return ESP_FAIL;
  }
  uint8_t buf[2048];
  int remaining = r->content_len;
  unsigned timeouts = 0;
  bool ok = true;
  while (remaining > 0) {
    int got = httpd_req_recv(
        r, (char *)buf, remaining < (int)sizeof buf ? remaining : sizeof buf);
    if (got == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 3)
      continue;
    if (got <= 0 || mbedtls_md_update(&ctx, buf, got) ||
        esp_ota_write(handle, buf, got) != ESP_OK) {
      ok = false;
      break;
    }
    remaining -= got;
  }
  uint8_t hash[32];
  if (mbedtls_md_finish(&ctx, hash))
    ok = false;
  mbedtls_md_free(&ctx);
  if (!ok || remaining || !equal(hash, expected_hash, 32)) {
    esp_ota_abort(handle);
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST,
                        "Incomplete or changed image");
    return ESP_FAIL;
  }
  err = esp_ota_end(handle);
  esp_app_desc_t next = {};
  if (err != ESP_OK ||
      esp_ota_get_partition_description(part, &next) != ESP_OK ||
      strcmp(next.project_name, esp_app_get_description()->project_name) ||
      esp_ota_set_boot_partition(part) != ESP_OK) {
    httpd_resp_send_err(r, HTTPD_400_BAD_REQUEST, "Image validation failed");
    return ESP_FAIL;
  }
  printf("OTA VERIFIED bytes=%d next=%s build=%s\n", r->content_len,
         part->label, build_id);
  httpd_resp_sendstr(r, "Verified; rebooting");
  vTaskDelay(pdMS_TO_TICKS(800));
  esp_restart();
  return ESP_OK;
}
static void got_ip(void *, esp_event_base_t, int32_t, void *data) {
  auto *event = (ip_event_got_ip_t *)data;
  printf("WIFI_UPDATE IP=" IPSTR " port=8080 build=%s\n",
         IP2STR(&event->ip_info.ip), build_id);
  if (server || !provisioned)
    return;
  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  cfg.server_port = 8080;
  cfg.stack_size = 8192;
  cfg.recv_wait_timeout = 5;
  if (httpd_start(&server, &cfg) != ESP_OK) {
    puts("WIFI_UPDATE server start failed");
    return;
  }
  httpd_uri_t i = {};
  i.uri = "/info";
  i.method = HTTP_GET;
  i.handler = info;
  httpd_register_uri_handler(server, &i);
  i.uri = "/challenge";
  i.handler = challenge;
  httpd_register_uri_handler(server, &i);
  i.uri = "/update";
  i.method = HTTP_POST;
  i.handler = update;
  httpd_register_uri_handler(server, &i);
}
static bool connect_saved() {
  nvs_handle_t h;
  if (nvs_open("od_update", NVS_READONLY, &h) != ESP_OK)
    return false;
  wifi_config_t cfg = {};
  char ssid[33] = {}, pass[64] = {};
  size_t s = sizeof ssid, p = sizeof pass, k = 32;
  esp_err_t a = nvs_get_str(h, "ssid", ssid, &s),
            b = nvs_get_str(h, "pass", pass, &p),
            c = nvs_get_blob(h, "key", key, &k);
  nvs_close(h);
  if (a || b || c || k != 32)
    return false;
  memcpy(cfg.sta.ssid, ssid, strlen(ssid));
  memcpy(cfg.sta.password, pass, strlen(pass));
  provisioned = true;
  cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
  cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK ||
      esp_wifi_connect() != ESP_OK) {
    puts("WIFI_UPDATE connection request failed");
    return false;
  }
  return true;
}
static void begin(const char *id) {
  build_id = id;
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip, nullptr);
  if (!connect_saved())
    puts("WIFI_UPDATE disabled/unconfigured; USB provision required");
}
static bool command(const char *line) {
  if (!strcmp(line, "UPDATE STATUS")) { printf("WIFI_UPDATE enabled=1 provisioned=%u build=%s\n", provisioned, build_id); return true; }
  if (strncmp(line, "WIFI ", 5))
    return false;
  char sh[65] = {}, ph[127] = {}, kh[65] = {}, extra;
  int fields = sscanf(line + 5, "%64s %126s %64s %c", sh, ph, kh, &extra);
  uint8_t ssid[33] = {}, pass[64] = {}, new_key[32];
  size_t sn = strlen(sh) / 2, pn = strlen(ph) / 2;
  if (fields != 3 || sn < 1 || sn > 32 || pn < 8 || pn > 63 ||
      !unhex(sh, ssid, sn) || !unhex(ph, pass, pn) || !unhex(kh, new_key, 32)) {
    puts("WIFI_UPDATE invalid provisioning");
    return true;
  }
  nvs_handle_t h;
  if (nvs_open("od_update", NVS_READWRITE, &h) != ESP_OK) {
    puts("WIFI_UPDATE storage unavailable");
    return true;
  }
  esp_err_t err = nvs_set_str(h, "ssid", (char *)ssid);
  if (!err)
    err = nvs_set_str(h, "pass", (char *)pass);
  if (!err)
    err = nvs_set_blob(h, "key", new_key, 32);
  if (!err)
    err = nvs_commit(h);
  nvs_close(h);
  if (err) {
    puts("WIFI_UPDATE storage failed");
    return true;
  }
  esp_wifi_disconnect();
  connect_saved();
  puts("WIFI_UPDATE configuration accepted; waiting for IP");
  return true;
}
} // namespace WifiUpdate

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "driver/i2c_master.h"
#include "Drivers.h"
#include "esp_err.h"
#include <string.h>

static const char *TAG = "Functions";

static uint8_t      ble_addr_type;
static uint16_t     sensor_chr_handle = 0;
static uint16_t     client_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool         notifications_enabled = false;

static uint8_t      latest_payload[BLE_PAYLOAD_MAX] = "0";
static uint16_t     latest_payload_len = 1;

static portMUX_TYPE ble_mux = portMUX_INITIALIZER_UNLOCKED;

static int ble_gap_event(struct ble_gap_event *event, void *arg);

//Oled Screen bitmaps
uint8_t font5x7[12][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x08,0x08,0x08,0x08,0x08}, // -
};
// OLED bitmap 128x64



float Temp_read(void)
{
    uint8_t Buffer[2];
    if (read_reg(Temp_dev, 0x05, Buffer, 2) != ESP_OK) return -999.0f;

    uint8_t upper = Buffer[0] & 0x1F;   // clear flag bits
    float temp;
    if (upper & 0x10) {            // sign bit set -> negative
        upper &= 0x0F;
        temp = 256.0f - (upper * 16.0f + Buffer[1] / 16.0f);
    } else {
        temp = upper * 16.0f + Buffer[1] / 16.0f;
    }
    return temp;
}

bool Heart_read_sample(uint32_t *red, uint32_t *ir)
{
    uint8_t wr, rd;
    read_reg(Heart_dev, 0x04, &wr, 1);
    read_reg(Heart_dev, 0x06, &rd, 1);
    if (((wr - rd) & 0x1F) == 0) return false;   // FIFO empty

    uint8_t b[6];
    if (read_reg(Heart_dev, 0x07, b, 6) != ESP_OK) return false;

    *red = (((uint32_t)b[0] << 16) | (b[1] << 8) | b[2]) & 0x3FFFF;
    *ir  = (((uint32_t)b[3] << 16) | (b[4] << 8) | b[5]) & 0x3FFFF;
    return true;
}

uint32_t HeartRateWindow[K];
int CountHeart = 0;
int IdxHeart = 0;
uint32_t Sum = 0;

uint32_t MovingAverage(uint32_t sample){
    if(CountHeart == K){
        Sum -= HeartRateWindow[IdxHeart];  
    }
    else{
        CountHeart++;
    }
    HeartRateWindow[IdxHeart] = sample;
    Sum += sample;
    if(IdxHeart == K -1){
        IdxHeart = 0;
    }
    else{
    IdxHeart++;
    }
    return (Sum/CountHeart);
}
float PreviousEma = 0;

float EMAFilter(float sample){
    float Ema;
    if(!PreviousEma){
        PreviousEma = sample;
    return sample;
    }
    else{
    Ema =(ALPHA * sample) + (1 - ALPHA) * PreviousEma;
    PreviousEma = Ema;
    return Ema;
    }
}
int FrameBuffer[1024];
void fb_set_pixel(int x, int y)       // turn ON
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    FrameBuffer[(y / 8) * OLED_W + x] |= (1 << (y % 8));
}

void fb_clear_pixel(int x, int y)     // turn OFF
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    FrameBuffer[(y / 8) * OLED_W + x] &= ~(1 << (y % 8));
}

void OLED_WriteBuffer(i2c_master_dev_handle_t dev, const uint8_t *bitmap)
{
    uint8_t line[OLED_W + 1];
    line[0] = 0x40;                          // 0x40 = data stream

    for (int page = 0; page < 7; page++) {
        write_reg(Oled_dev, 0x00, 0xB0 | page);          // set page
        write_reg(Oled_dev, 0x00, 0x00);                 // column low nibble = 0
        write_reg(Oled_dev, 0x00, 0x10);                 // column high nibble = 0

        memcpy(&line[1], &bitmap[page * OLED_W], OLED_W);
        ESP_ERROR_CHECK(i2c_master_transmit(dev, line, sizeof(line), -1));
    }
}

void horizontal_to_pages(uint8_t *src, uint8_t *dst)
{
    const int stride = OLED_W / 8;         // bytes per row in source
    memset(dst, 0, OLED_W * OLED_H / 8);

    for (int y = 0; y < OLED_H; y++) {
        for (int x = 0; x < OLED_W; x++) {
            uint8_t byte = src[y * stride + (x / 8)];
            if (byte & (0x80 >> (x % 8))) {              // MSB = leftmost pixel
                dst[(y / 8) * OLED_W + x] |= (1 << (y % 8));  // LSB = top of page
            }
        }
    }
}
void OLED_Clear()
{
    int length = 1023;
    uint8_t buffer[length];

    buffer[0] = 0x40; // Data control byte

    for (int i = 0; i < length; i++)
        buffer[i + 1] = 0;

    i2c_master_transmit(Oled_dev, buffer,
                        length + 1, -1);
}

static int sensor_chr_access(uint16_t conn_handle,
                             uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        uint8_t  buffer[BLE_PAYLOAD_MAX];
        uint16_t len;

        portENTER_CRITICAL(&ble_mux);
        len = latest_payload_len;
        memcpy(buffer, latest_payload, len);
        portEXIT_CRITICAL(&ble_mux);

        return os_mbuf_append(ctxt->om, buffer, len) == 0
               ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = BLE_UUID128_DECLARE(CHAR_UUID),
                .access_cb  = sensor_chr_access,
                .flags      = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &sensor_chr_handle,
            },
            { 0 }
        },
    },
    { 0 }
};

static void ble_app_advertise(void)
{
    struct ble_hs_adv_fields  fields;
    struct ble_gap_adv_params adv_params;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.flags            = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name             = (uint8_t *)BLE_DEVICE_NAME;
    fields.name_len         = strlen(BLE_DEVICE_NAME);
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Advertising fields error: rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Advertising start error: rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising started");
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {

    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            portENTER_CRITICAL(&ble_mux);
            client_conn_handle    = event->connect.conn_handle;
            notifications_enabled = false;
            portEXIT_CRITICAL(&ble_mux);
            ESP_LOGI(TAG, "Client connected");
        } else {
            ESP_LOGE(TAG, "Connection failed: status=%d", event->connect.status);
            ble_app_advertise();
        }
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == sensor_chr_handle) {
            portENTER_CRITICAL(&ble_mux);
            notifications_enabled = event->subscribe.cur_notify;
            portEXIT_CRITICAL(&ble_mux);
            ESP_LOGI(TAG, "Notifications: %s",
                     event->subscribe.cur_notify ? "ENABLED" : "DISABLED");
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Client disconnected, reason=%d", event->disconnect.reason);
        portENTER_CRITICAL(&ble_mux);
        client_conn_handle    = BLE_HS_CONN_HANDLE_NONE;
        notifications_enabled = false;
        portEXIT_CRITICAL(&ble_mux);
        ble_app_advertise();
        break;

    default:
        break;
    }
    return 0;
}

static void ble_app_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);
    ESP_LOGI(TAG, "BLE host synchronized");
    ble_app_advertise();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE host reset, reason=%d", reason);
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void ble_start(void)
{
    int rc;

    ESP_ERROR_CHECK(nimble_port_init());

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(BLE_DEVICE_NAME);

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT count failed: rc=%d", rc);
        return;
    }
    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT register failed: rc=%d", rc);
        return;
    }

    ble_hs_cfg.sync_cb  = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    nimble_port_freertos_init(ble_host_task);
}

int ble_send_value(const void *data, uint16_t len)
{
    if (data == NULL || len == 0 || len > BLE_PAYLOAD_MAX) {
        return -1;
    }

    portENTER_CRITICAL(&ble_mux);
    memcpy(latest_payload, data, len);
    latest_payload_len = len;
    uint16_t conn       = client_conn_handle;
    bool     subscribed = notifications_enabled;
    portEXIT_CRITICAL(&ble_mux);

    if (conn == BLE_HS_CONN_HANDLE_NONE || !subscribed) {
        return -2;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om == NULL) {
        return -3;
    }

    int rc = ble_gatts_notify_custom(conn, sensor_chr_handle, om);
    if (rc != 0) {
        ESP_LOGW(TAG, "Notify failed: rc=%d", rc);
        return -4;
    }
    return 0;
}
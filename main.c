/** @file
 * 
 * @author  Jithin M Das
 * 
 * @brief   Application main file
 */
 
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "timer_handler.h"
#include "nordic_common.h"
#include "softdevice_handler.h"
#include "ble_gap.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "ble_custom_service.h"
#include "nrf_delay.h"
#include "app_uart.h"

#define IS_SRVC_CHANGED_CHARACT_PRESENT 0
#define DEVICE_NAME                     "Custom_Service"

#define CONN_SUP_TIMEOUT    MSEC_TO_UNITS(4000, UNIT_10_MS) 
#define MAX_CONN_INTERVAL   MSEC_TO_UNITS(75, UNIT_1_25_MS)
#define MIN_CONN_INTERVAL   MSEC_TO_UNITS(20, UNIT_1_25_MS)
#define SLAVE_LATENCY       0

#define APP_ADV_INTERVAL                64 
#define APP_ADV_TIMEOUT_IN_SECONDS      180 

#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER)  
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER) 
#define MAX_CONN_PARAMS_UPDATE_COUNT    3  

#define RX_PIN_NUMBER  11
#define TX_PIN_NUMBER  9
#define CTS_PIN_NUMBER 10
#define RTS_PIN_NUMBER 8

#define UART_TX_BUF_SIZE                256
#define UART_RX_BUF_SIZE                256

static ble_cus_t                        m_cus;   
static uint16_t                         m_conn_handle = BLE_CONN_HANDLE_INVALID;

void scheduler_init(void);
static void ble_evt_handler(ble_evt_t * p_ble_evt);
static void on_adv_evt(ble_adv_evt_t ble_adv_evt);
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt);
static void conn_params_error_handler(uint32_t nrf_error);
static void cus_data_handler(ble_cus_t * p_cus, uint8_t * p_data, uint16_t length);
static void on_ble_evt(ble_evt_t * p_ble_evt);
void uart_evt_handler(app_uart_evt_t * p_event);

void ble_stack_init()
{
    uint32_t err_code;
    
    // Initialize soft_device
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_XTAL_20_PPM, NULL);
    
    // Enable BLE stack.
    ble_enable_params_t ble_enable_params;
    memset(&ble_enable_params, 0, sizeof(ble_enable_params));
    ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;
    err_code = sd_ble_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);
    
    // Subscribe for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_handler);
    APP_ERROR_CHECK(err_code);
}

static void ble_evt_handler(ble_evt_t * p_ble_evt)
{
    ble_conn_params_on_ble_evt(p_ble_evt);
    ble_cus_on_ble_evt(&m_cus, p_ble_evt);
    on_ble_evt(p_ble_evt);
    ble_advertising_on_ble_evt(p_ble_evt);
}

/**@brief Function for the Application's S110 SoftDevice event handler.
 *
 * @param[in] p_ble_evt S110 SoftDevice event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt)
{
    uint32_t    err_code;
    
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;
            
        case BLE_GAP_EVT_DISCONNECTED:
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of 
 *          the device. It also sets the permissions and appearance.
 */
void gap_params_init()
{
    uint32_t err_code;
    ble_gap_conn_sec_mode_t sec_mode;
    ble_gap_conn_params_t ble_gap_conn_params;
    
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
    
    err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);
    
    memset(&ble_gap_conn_params, 0, sizeof(ble_gap_conn_params));
    ble_gap_conn_params.conn_sup_timeout = CONN_SUP_TIMEOUT;
    ble_gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    ble_gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    ble_gap_conn_params.slave_latency = SLAVE_LATENCY;
    
    err_code = sd_ble_gap_ppcp_set(&ble_gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

void services_init(void)
{
    uint32_t       err_code;
    ble_cus_init_t cus_init;
    
    memset(&cus_init, 0, sizeof(cus_init));

    cus_init.data_handler = cus_data_handler;
    
    err_code = ble_cus_init(&m_cus, &cus_init);
    APP_ERROR_CHECK(err_code);
}

static void cus_data_handler(ble_cus_t * p_cus, uint8_t * p_data, uint16_t length)
{
}

void advertising_init(void)
{
    uint32_t err_code;
    ble_advdata_t advdata;
//    ble_advdata_t scanrsp;
    
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type = BLE_ADVDATA_FULL_NAME;
    advdata.include_appearance = true;
    advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    
//    memset(&scanrsp, 0, sizeof(scanrsp));
    ble_adv_modes_config_t options = {1};
    options.ble_adv_fast_enabled = BLE_ADV_FAST_ENABLED ;
    options.ble_adv_fast_interval = APP_ADV_INTERVAL;
    options.ble_adv_fast_timeout = APP_ADV_TIMEOUT_IN_SECONDS;
    
    err_code = ble_advertising_init(&advdata, NULL, &options, on_adv_evt, NULL);
    APP_ERROR_CHECK(err_code);
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    
}

static void conn_params_init()
{
    uint32_t    err_code;
    ble_conn_params_init_t conn_params;
    
    conn_params.p_conn_params                   = NULL;
    conn_params.first_conn_params_update_delay  = FIRST_CONN_PARAMS_UPDATE_DELAY;
    conn_params.next_conn_params_update_delay   = NEXT_CONN_PARAMS_UPDATE_DELAY;
    conn_params.max_conn_params_update_count    = MAX_CONN_PARAMS_UPDATE_COUNT;
    conn_params.start_on_notify_cccd_handle     = BLE_GATT_HANDLE_INVALID;
    conn_params.disconnect_on_fail              = false;
    conn_params.evt_handler                     = on_conn_params_evt;
    conn_params.error_handler                   = conn_params_error_handler;    
    
    err_code = ble_conn_params_init(&conn_params);
    APP_ERROR_CHECK(err_code);
}

static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

/**@brief Function for placing the application in low power state while waiting for events.
 */
static void power_manage(void)
{
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}

void data_send()
{
    static uint8_t data_array[BLE_CUS_MAX_DATA_LEN] = "j\n";
    static uint8_t index = 5;
    uint32_t       err_code;
    
    err_code = ble_cus_string_send(&m_cus, data_array, 2);
//    printf("string send = %d\n", err_code);
    if (err_code != NRF_ERROR_INVALID_STATE)
    {
        APP_ERROR_CHECK(err_code);
    }
}

void uart_init(void)
{
    uint32_t err_code;
    const app_uart_comm_params_t comm_params = {
        RX_PIN_NUMBER,
        TX_PIN_NUMBER,
        RTS_PIN_NUMBER,
        CTS_PIN_NUMBER,
        APP_UART_FLOW_CONTROL_DISABLED,
        false,
        UART_BAUDRATE_BAUDRATE_Baud38400
    };
    
    APP_UART_FIFO_INIT( &comm_params,
                        UART_RX_BUF_SIZE,
                        UART_TX_BUF_SIZE,
                        uart_evt_handler,
                        APP_IRQ_PRIORITY_LOW,
                        err_code );
    APP_ERROR_CHECK(err_code);
}

void uart_evt_handler(app_uart_evt_t * p_event)
{
    
    static uint8_t data_array[BLE_CUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') || (index >= (BLE_CUS_MAX_DATA_LEN)))
            {
                err_code = ble_cus_string_send(&m_cus, data_array, index);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
                
                index = 0;
            }
            printf("%s\n", (char*)data_array);
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}

int main(void)
{
    uint32_t err_code;
    
    timer_init();
    uart_init();
    printf("Hello world\n");
    ble_stack_init();
    gap_params_init();
    services_init();
    advertising_init();
    conn_params_init();
    
    err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
    
    for(;;)
    {
//        power_manage();
//        data_send();
//        printf("cus init = %d,%d\n", ((&m_cus)->conn_handle == BLE_CONN_HANDLE_INVALID), (!(&m_cus)->is_notification_enabled));        
//        nrf_delay_ms(100);
    }
}

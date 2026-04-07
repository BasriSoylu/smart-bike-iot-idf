#pragma once

#include "driver/uart.h"

typedef struct {
    uart_port_t           port_num;      /*!< UART port numarasi             */
    int                   tx_pin;        /*!< UART TX pin numarasi           */
    int                   rx_pin;        /*!< UART RX pin numarasi           */
    int                   baud_rate;     /*!< UART baud rate                 */
    int                   rx_buf_size;   /*!< Alim tamponu boyutu (bayt)     */
    int                   tx_buf_size;   /*!< Gonderim tamponu boyutu (bayt) */
    uart_word_length_t    veri_bitleri;  /*!< Veri bit sayisi                */
    uart_parity_t         parity;        /*!< Parite ayari                   */
    uart_stop_bits_t      stop_bitleri;  /*!< Stop bit sayisi                */
    uart_hw_flowcontrol_t akis_kontrolu; /*!< Donanim akis kontrolu          */
} uart_cfg_t;

typedef uart_port_t uart_handle_t;

/****************************************************************
 * Versiyon       : v1.1.0
 * Branch         : main
 * Kullanilan SDK : ESP-IDF v5.5
 * Kullanilan IDE : Visual Studio Code
 *===============================================================
 * Yazar         : Hasan Basri SOYLU
 * Tarih         : 08.04.2026
 *===============================================================
 * Aciklama      : UART sub-modülü tip tanımları. uart_cfg_t
 *                 yapılandırma struct'ı ve uart_handle_t
 *                 tanıtıcı tipini tanımlar.
 ****************************************************************/

#pragma once

#include "driver/uart.h"

typedef struct {
    uart_port_t           port_num;      /*!< UART port numarası            */
    int                   tx_pin;        /*!< UART TX pin numarası          */
    int                   rx_pin;        /*!< UART RX pin numarası          */
    int                   baud_rate;     /*!< UART baud rate                */
    int                   rx_buf_size;   /*!< Alım tamponu boyutu (bayt)    */
    int                   tx_buf_size;   /*!< Gönderim tamponu boyutu (bayt)*/
    uart_word_length_t    veri_bitleri;  /*!< Veri bit sayısı               */
    uart_parity_t         parity;        /*!< Parite ayarı                  */
    uart_stop_bits_t      stop_bitleri;  /*!< Stop bit sayısı               */
    uart_hw_flowcontrol_t akis_kontrolu; /*!< Donanım akış kontrolü         */
} uart_cfg_t;

typedef uart_port_t uart_handle_t;

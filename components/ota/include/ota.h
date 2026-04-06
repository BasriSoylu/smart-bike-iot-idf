#pragma once

// Sunucudan versiyon kontrol et, yeni varsa firmware indir ve flash'la.
// Döndürür: 0 = güncelleme yok, 1 = güncellendi (restart gelecek), -1 = hata
int ota_check_and_update(void);

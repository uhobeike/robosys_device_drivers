# robosys_device_drivers
Raspberry Pi 3B+に接続した可変抵抗により、3つのLEDおよびLCDの制御を行うことができるパッケージです。

## 動作環境
#### ハードウェア
・Raspberry Pi 3B+

・ノートPC

#### ソフトウェア
・Raspberry Pi 3B+（ubuntu 18.04 server）

・ノートPC（ubuntu 18.04 desktop）

## インストール & ビルド
#### Raspberry Pi 3B+
```
~$ git clone robosys_device_drivers 
~$ cd ~/robosys_device_drivers/src/drivers
~/robosys_device_drivers/src/drivers$ make
~/robosys_device_drivers/src/drivers$ ./install.bash
~/robosys_device_drivers/src/drivers$ g++ -o test 3_led_test.cpp
```

## 実行方法
```
~/robosys_device_drivers/src/drivers$ ./test
```

## 実行結果
#### 画像をクリックすると、youtubeサイトに飛べます。
|lcd & led control|lcd only control|
|---|---|
|[![robosys 2020](https://img.youtube.com/vi/JrrdK_rhAd8/0.jpg)](https://www.youtube.com/watch?v=JrrdK_rhAd8)|[![robosys 2020 lcd_control](https://img.youtube.com/vi/Y0zuZoEhaN8/0.jpg)](https://www.youtube.com/watch?v=Y0zuZoEhaN8)|


## 回路構成
#### 使用した電子部品（おおまか）
| 電子部品名                                           | 電子部品情報                                  | 
| ---------------------------------------------------- | --------------------------------------------- | 
| 12bit 4ch AD コンバータ MCP3204-BI/P                 | https://akizukidenshi.com/catalog/g/gI-00239/ | 
| I2C接続小型LCDモジュール(8×2行) ピッチ変換モジュール | https://akizukidenshi.com/catalog/g/gM-09109/ | 
| 半固定ボリューム 100Ω                               | https://akizukidenshi.com/catalog/g/gP-06099/ | 
| 高輝度5mm赤LED OSR5MA511A-VW                         | https://akizukidenshi.com/catalog/g/gI-02423/ | 

## ソースコード一覧
| ソースコード名  | 役割                                         | 
| --------------- | -------------------------------------------- | 
| 3_led.c         | デバイスドライバ作成ファイル                 | 
| i2c-lcd.h       | lcd用ヘッダーファイル                        | 
| 3_led_test.cpp  | 作成したデバイスドライバを使用して、色々やる | 
| install.bash    | カーネルモジュールの挿入および権限設定       | 
| Makefile        | ビルド用ファイル                             | 

## デバイスドライバ一覧
| デバイスドライバ名 | 役割                                      | 
| ------------------ | ----------------------------------------- | 
| /dev/lcd_row_10    | lcdの一行目に出力                         | 
| /dev/lcd_row_20    | lcdの二行目に出力                         | 
| /dev/lcd_clear0    | lcdに出力されているものすべて削除         | 
| /dev/analog_read0  | MCP3204の1chに繋がっているAD変換値を読む | 
| /dev/led_blink0    | GPIO20に繋がっているLEDの点滅             | 
| /dev/led_blink1    | GPIO21に繋がっているLEDの点滅             | 
| /dev/led_blink2    | GPIO26に繋がっているLEDの点滅             | 
## 参考元
・https://github.com/rt-net/RaspberryPiMouse

・https://qiita.com/iwatake2222/items/26d5f7f4894ccc4ce227

・https://qiita.com/iwatake2222/items/dbc544864f0e9873270a

・https://moba1.hatenablog.com/entry/2019/10/09/114145

・https://www.amazon.co.jp/gp/product/488337940X/ref=ppx_yo_dt_b_asin_title_o01_s00?ie=UTF8&psc=1
## ライセンス

このリポジトリはGPLライセンスで公開されています。詳細は[COPYING](./COPYING)を確認してください。

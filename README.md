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
#### robosys_device_drivers_2020    robosys_device_drivers_2020 (lcd_control)

|lcd & led control|lcd only control|
|---|---|
|[![robosys 2020](https://img.youtube.com/vi/JrrdK_rhAd8/0.jpg)](https://www.youtube.com/watch?v=JrrdK_rhAd8)|[![robosys 2020 lcd_control](https://img.youtube.com/vi/Y0zuZoEhaN8/0.jpg)](https://www.youtube.com/watch?v=Y0zuZoEhaN8)|


## 回路構成

## ソースコード一覧

## デバイスドライバ一覧

## ライセンス

このリポジトリはGPLライセンスで公開されています。詳細は[COPYING](./COPYING)を確認してください。

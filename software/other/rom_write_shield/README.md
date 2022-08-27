# EPROM Programming Shield を用いてEPROMの読み書きをするプログラム

## eprom_writer_stk500

### 準備

1. 使用するボードに合わせてスケッチを設定する
   * v1.0 : そのまま (`REVERSE_PIN_A0_TO_A5` を定義する)
   * v1.1 : 先頭の `#if 1` を `#if 0` にするなど (`REVERSE_PIN_A0_TO_A5` を定義しない)
2. スケッチをArduino UNO または互換機に書き込む
   * UNO用のシールドを接続でき、I/Oが5Vであれば、他のタイプでも多分可
3. シールドを接続する
4. VCCおよびVPPの電圧をEPROMの仕様に合わせて調整する
   1. Vrawの半固定抵抗で昇圧する電圧を調整する
   2. VCCおよびVPPの半固定抵抗で昇圧した電圧の分圧を調整する

### 使用方法

STK500のプロトコルを(一部)実装しており、avrdudeを用いて操作できる。

例えば以下のようなコマンドを用いる。  
(ファイルのパスやポート名は環境に合わせて調整すること)

```
avrdude -C eprom_writer_stk500.conf -c eprom_writer -p eprom -P COMx -U eeprom:w:hoge.hex:i
```

「flashに対する読み書き」で、EPROM Address Expansion Board や
EPROM Zero Pressure Write Board によるアドレス拡張を行う読み書きを表す。

「eepromに対する読み書き」で、アドレス拡張を行わない読み書きを表す。

なお、avrdudeの仕様で、データのバイト数が奇数だと一部書き込まれないことがあるようなので注意。  
(flashに書き込むモードの際、2バイトずつしか書き込めないため？)

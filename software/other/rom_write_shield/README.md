# EPROM Programming Shield ��p����EPROM�̓ǂݏ���������v���O����

## eprom_writer_stk500

### ����

1. �g�p����{�[�h�ɍ��킹�ăX�P�b�`��ݒ肷��
   * v1.0 : ���̂܂� (`REVERSE_PIN_A0_TO_A5` ���`����)
   * v1.1 : �擪�� `#if 1` �� `#if 0` �ɂ���Ȃ� (`REVERSE_PIN_A0_TO_A5` ���`���Ȃ�)
2. �X�P�b�`��Arduino UNO �܂��͌݊��@�ɏ�������
   * UNO�p�̃V�[���h��ڑ��ł��AI/O��5V�ł���΁A���̃^�C�v�ł�������
3. �V�[���h��ڑ�����
4. VCC�����VPP�̓d����EPROM�̎d�l�ɍ��킹�Ē�������
   1. Vraw�̔��Œ��R�ŏ�������d���𒲐�����
   2. VCC�����VPP�̔��Œ��R�ŏ��������d���̕����𒲐�����

### �g�p���@

STK500�̃v���g�R����(�ꕔ)�������Ă���Aavrdude��p���đ���ł���B

�Ⴆ�Έȉ��̂悤�ȃR�}���h��p����B  
(�t�@�C���̃p�X��|�[�g���͊��ɍ��킹�Ē������邱��)

```
avrdude -C eprom_writer_stk500.conf -c eprom_writer -p eprom -P COMx -U eeprom:w:hoge.hex:i
```

�uflash�ɑ΂���ǂݏ����v�ŁAEPROM Address Expansion Board ��
EPROM Zero Pressure Write Board �ɂ��A�h���X�g�����s���ǂݏ�����\���B

�ueeprom�ɑ΂���ǂݏ����v�ŁA�A�h���X�g�����s��Ȃ��ǂݏ�����\���B

�Ȃ��Aavrdude�̎d�l�ŁA�f�[�^�̃o�C�g��������ƈꕔ�������܂�Ȃ����Ƃ�����悤�Ȃ̂Œ��ӁB  
(flash�ɏ������ރ��[�h�̍ہA2�o�C�g�������������߂Ȃ����߁H)

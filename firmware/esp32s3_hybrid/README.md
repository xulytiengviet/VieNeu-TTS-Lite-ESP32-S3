# ESP32-S3 hybrid VieNeu-TTS · HTTP WAV → I2S

**Prototype, không phải VieNeu ONNX chạy trên MCU.** Host/Orange Pi chạy VieNeu v3 Nano và tổng hợp WAV mono 16-bit 24 kHz. ESP32-S3 tải **toàn bộ** WAV từ LAN HTTP, kiểm tra RIFF/WAVE, chuyển mono thành stereo và phát qua I2S DAC/amp (ví dụ MAX98357A).

## Hardware

- ESP32-S3 (khuyến nghị board có PSRAM; firmware dành 1 MiB PSRAM cho buffer WAV).
- I2S amp DAC MAX98357A (hoặc tương đương, có dây GND chung), loa theo thông số amp.
- Chân ví dụ: ESP32-S3 GPIO4 → BCLK, GPIO5 → LRC/WS, GPIO6 → DIN. **Không dùng những chân đã có chức năng xung đột trên board thực tế**; thay trong menuconfig.
- Nguồn DAC/amp phù hợp thông số nhà sản xuất. Không đấu loa trực tiếp vào GPIO.

## Tạo API host

Ở thư mục gốc repository:

```bash
uv sync
uv run python scripts/download_models.py --skip-moss
export VIENEU_MODE=v3nano
export VIENEU_ONNX_DIR="$PWD/models/VieNeu-TTS-v3-Nano"
export HF_HUB_OFFLINE=1
export HOST=0.0.0.0 PORT=8000
uv run python -m apps.openai_speech
```

Bằng máy khác trong LAN, kiểm tra `curl http://HOST_IP:8000/health`. Endpoint phải trả về WAV đúng thông số. Nếu key được bật trên host, điền key ở menuconfig; **token sẽ truyền plaintext qua HTTP**, chỉ dùng mạng LAB tin cậy. Không mở port Internet.

## Flash firmware

Cài ESP-IDF v5.3 trở lên, đã chạy export shell chính thức:

```bash
cd firmware/esp32s3_hybrid
idf.py set-target esp32s3
idf.py menuconfig
# -> VieNeu Hybrid Demo: SSID, Wi-Fi password, LAN API URL,
#    voice, text, BCLK/WS/DIN pins
# -> Component config: bật PSRAM theo loại board nếu có
idf.py build
idf.py -p COM5 flash monitor            # Windows: thay COM5
# hoặc -p /dev/ttyUSB0 trên Linux
```

Config mặc định URL `http://192.168.1.10:8000/v1/audio/speech` là placeholder, **phải thay** bằng IP host LAN của mình.

Sau khi kết nối Wi-Fi, firmware phát một câu rồi dừng. Log có: HTTP status, bytes tải về, thời gian HTTP, thông số WAV, free heap và thời gian ghi I2S. Buffer WAV tối đa 1 MiB; đoạn dài hơn sẽ fail an toàn. Kết quả phụ thuộc firmware/board; chưa có log build hay phép đo board được xác nhận từ tác giả trang.

### Hạn chế có chủ ý

- HTTP **không mã hóa**: chỉ dùng trong LAN demo. Production cần HTTPS có CA validation, secret provisioning, retry, rate limit và phân đoạn audio hợp lý.
- Mặc định request tạo audio một lần và tải xong mới phát; **không phải streaming có độ trễ thấp**.
- Parser chỉ nhận WAV RIFF PCM **mono 16-bit 24000 Hz**; WAV 32-bit float/stereo/48 kHz hoặc codec khác bị từ chối.
- Buffer 1 MiB cần PSRAM; trên board không có PSRAM sẽ không chạy nguyên cấu hình.
- Voice cloning/onnxruntime/sea-g2p/CVNSS4.0 không chạy trên ESP32-S3.
- I2S DMA nhận mẫu stereo từ WAV mono, nhưng không kiểm chứng analog amp hoặc dropout bằng phần mềm; cần đo thực tế.

## Kiểm thử parser thuần C trước flash

```bash
cc -std=c11 -Wall -Wextra -Werror -I main \
  main/wav_pcm.c tests/test_wav_pcm.c \
  -o /tmp/test_wav && /tmp/test_wav
```

Xem [docs/EDGE_EXPERIMENTS.md](../../docs/EDGE_EXPERIMENTS.md) để ghi chép benchmark và gate công bố.

**Giấy phép:** Apache-2.0 theo repository. Mô hình/trọng số được ràng buộc thêm bởi giấy phép upstream tương ứng.

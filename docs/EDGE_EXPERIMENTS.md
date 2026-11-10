# VieNeu Edge Lab · Hướng dẫn thử nghiệm có thể tái lập

**Cập nhật:** 2026-11-10 · **Tác giả phần thực nghiệm:** Long Ngo · **Repository:** [VieNeu-TTS-Lite-ESP32-S3](https://github.com/xulytiengviet/VieNeu-TTS-Lite-ESP32-S3) · **Trang tương tác:** [docs/index.html](index.html)

> **Phân biệt ba lớp:** CVNSS4.0 là bộ quy tắc/biểu diễn chữ viết tiếng Việt; Base27 ở đây là transport encoding nghiêm ngặt trên **A–Z + SPACE**; ternary weights thuộc bài toán lượng tử hóa mô hình (−1, 0, +1). Không tự suy ra hiệu quả nén TTS từ đẳng thức 27=3³.

## 0. Trạng thái kiểm chứng

| Thành phần | Phạm vi | Trạng thái |
| --- | --- | --- |
| VieNeu v3 Nano CPU / ONNX 24 kHz | Backend hiện có | Mã nguồn gốc trong repository |
| Base27 27 symbols → 3 trits | Python + JS | Reference implementation, host unit tests |
| CVNSS4.0 Unicode → canonical ASCII | Converter / rules / ambiguity resolution | **Chưa tích hợp vào pipeline**, không nhận mình đã chuyển |
| Thiết bị ESP32-S3 phát WAV qua I2S | ESP-IDF hybrid LAN | Source prototype; **cần flash + đo trên board** |
| Student TTS INT8 | Distillation từ teacher | **Chưa có checkpoint/benchmark** |
| Ternary inference trên ESP32-S3 | QAT/distillation + kernel | **Chưa được hiện thực hóa** |

Cấu hình tham khảo: ESP32-S3 N8R8 hoặc N16R8 với đủ RAM, DAC I2S MAX98357A và loa phù hợp; bộ xử lý VieNeu Nano chạy trên Orange Pi, mini PC hoặc Instabox. ESP32-S3 **không** chạy bốn ONNX graph gốc trong prototype này.

## 1. Chạy baseline VieNeu trên host/Orange Pi

Cài Python 3.10+, [uv](https://docs.astral.sh/uv/), đủ RAM và kết nối mạng để tải trọng số lần đầu.

```bash
git clone https://github.com/xulytiengviet/VieNeu-TTS-Lite-ESP32-S3.git
cd VieNeu-TTS-Lite-ESP32-S3
uv sync
uv run python scripts/download_models.py --skip-moss
uv run python scripts/download_models.py --offline-check
uv run python examples/main_v3nano.py
uv run python tools/model_audit.py
```

Kết quả: `output_nano.wav` và log thời gian/RTF của `examples/main_v3nano.py`. Ghi thông số host, CPU, RAM, phiên bản, duration audio, thời gian wall-clock và peak RSS. **RTF = thời gian sinh âm / thời lượng âm thanh**; phải đo trên thiết bị mục tiêu, không suy luận từ Host.

Chạy API LAN:

```bash
export VIENEU_MODE=v3nano
export VIENEU_ONNX_DIR="$PWD/models/VieNeu-TTS-v3-Nano"
export HOST=0.0.0.0 PORT=8000
export HF_HUB_OFFLINE=1
uv run python -m apps.openai_speech
```

Kiểm tra trong cùng mạng:

```bash
curl -fsS http://127.0.0.1:8000/health
curl -fsS http://127.0.0.1:8000/v1/audio/speech \
  -H 'Content-Type: application/json' \
  -d '{"model":"vieneu-v3-nano","voice":"Mai Anh","input":"Xin chào Việt Nam","response_format":"wav","sample_rate":24000}' -o sample.wav
```

Nếu có `VIENEU_API_KEY`, thêm `Authorization: Bearer ...`. Để API không có khóa **chỉ dùng LAN tin cậy, không mở trực tiếp Internet**. Endpoint và hỗ trợ `voice` phụ thuộc phiên bản backend hiện hành; kiểm tra `/v1/voices/presets` nếu gặp lỗi giọng.

## 2. Base27 / CVNSS4.0 / round-trip

**Quy ước cố định**: `ABCDEFGHIJKLMNOPQRSTUVWXYZ ` = chỉ số 0..26; mỗi ký hiệu mã hóa 3 trít hệ cơ số 3 `000..222`. Ví dụ `A=000`, `Z=221`, `SPACE=222`.

```bash
python tools/base27.py encode "XIN CHAO VIET NAM"
python tools/base27.py check "XIN CHAO VIET NAM"
python tools/base27.py decode 000001002  # => ABC; chuỗi trít truyền liền mạch
python -m unittest discover -s tests -p 'test_base27_reference.py' -v
```

`decode` yêu cầu input trít **liền mạch**; không bao gồm dấu cách trong chuỗi mã. Trình mô phỏng web hiển thị các nhóm 3 trít bằng khoảng cách cho dễ đọc và tự gỡ khoảng trắng khi copy. Unicode tiếng Việt `"Xin chào"` bị từ chối cố ý.

Để tích hợp engine CVNSS4.0 thực sự, cần hợp đồng giao diện:

```text
Unicode NFC (giữ bản gốc)
   ├─ validate UTF-8 + checksum
   ├─ CVNSS4.0.encode(text) -> [A-Z SPACE] (được kiểm toán ngoại lệ/nhập nhằng)
   ├─ Base27.encode(ascii) -> trits
   ├─ Base27.decode(trits) -> ascii
   └─ CVNSS4.0.decode(ascii, context) -> Unicode NFC
            ↳ so sánh văn bản gốc, lỗi phát âm, thanh điệu, trường hợp nhập nhằng
```

Cần đo **exact-match** và **character error rate** ở chiều phục hồi, và đánh dấu mơ hồ thay vì bí mật làm mất dữ liệu. Nếu CVNSS4.0 không thể biểu diễn ký tự ngoài bảng, sử dụng sidecar/escape/versioned payload ở tầng *ứng dụng*, không tự phá vỡ mapping chuẩn Base27. Không được thay `sea-g2p` trong VieNeu nếu chưa có bộ kiểm thử G2P/âm học đối chiếu.


## 2b. Kiểm toán CVNSS4.0 bằng dữ liệu đầu ra thật

Trình tham chiếu Base27 **không** tự tạo chuỗi CVNSS4.0 từ Unicode. Để nối với bộ chuyển đổi CVNSS4.0 đang nghiên cứu, xuất CSV UTF-8 từ **converter đã kiểm chứng**, gồm đúng các cột:

```csv
unicode_source,cvnss_ascii,unicode_restored
```

Mỗi dòng phải do converter thật sinh ra: văn bản gốc Unicode, payload ASCII A–Z + SPACE, và văn bản phục hồi bằng thuật toán giải mã có ngữ cảnh. **Không** lấy câu chữ không dấu thông thường thay thế CVNSS4.0 mà coi như chuẩn.

```bash
python -m tools.cvnss_audit path/to/exported_pairs.csv
```

Kết quả JSON gồm số dòng, số dòng đạt round-trip Base27, số dòng khớp Unicode NFC, vị trí lỗi và `pass`. Tham chiếu này **không xác nhận đúng quy tắc CVNSS4.0, không xác nhận âm vị/thanh điệu**, và chỉ báo PASS về hợp đồng I/O. Với bộ dữ liệu có nhập nhằng, cần thêm đối chiếu thủ công và bộ giải mơ hồ chuyên biệt.

## 3. Ước tính trọng số tam phân (host only)

```bash
python tools/ternary_budget.py --parameters 48000000
python tools/ternary_budget.py --parameters 1000000
```

- FP32: 32 bit/trọng số;
- INT8: 8 bit/trọng số;
- đóng gói ternary hai bit: 2 bit/trọng số;
- entropy tam phân lý tưởng: `log2(3) ≈ 1.585` bit/trọng số.

48 triệu tham số có trọng số lý thuyết tương ứng **192 MB / 48 MB / 12 MB / 9.51 MB** (hệ MB thập phân). **Không** tính activation, kernel, cache, codec/vocoder, alignment, chất lượng âm thanh, peak RAM hay thời gian giải mã. Tối ưu ternary trên CPU Xtensa nhị phân phải được đo so sánh với INT8 bằng **cùng student model / cùng dataset / cùng thiết bị**.

## 4. Prototype firmware ESP32-S3 · Hybrid HTTP WAV → I2S

Firmware: [firmware/esp32s3_hybrid](../firmware/esp32s3_hybrid/README.md). Yêu cầu [ESP-IDF v5.3+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/index.html), ESP32-S3, USB-UART, Wi-Fi 2.4 GHz, module I2S DAC/amp. Chọn chân theo board; ví dụ tổng quát: BCLK=GPIO4, LRCK/WS=GPIO5, DIN=GPIO6, GND chung. Kiểm tra sơ đồ phần cứng trước khi cấp nguồn và đấu loa đúng thiết kế amplifier.

```bash
cd firmware/esp32s3_hybrid
idf.py set-target esp32s3
idf.py menuconfig   # VieNeu Hybrid Demo: SSID, password, API HTTP LAN, voice, I2S GPIO
idf.py build
idf.py -p <SERIAL_PORT> flash monitor
```

**Chú ý:** demo này dùng **HTTP LAN** (không TLS) để đơn giản hóa môi trường thử nghiệm. Không đặt API key vào firmware khi chạy trên mạng không tin cậy. Có thể bật bearer key trên LAN để giảm request trái phép, nhưng token trên HTTP vẫn truyền không mã hóa. Production cần HTTPS đúng chuẩn, xác minh server, lưu secret an toàn và hạn chế truy cập. ESP32-S3 tải toàn bộ WAV (giới hạn 1 MiB) rồi phát; chưa progressive streaming và chưa thực thi mô hình TTS trên board.

Điều kiện hỗ trợ WAV: RIFF/WAVE PCM mono, 16-bit, **24,000 Hz**, chiều dài <= 1 MiB. Firmware kiểm tra header, chunk và dữ liệu trước khi cho vào DMA I2S stereo (nhân đôi hai kênh), không mặc định tiêu đề WAV luôn dài 44 byte. Chương trình mẫu thực hiện một yêu cầu sau khi kết nối; có thể reset để đo lại.

Kiểm thử parser trên host GCC trước khi flash:

```bash
cc -std=c11 -Wall -Wextra -Werror \
  -Ifirmware/esp32s3_hybrid/main \
  firmware/esp32s3_hybrid/main/wav_pcm.c \
  firmware/esp32s3_hybrid/tests/test_wav_pcm.c \
  -o /tmp/test_wav && /tmp/test_wav
```

### Ghi log đo

| Metric | Đơn vị | Điều kiện |
| --- | --- | --- |
| Board / CPU / flash / PSRAM | tên, MB, MHz | luôn công bố |
| Firmware version / toolchain | IDF version, commit SHA | luôn công bố |
| Free heap before / after | bytes | `esp_get_free_heap_size` |
| Request duration | ms | từ gửi POST đến đọc hết WAV |
| WAV size & duration | bytes / s | từ chunk `data`, không đoán từ HTTP Content-Length |
| I2S writing duration | ms | từ bắt đầu ghi đến ghi đủ mẫu |
| Dropouts / underruns | số lần | tai nghe/logic analyzer; chưa tự đo bằng firmware |
| RTF of neural generation | t/t | **đo bên host**, không dùng request roundtrip làm RTF |
| Quality | MOS/CMOS + CER/WER | câu có dấu, thanh điệu, tên riêng, số, ngoại lệ |

**PASS Gate 01** chỉ khi UART log xác nhận HTTP 200, WAV hợp lệ, I2S ghi đủ, âm thanh nghe đúng, thời gian và heap được ghi. **Không** ghi '100% ESP32 chạy VieNeu TTS offline' từ phép thử hybrid.

## 5. Nghiên cứu student TTS INT8 / Ternary

Lộ trình khả thi, **chưa có pipeline training hay checkpoint trong repository**:

1. Xây tập chuẩn hóa câu Việt có quyền sử dụng, nhãn âm vị/thanh điệu, cặp WAV tham chiếu và baseline teacher VieNeu, tránh train độc lập trực tiếp từ trít mà không có alignment.
2. Student khoảng 0.5–2M tham số là **mục tiêu thiết kế**, không cam kết đạt chất lượng. Giọng cố định, 16 kHz, câu ngắn; ưu tiên vocoder nhỏ hoặc bộ ghép nối có kiểm chứng.
3. INT8 đầu tiên bằng ESP-DL/ESP-NN hoặc kernel phù hợp; audit operator từng layer và peak activation; đo RTF trên ESP32-S3.
4. Sau khi có baseline đạt tiêu chí, thử ternary-aware training/weight quantization với bộ trọng số -1/0/+1, hiệu chỉnh thang và packing, benchmark cùng student / cùng dữ liệu.
5. Gate cuối phải có chất lượng phát âm (đặc biệt 6 thanh điệu), tốc độ, bộ nhớ, điện năng, số lượng câu test và sai số so với baseline.

**Không đánh đồng:** Base27 sử dụng bộ ba chữ số `0,1,2` để mã ký hiệu; ternary ML thường dùng trọng số `-1,0,+1`. Mã Base27 không làm tự động giảm kích thước ONNX.

## 6. Bảo trì / giấy phép / tài liệu

Repo hiện có giấy phép Apache-2.0 và credit upstream; giữ nguyên. Các tệp mới được đóng góp theo giấy phép repository trừ khi có quy định khác. Phải tuân thủ giấy phép model weights và dữ liệu huấn luyện riêng.

- [VieNeu-TTS upstream](https://github.com/pnnbao97/VieNeu-TTS)
- [VieNeu v3 Nano weights](https://huggingface.co/pnnbao-ump/VieNeu-TTS-v3-Nano)
- [ESP32-S3 I2S STD](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2s.html)
- [ESP-DL](https://github.com/espressif/esp-dl)
- [ESP-IDF Wi-Fi station](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/wifi.html)
- [ESP-IDF HTTP Client](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/protocols/esp_http_client.html)

## 7. GitHub Pages

GitHub repository → **Settings → Pages** → Source **Deploy from a branch** → Branch **main** / folder **/docs** → Save.

Trang dự kiến tại `https://xulytiengviet.github.io/VieNeu-TTS-Lite-ESP32-S3/`. Nếu đang có cấu hình GitHub Actions Pages riêng, không thay cấu hình hiện tại mà kiểm tra conflict trước.

Các bài kiểm tra host chạy trong workflow `.github/workflows/edge-lab.yml`; firmware ESP-IDF cần môi trường IDF, không được workflow host tuyên bố đã build hoặc thử trên board.

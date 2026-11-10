# VieNeu-TTS Lite · ESP32-S3 Edge Research Lab

**CVNSS4.0 · Base27 · Ternary · Vietnamese Tiny TTS · ESP32-S3**

[**🌐 Mở trang thí nghiệm GitHub Pages**](https://xulytiengviet.github.io/VieNeu-TTS-Lite-ESP32-S3/) · [**Trang nguồn trong repository**](docs/index.html) · [**Hướng dẫn thực nghiệm**](docs/EDGE_EXPERIMENTS.md) · [**Firmware ESP-IDF**](firmware/esp32s3_hybrid/README.md)

> **Trạng thái:** Base27 + bộ kiểm thử và firmware *hybrid* tham chiếu được cung cấp; **chưa chứng minh VieNeu ONNX chạy trực tiếp trên ESP32-S3**. Nhánh student INT8/ternary vẫn là nghiên cứu. Mã CVNSS4.0 Unicode↔ASCII phải được tích hợp từ converter đã kiểm chứng; không được coi Base27 là CVNSS4.0.

## Bắt đầu nhanh với thành phần nghiên cứu

```bash
python -m unittest discover -s tests -p 'test_base27_reference.py' -v
python -m unittest discover -s tests -p 'test_cvnss_audit.py' -v
python tools/base27.py encode "XIN CHAO VIET NAM"
python tools/ternary_budget.py --parameters 48000000
```

- **Mã hóa**: `tools/base27.py` bảo toàn A–Z + SPACE ↔ ba chữ số `0/1/2`; không tự chuyển Unicode. Để kiểm tra CVNSS4.0 thật, xuất CSV với ba trường `unicode_source,cvnss_ascii,unicode_restored`, chạy `python -m tools.cvnss_audit data.csv`.
- **Ngân sách trọng số**: `tools/ternary_budget.py` so sánh FP32, INT8, ternary 2-bit và giới hạn lý tưởng; không phải benchmark tốc độ.
- **Hybrid ESP32-S3**: `firmware/esp32s3_hybrid/` dùng ESP-IDF Wi-Fi → VieNeu Nano HTTP LAN → RIFF/WAV → I2S; mô hình neural vẫn chạy trên host/Orange Pi.
- **Giao diện**: `docs/index.html`, `docs/style.css`, `docs/app.js`; không CDN, hoạt động offline cho phần mô phỏng.
- **Kiểm thử nhẹ**: `.github/workflows/edge-lab.yml` kiểm tra Python, parser C và JavaScript (không giả lập đã flash board).

Để xuất bản Pages: **Settings → Pages → Deploy from a branch → main /docs**. Giữ nguyên bản ghi, pipeline và giấy phép từ upstream; phần bổ sung này không thay thế backend VieNeu-TTS.

---

## Backend VieNeu-TTS gốc

# VieNeu-TTS (Instabox · Orange Pi)

Bản rút gọn để chạy **VieNeu-TTS v3 Nano** (ONNX/CPU, 24 kHz) trên 2 nguồn

- **Instabox** → API + URL public HTTPS: truy cập TTS online
- **Orange Pi (RK3588 / aarch64)** → xem [ORANGEPI.md](ORANGEPI.md): truy cập offline local

> Trên CPU (Instabox free / Orange Pi) chỉ dùng **v3 Nano @ 24 kHz**

## Triển khai Nhanh: deploy trên Instabox

### 1) Chuẩn bị box

```bash
# chọn account Instabox rồi resume/start box
instabox resume <box>   # hoặc: instabox start <box>
instabox exec <box> -- 'uname -m && python3 --version'
```

### 2) Đưa mã nguồn lên box

```bash
# từ máy host (repo này)
instabox push <box> ./VieNeu-TTS /data/workspace/VieNeu-TTS
# hoặc clone/pull git trên box nếu đã đẩy lên GitHub
```

### 3) Cài deps + model (trên box)

```bash
instabox exec <box> -- 'export TMPDIR=/data/home/tmp HF_HOME=/data/home/.cache/huggingface XDG_CACHE_HOME=/data/home/.cache
mkdir -p "$TMPDIR" "$HF_HOME"
cd /data/workspace/VieNeu-TTS
uv sync
uv run python scripts/download_models.py
'
```

Model flat offline sẽ nằm tại:

`models/VieNeu-TTS-v3-Nano/` (hoặc dùng HF cache dưới `HF_HOME`)

### 4) Chạy API trên box

```bash
instabox exec <box> -- 'cd /data/workspace/VieNeu-TTS
export TMPDIR=/data/home/tmp HOME=/data/home
export HF_HOME=/data/home/.cache/huggingface
export HOST=0.0.0.0 PORT=8000
export VIENEU_MODE=v3nano
export VIENEU_ONNX_DIR=/data/workspace/VieNeu-TTS/models/VieNeu-TTS-v3-Nano
export VieNeuTTS_WEB_ENABLED=true
# optional:
# export VIENEU_API_KEY=...
nohup uv run python -m apps.openai_speech > /data/workspace/vieneu-api.log 2>&1 &
'
```

Hoặc dùng script sẵn (nếu đã cấu hình key/web flag trên box):

```bash
instabox exec <box> -- '/data/workspace/start-vieneu-api.sh'
```

### 5) Publish URL online

```bash
instabox port publish <box> 8000 --name 'VieNeu TTS'
# ví dụ: https://8000-<machine-id>.instabox.tech
```

Kiểm tra:

```bash
curl -s https://8000-<id>.instabox.tech/health
curl -s https://8000-<id>.instabox.tech/v1/voices/presets \
  -H "Authorization: Bearer $VIENEU_API_KEY"
```

## Auth & Web UI

| Đường dẫn | Hành vi |
|---|---|
| `GET /` | Web UI — bật/tắt bằng `VieNeuTTS_WEB_ENABLED=true\|false` |
| `/web/voices`, `/web/speech`, `/web/history` | Helper cho UI, **không** cần Bearer |
| `/v1/*` | API OpenAI-compatible — **cần** `Authorization: Bearer $VIENEU_API_KEY` nếu key được set |
| `/health` | Luôn mở |

- Web tắt → `/` trả **“Webapp đang đóng”** (403)
- File WAV UI lưu dưới thư mục history (mặc định `models/web-history` hoặc path bạn set)

## Gọi API

```bash
BASE=https://8000-<id>.instabox.tech

curl -N "$BASE/v1/audio/speech" \
  -H "Authorization: Bearer $VIENEU_API_KEY" \
  -H 'Content-Type: application/json' \
  -d '{"model":"vieneu-v3-nano","voice":"Mai Anh","input":"Xin chào","response_format":"wav","sample_rate":24000}' \
  -o out.wav
```

OpenAI SDK: `base_url="$BASE/v1"`, `model="vieneu-v3-nano"`.

## Chạy trên Orange Pi (local / LAN)

Xem hướng dẫn riêng: **[ORANGEPI.md](ORANGEPI.md)**

## Script hữu ích

| Script | Việc |
|---|---|
| `scripts/download_models.py` | Tải model về `models/` (cần internet 1 lần) |
| `scripts/run_lan_api.sh` | Chạy API offline/LAN trên Orange Pi |

## Lưu ý vận hành Instabox

- Free plan: idle pause ~30 phút, max-run 480 phút
- Sau `stop`, process API mất — cần start lại
- `suspend` giữ process/memory; public request có thể wake box đang idle-pause

## LICENSE
- Theo license mã nguồn gốc Apache License 2.0

## Credits
- Mã nguồn gốc https://github.com/pnnbao97/VieNeu-TTS
- Mã nguồn ban đầu https://github.com/thanhtantran/VieNeu-TTS

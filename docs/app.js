/* Offline, deterministic Base27 transport demo. NOT a CVNSS4.0 converter or TTS engine. */
(() => {
  'use strict';
  const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ ';
  const $ = (id) => document.getElementById(id);
  const toTrits = (text) => {
    const words = [];
    for (const ch of text) {
      const n = ALPHABET.indexOf(ch);
      if (n < 0) throw new Error('Ký tự ngoài bảng A–Z + SPACE: ' + JSON.stringify(ch) + '. Hãy chuyển đổi bằng CVNSS4.0 có kiểm chứng trước.');
      words.push(Math.floor(n / 9).toString() + Math.floor((n % 9) / 3).toString() + (n % 3).toString());
    }
    return words.join('');
  };
  const fromTrits = (trits) => {
    if (trits.length % 3 !== 0 || /[^012]/.test(trits)) throw new Error('Chuỗi trít phải chứa 0, 1, 2 và có độ dài bội số của 3.');
    let result = '';
    for (let i = 0; i < trits.length; i += 3) {
      const index = Number(trits[i]) * 9 + Number(trits[i + 1]) * 3 + Number(trits[i + 2]);
      result += ALPHABET[index];
    }
    return result;
  };
  const msg = (text, isError = false) => {
    $('lab-status').textContent = text;
    $('lab-status').classList.toggle('error', isError);
  };
  const run = () => {
    const source = $('source').value;
    try {
      const coded = toTrits(source);
      const decoded = fromTrits(coded);
      $('trits').textContent = coded ? coded.match(/.{3}/g).join(' ') : '—';
      $('decoded').textContent = decoded || '—';
      $('symbols').textContent = String(source.length);
      $('trit-count').textContent = String(coded.length);
      const ok = decoded === source;
      $('roundtrip').textContent = ok ? 'PASS' : 'FAIL';
      $('roundtrip').style.color = ok ? 'var(--green)' : 'var(--red)';
      msg(ok ? 'PASS · Exact ASCII round-trip. Không chứng minh Unicode/CVNSS4.0 hay chất lượng TTS.' : 'FAIL · Round-trip mismatch.', !ok);
    } catch (err) {
      $('trits').textContent = '—';
      $('decoded').textContent = '—';
      $('symbols').textContent = '0';
      $('trit-count').textContent = '0';
      $('roundtrip').textContent = 'ERROR';
      $('roundtrip').style.color = 'var(--red)';
      msg(err.message, true);
    }
  };
  const copyText = async (text, button) => {
    try {
      await navigator.clipboard.writeText(text);
      button.textContent = 'COPIED ✓';
      window.setTimeout(() => { button.textContent = button.id === 'copy-experiment' ? 'COPY COMMANDS' : 'COPY'; }, 1400);
    } catch {
      msg('Không thể sao chép tự động: hãy chọn và sao chép thủ công.', true);
    }
  };
  const experiments = {
    host: {
      title: 'EXP-01 // HOST BASELINE',
      note: 'Điều kiện PASS: WAV 24 kHz được tạo; ghi RTF, thời gian khởi động, CPU và dung lượng model. Không sử dụng kết quả host để suy luận tốc độ ESP32-S3.',
      code: 'git clone https://github.com/xulytiengviet/VieNeu-TTS-Lite-ESP32-S3.git\ncd VieNeu-TTS-Lite-ESP32-S3\nuv sync\nuv run python scripts/download_models.py --skip-moss\nuv run python scripts/download_models.py --offline-check\nuv run python examples/main_v3nano.py'
    },
    esp: {
      title: 'EXP-02 // ESP-IDF HYBRID',
      note: 'Dùng URL http://IP_ORANGE_PI:8000/v1/audio/speech trong LAN tin cậy; firmware tải đầy WAV trước khi phát. Chưa là mô hình VieNeu chạy offline trên ESP32-S3.',
      code: 'cd firmware/esp32s3_hybrid\nidf.py set-target esp32s3\nidf.py menuconfig   # Wi-Fi / LAN API / I2S pins\nidf.py build\nidf.py -p PORT flash monitor\n# Server trên Orange Pi: xem ORANGEPI.md\n# Đo: request ms, WAV bytes, heap, audio OK'
    },
    base27: {
      title: 'EXP-03 // BASE27 REFERENCE / TERNARY BUDGET',
      note: 'PASS yêu cầu round-trip dữ liệu A–Z + SPACE, test không hợp lệ, và số liệu kích thước trọng số được ghi chú là lý thuyết. CVNSS4.0 converter thật phải được kiểm thử riêng.',
      code: 'python tools/base27.py encode "XIN CHAO VIET NAM"\npython tools/base27.py decode 212022111222002021000112222210022011201222111000110
python -m unittest discover -s tests -p "test_base27_reference.py"\npython tools/ternary_budget.py --parameters 48000000\n# firmware parser tests (host GCC)\ncc -std=c11 -Wall -Wextra -Werror -Ifirmware/esp32s3_hybrid/main firmware/esp32s3_hybrid/main/wav_pcm.c firmware/esp32s3_hybrid/tests/test_wav_pcm.c -o /tmp/test_wav && /tmp/test_wav'
    }
  };
  const alphabet = $('alphabet');
  [...ALPHABET].forEach((ch, i) => {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'symbol';
    button.setAttribute('aria-label', 'Thêm ' + (ch === ' ' ? 'SPACE' : ch));
    const trits = Math.floor(i / 9).toString() + Math.floor(i % 9 / 3).toString() + (i % 3).toString();
    const main = document.createElement('strong');
    main.textContent = ch === ' ' ? '␠' : ch;
    const sub = document.createElement('small');
    sub.textContent = trits;
    button.append(main, sub);
    button.addEventListener('click', () => { $('source').value += ch; run(); $('source').focus(); });
    alphabet.append(button);
  });
  const updateBudget = () => {
    const m = Number($('parameters').value), count = m * 1e6;
    $('parameters-label').textContent = m.toFixed(1) + 'M';
    const values = {fp32: count * 4 / 1e6, int8: count / 1e6, ternary: count / 4e6, entropy: count * Math.log2(3) / 8e6};
    for (const [k, mb] of Object.entries(values)) {
      $(k + '-value').textContent = mb.toFixed(2) + ' MB';
      $(k + '-bar').style.width = Math.max(0.4, (mb / values.fp32) * 100).toFixed(2) + '%';
    }
  };
  document.querySelectorAll('.experiment-tab').forEach((tab) => tab.addEventListener('click', () => {
    document.querySelectorAll('.experiment-tab').forEach((t) => {
      const active = t === tab;
      t.classList.toggle('selected', active);
      t.setAttribute('aria-selected', String(active));
    });
    const e = experiments[tab.dataset.tab];
    $('experiment-title').textContent = e.title;
    $('experiment-text').textContent = e.code;
    $('experiment-text').setAttribute('aria-labelledby', tab.id);
    $('experiment-note').textContent = e.note;
  }));
  $('source').addEventListener('input', run);
  $('encode').addEventListener('click', run);
  $('sample').addEventListener('click', () => { $('source').value = 'XIN CHAO VIET NAM'; run(); });
  $('clear').addEventListener('click', () => { $('source').value = ''; run(); $('source').focus(); });
  $('copy').addEventListener('click', () => copyText($('trits').textContent.replace(/\s/g, ''), $('copy')));
  $('copy-experiment').addEventListener('click', () => copyText($('experiment-text').textContent, $('copy-experiment')));
  $('parameters').addEventListener('input', updateBudget);
  $('experiment-text').textContent = experiments.host.code;
  $('experiment-note').textContent = experiments.host.note;
  updateBudget();
  run();
})();
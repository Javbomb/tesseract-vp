"""Check an HDMI capture (tvp_cap.bin) against the film's own audio.

python -I iec_verify.py capture.bin source.thd|source.dts out_prefix
Unpacks the IEC 61937 bursts, rebuilds the elementary stream (out_prefix.thd/.dts) and
reports how many access units appear byte for byte in the source window.
"""
import subprocess
import sys

MAT_FRAME = 61424
MAT_CODES = ((0, 20), (30708, 12), (MAT_FRAME - 16, 16))


def bursts(raw):
    i, n = 0, len(raw)
    while i + 8 <= n:
        j = raw.find(b"\x72\xf8\x1f\x4e", i)
        if j < 0 or j + 8 > n:
            return
        if j & 1:
            i = j + 1
            continue
        pc = raw[j + 4] | raw[j + 5] << 8
        pd = raw[j + 6] | raw[j + 7] << 8
        kind = pc & 0x7F
        size = MAT_FRAME if kind == 0x16 else pd if kind in (0x11, 0x15) else (pd + 7) // 8
        body = raw[j + 8:j + 8 + size + (size & 1)]
        if len(body) < size:
            return
        sw = bytearray(len(body))
        sw[0::2], sw[1::2] = body[1::2], body[0::2]
        yield kind, bytes(sw[:size])
        i = j + 8 + size


def mat_to_truehd(frames):
    data = bytearray()
    for f in frames:
        keep, pos = bytearray(), 0
        for at, ln in MAT_CODES:
            keep += f[pos:at]
            pos = at + ln
        data += keep + f[pos:]
    aus, pos = [], 0
    while pos + 4 <= len(data):
        if data[pos] == 0 and data[pos + 1] == 0:
            pos += 2
            continue
        ln = ((data[pos] & 0x0F) << 8 | data[pos + 1]) * 2
        if ln < 4 or pos + ln > len(data):
            break
        aus.append(bytes(data[pos:pos + ln]))
        pos += ln
    return aus


def major_sync(au):
    k = au.find(b"\xf8\x72\x6f\xba")
    if k < 0 or k + 18 > len(au):
        return None
    return {"substreams": au[k + 16] >> 4, "ext_substream_info": au[k + 16] & 3, "substream_info": au[k + 17]}


def main():
    cap, src, out = sys.argv[1:4]
    raw = open(cap, "rb").read()
    ref = open(src, "rb").read()
    kinds = {}
    for kind, body in bursts(raw):
        kinds.setdefault(kind, []).append(body)
    print("bursts by type:", {hex(k): len(v) for k, v in kinds.items()})
    if 0x16 in kinds:
        units, ext = mat_to_truehd(kinds[0x16]), ".thd"
        syncs = [s for s in (major_sync(u) for u in units) if s]
        print("TrueHD major syncs in capture:", len(syncs), "first:", syncs[0] if syncs else None)
        k = ref.find(b"\xf8\x72\x6f\xba")
        print("TrueHD major sync in source:  ", major_sync(ref[k - 4:k + 40]) if k >= 4 else None)
    elif 0x11 in kinds:
        units = [b[12:12 + (b[10] << 8 | b[11])] for b in kinds[0x11]]
        ext = ".dts"
    else:
        print("no MAT or DTS-HD bursts")
        return 1
    body = units[1:-1]
    hits = sum(1 for u in body if ref.find(u) >= 0)
    nbytes = sum(len(u) for u in body)
    print(f"access units: {len(units)} (checked {len(body)}, {nbytes} bytes); found byte-exact in source: {hits}")
    open(out + ext, "wb").write(b"".join(units))
    probe = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=codec_name,profile,channels",
                            "-of", "compact", out + ext], capture_output=True, text=True)
    print("ffprobe of rebuilt stream:", probe.stdout.strip() or probe.stderr.strip()[-200:])
    return 0 if body and hits == len(body) else 2


if __name__ == "__main__":
    sys.exit(main())

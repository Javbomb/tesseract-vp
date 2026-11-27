/*
 * Tesseract VP: code shared by the control page (/) and the projector overlay (/tv).
 * Served as /scope.js. Copyright (c) 2026 Broken Light. All rights reserved.
 */
'use strict';
const CVP = (() => {
  const M1 = 2610 / 16384, M2 = 2523 / 4096 * 128, C1 = 3424 / 4096, C2 = 2413 / 4096 * 32, C3 = 2392 / 4096 * 32;
  const pq = (n) => { if (n <= 0) return 0; const y = Math.pow(n / 10000, M1); return Math.pow((C1 + C2 * y) / (1 + C3 * y), M2); };
  const ipq = (e) => { if (e <= 0) return 0; const p = Math.pow(e, 1 / M2); return 10000 * Math.pow(Math.max(p - C1, 0) / (C2 - C3 * p), 1 / M1); };

  /* the curve, ported line for line from core/dtm_core.c */
  const kneeNits = (punch) => 3.5 * Math.pow(2, (punch - 5) / 2);
  function mapToTarget(p, srcPeak, tgt, nits) {
    if (srcPeak <= tgt) return nits < tgt ? nits : tgt;
    const srcPq = pq(srcPeak);
    let e = Math.min(1, pq(nits) / srcPq);
    const m = pq(tgt) / srcPq;
    let ks;
    if (p.curve < 0.5) ks = m - 0.5 * (1 - m);
    else { ks = pq(kneeNits(p.punch)) / srcPq; if (ks > m - 0.02) ks = m - 0.02; }
    if (ks < 0) ks = 0;
    if (e >= ks) {
      const t = (e - ks) / (1 - ks);
      if (p.curve < 0.5) {
        const t2 = t * t, t3 = t2 * t;
        e = (2 * t3 - 3 * t2 + 1) * ks + (t3 - 2 * t2 + t) * (1 - ks) + (-2 * t3 + 3 * t2) * m;
      } else {
        const a = (1 - ks) / (m - ks);
        const k = 1 - 0.5 * Math.min(1, p.top_shape || 0);
        let h = a * t / (1 + (a - 1) * Math.pow(t, k));
        if (p.hl_lift > 0) { const c = Math.min(1, p.hl_lift); let h6 = h * h * h; h6 *= h6; h += c * h6 * (1 - h); }
        e = ks + (m - ks) * h;
      }
    }
    const out = ipq(e * srcPq);
    return out < tgt ? out : tgt;
  }
  function curveNits(p, srcPeak, target, nits) {
    const dpl = p.target_nits;
    if (target < dpl) target = dpl;
    let ratio = dpl / target;
    if (p.shadow_keep > 0 && ratio < 1 && nits > 0) {
      const o = Math.log2(nits / p.shadow_keep) / 3;
      const w = o <= 0 ? 1 : o >= 1 ? 0 : 1 - o * o * (3 - 2 * o);
      ratio += (1 - ratio) * w;
    }
    let out = mapToTarget(p, srcPeak, target, nits) * ratio;
    const pivot = 0.2 * dpl, k = target / dpl - 1;
    if (p.pop && out > 0.01 * dpl && out < dpl && srcPeak > target) {
      const piv = 0.15 * dpl, x = Math.log2(out / piv), e = Math.log2((x > 0 ? dpl : 0.01 * dpl) / piv), t = x / e;
      out = piv * Math.pow(2, x + p.pop * Math.min(1, Math.log2(srcPeak / target)) * e * t * (1 - t) * (1 - t));
    }
    if (p.shadow_depth > 0 && out > 0 && out < pivot && k > 0) out *= Math.pow(out / pivot, p.shadow_depth * Math.min(1, k));
    if (p.deep_lift > 0 && out > 0 && out < 2) { const o = Math.log2(out / 0.25) / 3, w = o <= 0 ? 1 : 1 - o * o * (3 - 2 * o); out *= Math.pow(2, p.deep_lift * w); }
    return out;
  }
  /* FALL (black excluded) and the average level above the display peak, from the 128-bin PQ histogram. */
  function frameLevels(p, hist) {
    let n = 0, sum = 0, hn = 0, hsum = 0;
    const N = hist.length;
    for (let i = 1; i < N; i++) {
      const v = hist[i]; if (!v) continue;
      const nits = ipq(i / (N - 1));
      n += v;
      sum += v * (p.fall_hl_mod > 0 && nits > p.target_nits ? nits - p.fall_hl_mod * (nits - p.target_nits) : nits);
      if (nits > p.target_nits) { hn += v; hsum += v * nits; }
    }
    return { fall: n > 0 ? sum / n : 0, avgHl: hn > 0 ? hsum / hn : 0 };
  }
  /* The scene target the core wants for this frame (before it moves there at the set speed). */
  function targetRaw(p, peak, hist) {
    const { fall, avgHl } = frameLevels(p, hist);
    let t = p.target_nits;
    if (p.dyn_tuning > 0) {
      t = (p.add_peak ? p.target_nits : 0) + 2 * fall * (1 + p.dyn_tuning / 50);
      if (p.avghl_ceiling > 0 && avgHl > 0 && t > p.avghl_ceiling * avgHl) t = p.avghl_ceiling * avgHl;
      if (p.min_ratio > 1) { const floor = p.min_ratio * p.target_nits * Math.pow(peak / 1000, p.floor_exp); if (t < floor) t = floor; }
      if (t > peak) t = peak;
      if (t < p.target_nits) t = p.target_nits;
    }
    return t;
  }
  /* The hardware's x grid (149 points, nits). */
  const GRID = (() => {
    const g = [0];
    for (let i = 1; i < 8; i++) g.push(10000 * Math.pow(2, i - 1 + 4) / 4294967296);
    for (let j = 11; j < 20; j++) for (let k = 0; k < 4; k++) g.push(10000 * (Math.pow(2, j - 2) * k + Math.pow(2, j)) / 4294967296);
    for (let j = 20; j < 31; j++) for (let k = 0; k < 8; k++) g.push(10000 * (Math.pow(2, j - 3) * k + Math.pow(2, j)) / 4294967296);
    while (g.length < 148) g.push(10000 * (Math.pow(2, 27) * (g.length - 140) + Math.pow(2, 31)) / 4294967296);
    g.push(10000);
    return g;
  })();
  /* (1 - e^-x) x 255 for x = i / 128, i < 2048 (x < 16, past which it is 255) */
  const GLOW = new Float32Array(2048).map((_, i) => (1 - Math.exp(-i / 128)) * 255);

  /* the peak the output is tone mapped to: HDR10 out has its own, unless the display can't take PQ (info output.pq_sink) */
  let pqSink = true;
  const setPqSink = (v) => { pqSink = v !== 0 && v !== false; };
  const peakKey = (p) => (+p.out_hdr === 1 && pqSink ? 'target_nits_hdr' : 'target_nits');
  const peakNits = (p) => +p[peakKey(p)] || +p.target_nits || 100;
  /* as dtm_apply_simple in core/dtm_core.c, with the brain's peak for the output */
  function effective(q) {
    const p = Object.assign({}, q), c = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
    p.target_nits = peakNits(q);
    const b = c(p.bright_adj || 0, -5, 5), h = c(p.hl_adj || 0, -5, 5), s = c(p.shadow_adj || 0, -5, 5);
    const cc = c(p.colour_adj || 0, -5, 5), d = c(p.depth_adj || 0, 0, 5), tw = (x, u, top, keep) => (u < 0 ? x + (top - x) * -u / 5 : x * (1 - (1 - keep) * u / 5));
    p.hl_desat = c(tw(p.hl_desat || 0, cc, 1, 0), 0, 2); p.cg_share = c(tw(p.cg_share || 0, cc, 1.5, 0.5), 0, 1.5); p.cg_topmax = c(tw(p.cg_topmax || 0, cc, 1, 0.5), 0, 1);
    p.contrast = c((p.contrast || 0) + 0.06 * d, 0, 1); p.shadow_sep = c((p.shadow_sep || 0) + 0.1 * d, 0, 1);
    p.scene_bright = c((p.scene_bright || 0) + 0.12 * b, -2, 2);
    const f = Math.pow(2, -p.scene_bright);
    if (p.dyn_tuning > 0) p.dyn_tuning = c(50 * ((1 + p.dyn_tuning / 50) * f - 1), 0.5, 100);
    if (p.min_ratio > 1) p.min_ratio = c(p.min_ratio * f, 1, 4);
    if (p.avghl_ceiling > 0) p.avghl_ceiling = c(p.avghl_ceiling * f, 0.5, 6);
    p.punch = c(p.punch + 0.3 * b, 0, 10);
    const u = c(p.punch_adj || 0, -5, 5);
    p.punch = c(p.punch + 0.7 * u, 0, 10); p.pop = c((p.pop || 0) + 0.06 * u, -0.3, 0.3);
    if (h > 0) { p.hl_lift *= 1 - 0.15 * h; p.fall_hl_mod *= 1 - h / 15; }
    else { p.hl_lift += (1 - p.hl_lift) * (-h / 5); p.fall_hl_mod += (1 - p.fall_hl_mod) * (-h / 5); }
    p.hl_lift = c(p.hl_lift, 0, 1); p.fall_hl_mod = c(p.fall_hl_mod, 0, 1);
    p.hl_desat = c(h > 0 ? (p.hl_desat || 0) + 0.15 * h : (p.hl_desat || 0) * (1 + h / 5), 0, 2);
    if (s > 0) { p.shadow_keep = c(p.shadow_keep + 3 * Math.pow(s / 5, 1.5), 0, 50); p.shadow_depth = c(p.shadow_depth - 0.03 * s, 0, 0.5); }
    else p.shadow_depth = c(p.shadow_depth - 0.04 * s, 0, 0.5);
    p.bright_adj = p.hl_adj = p.shadow_adj = p.punch_adj = p.colour_adj = p.depth_adj = 0;
    return p;
  }
  /* what the Cube adds on top of the core's maths is measured as the gap between its curve and ours */
  function liveCurve(p0, s0) {
    const p = effective(p0), s = Object.assign({}, s0, { params: effective(s0.params) });
    const sp = s.params, peak = s.curve_peak > 0 ? s.curve_peak : p.target_nits;
    const tSrv = Math.max(sp.target_nits, s.scene_target || sp.target_nits);
    const rawSrv = targetRaw(sp, peak, s.hist), rawNew = targetRaw(p, peak, s.hist);
    let T = rawSrv > 0 ? tSrv * rawNew / rawSrv : rawNew;
    T = Math.max(p.target_nits, Math.min(T, Math.max(peak, p.target_nits)));
    if (!(p.dyn_tuning > 0)) T = p.target_nits;
    const pts = [];
    const srv = s.curve || [];
    for (let i = 0; i < srv.length; i++) {
      const x = srv[i][0];
      const mine = curveNits(sp, peak, tSrv, x);
      const corr = srv[i][1] > 0 && mine > 0 ? pq(srv[i][1]) - pq(mine) : 0;
      const out = curveNits(p, peak, T, x);
      pts.push([x, Math.min(p.target_nits, ipq(Math.max(0, pq(out) + corr)))]);
    }
    if (!pts.length) for (let i = 1; i < GRID.length; i++) pts.push([GRID[i], curveNits(p, peak, T, GRID[i])]);
    return { pts, T, peak, knee: kneeNits(p.punch) };
  }

  const TRACE = [[1, 0.16, 0.12], [0.12, 1, 0.2], [0.18, 0.36, 1]];
  const MONO = [0.86, 0.9, 0.93];
  const PINK = [1, 0.24, 0.8], BLUE = [0.31, 0.56, 1];
  const BG = [11, 13, 14];
  const FONT = 'ui-monospace, "Red Hat Mono", "Cascadia Mono", Consolas, Menlo, monospace';

  /* Rows (0 = top of the plot) where each 8-bit code starts and ends on a scale. */
  function codeEdges(scale, ph, dpl) {
    const top = new Float64Array(256), bot = new Float64Array(256);
    if (scale === 'nits') {
      const tp = pq(dpl), row = (n) => ph * (1 - Math.min(1, pq(n) / tp));
      for (let k = 0; k < 256; k++) {
        const lo = k ? dpl * Math.pow((k - 0.5) / 255, 2.2) : 0, hi = dpl * Math.pow(Math.min(255, k + 0.5) / 255, 2.2);
        top[k] = k === 255 ? 0 : row(hi); bot[k] = row(lo);
      }
    } else {
      const step = ph / 256;
      for (let k = 0; k < 256; k++) { top[k] = (255 - k) * step; bot[k] = top[k] + step; }
    }
    return { top, bot };
  }
  const bandCache = new Map();
  function bandsFor(scale, ph, dpl) {
    const key = scale + ph + ':' + (scale === 'nits' ? dpl : 0);
    let b = bandCache.get(key);
    if (b) return b;
    const e = codeEdges(scale, ph, dpl);
    b = { top: e.top, bot: e.bot, rows: [] };
    for (let k = 0; k < 256; k++) b.rows.push(bandRows(e.top[k], e.bot[k], ph));
    if (bandCache.size > 24) bandCache.clear();
    bandCache.set(key, b);
    return b;
  }
  /* each row a code touches gets its area share, scaled to the code's rows on a straight scale */
  function bandRows(a0, a1, ph) {
    a1 = Math.max(a1, a0 + 1e-6);
    const f = ph / 256 / (a1 - a0), rows = [];
    for (let r = Math.floor(a0); r < Math.min(ph, Math.ceil(a1)); r++) {
      const w = Math.min(a1, r + 1) - Math.max(a0, r);
      if (w > 0) rows.push(r, w * f);
    }
    return rows;
  }
  /* the capture's 8-bit conversion fills codes unevenly (stripes): heights follow the local average count */
  const SMOOTH = 3, MIN_AVG = 400, DRIFT = 1.5;
  function evenCodes(B, tot, ph) {
    const e = new Float64Array(257);
    let moved = false;
    for (let k = 0; k < 256; k++) {
      let s = 0, n = 0;
      for (let j = Math.max(0, k - SMOOTH); j <= Math.min(255, k + SMOOTH); j++) { s += tot[j]; n++; }
      const avg = s / n, w = avg >= MIN_AVG && k > 0 && k < 255 ? tot[k] / avg : 1;
      let x = Math.min(k + 1 + DRIFT, Math.max(k + 1 - DRIFT, e[k] + w));
      if (x < e[k]) x = e[k];
      e[k + 1] = x;
      if (Math.abs(x - (k + 1)) > 1e-3) moved = true;
    }
    if (!moved) return B.rows;
    /* code units to rows: unit x in [j, j + 1] runs up code j's band */
    const row = (x) => { const j = Math.min(255, Math.max(0, Math.floor(x))), f = Math.min(1, x - j); return B.bot[j] + f * (B.top[j] - B.bot[j]); };
    const rows = new Array(256);
    for (let k = 0; k < 256; k++) rows[k] = bandRows(row(e[k + 1]), row(e[k]), ph);
    return rows;
  }

  class Wave {
    constructor(canvas) { this.cv = canvas; this.data = null; this.frame = 0; this.work = document.createElement('canvas'); this.acc = null; }
    /* buf from /api/wavehist */
    setData(buf) {
      const h = new Uint32Array(buf, 0, 6);
      const cols = h[0], planes = h[1], frame = h[2] + h[3] * 4294967296, hasCol = h[4];
      this.frame = frame;
      this.source = h[5];	/* 1 video layer, 2 whole screen */
      if (!frame) { this.data = null; return; }
      this.data = { cols, planes, counts: new Uint16Array(buf, 24, planes * 256 * cols),
        colour: hasCol ? new Uint8Array(buf, 24 + planes * 256 * cols * 2, 256 * cols * 3) : null };
      /* per-code totals for evenCodes; the share at code 255 turns the clip line red */
      const tot = this.data.tot = new Float64Array(planes * 256), counts = this.data.counts;
      let share = 0;
      for (let c = 0; c < planes; c++) {
        let all = 0;
        for (let k = 0; k < 256; k++) {
          let s = 0;
          for (let i = (c * 256 + k) * cols, e = i + cols; i < e; i++) s += counts[i];
          tot[c * 256 + k] = s; all += s;
        }
        if (all) share = Math.max(share, tot[c * 256 + 255] / all);
      }
      this.clipShare = share;
    }
    /* o: { mode: 'rgb'|'parade'|'max'|'luma', scale: 'nits'|'ire'|'code', gain, colour, clip, dpl, maxDpr } */
    paint(o) {
      /* o.dpr: device pixels per CSS pixel (the overlay passes its stage scale in too) */
      const cv = this.cv, dpr = o.dpr || Math.min(o.maxDpr || 2, window.devicePixelRatio || 1);
      const cw = Math.max(2, Math.round(cv.clientWidth * dpr)), ch = Math.max(2, Math.round(cv.clientHeight * dpr));
      if (cv.width !== cw || cv.height !== ch) { cv.width = cw; cv.height = ch; }
      const g = cv.getContext('2d');
      /* room above the top line for its label (labels sit above their lines) */
      const fs = o.fontPx ? Math.round(o.fontPx * dpr) : Math.round(Math.max(10.5 * dpr, Math.min(15 * dpr, ch / 34)));
      const top = Math.max(Math.round(ch * 0.045), fs + Math.round(5 * dpr)), bottom = Math.round(ch * 0.06), ph = ch - top - bottom;
      g.fillStyle = `rgb(${BG})`; g.fillRect(0, 0, cw, ch);
      const d = this.data;
      if (d && ph > 4) this.trace(g, o, cw, ch, top, ph);   /* a canvas too small to draw in (mid-resize) */
      this.graticule(g, o, cw, ch, top, ph, dpr);
    }
    trace(g, o, cw, ch, top, ph) {
      const d = this.data, cols = d.cols, parade = o.mode === 'parade';
      const planes = d.planes, bw = Math.min(cols, cw);
      const sub = parade ? Math.floor(bw / 3) : bw;          /* width each plane is drawn into */
      const merged = cols / sub;                               /* capture columns per drawn column */
      const colOf = new Int32Array(cols);
      for (let x = 0; x < cols; x++) colOf[x] = Math.min(sub - 1, Math.floor(x * sub / cols));
      const B = bandsFor(o.scale, ph, o.dpl);
      const n = ph * bw;
      if (!this.acc || this.acc.length !== n * 5) this.acc = new Float32Array(n * 5);
      const acc = this.acc; acc.fill(0);
      /* planes 0..2 in acc[0..3n) (one-plane modes use plane 0), clip highs at 3n, crushed at 4n */
      const useCol = !parade && planes === 1 && o.colour && d.colour;
      let cacc = null;
      if (useCol) { if (!this.cacc || this.cacc.length !== n * 3) this.cacc = new Float32Array(n * 3); cacc = this.cacc; cacc.fill(0); }
      for (let c = 0; c < planes; c++) {
        const xoff = parade ? c * sub : 0, plane = (planes === 3 ? c : 0) * n;
        const rowsOf = evenCodes(B, d.tot.subarray(c * 256, c * 256 + 256), ph);
        for (let k = 0; k < 256; k++) {
          const band = rowsOf[k], base = (c * 256 + k) * cols;
          /* with the clipping view on, codes 255 and 0 go to their own layers (pink, blue) */
          const dst = o.clip && k === 255 ? 3 * n : o.clip && k === 0 ? 4 * n : plane;
          const tint = cacc && dst === plane;
          for (let x = 0; x < cols; x++) {
            const cnt = d.counts[base + x]; if (!cnt) continue;
            const xx = xoff + colOf[x];
            for (let j = 0; j < band.length; j += 2) {
              const idx = band[j] * bw + xx, v = cnt * band[j + 1];
              acc[dst + idx] += v;
              if (tint) { const ci = (k * cols + x) * 3, q = idx * 3; cacc[q] += v * d.colour[ci]; cacc[q + 1] += v * d.colour[ci + 1]; cacc[q + 2] += v * d.colour[ci + 2]; }
            }
          }
        }
      }
      const kk = (0.077 * ph / 256) * Math.pow(2, o.gain) / merged;
      /* from a table: a Math.exp per pixel and plane is too slow on the Fire TV */
      const glow = (v) => { const x = v * kk * 128; return x >= 2047 ? 255 : GLOW[x | 0]; };
      const img = g.createImageData(bw, ph), p = img.data;
      const nP = planes;
      for (let i = 0; i < n; i++) {
        let R = BG[0], G = BG[1], Bl = BG[2];
        for (let c = 0; c < nP; c++) {
          const v = acc[c * n + i]; if (!v) continue;
          const l = glow(v);
          let t;
          if (planes === 3) t = TRACE[c];
          else if (cacc) { const q = i * 3, m = Math.max(cacc[q], cacc[q + 1], cacc[q + 2]) || 1; t = [cacc[q] / m, cacc[q + 1] / m, cacc[q + 2] / m]; }
          else t = MONO;
          R += l * t[0]; G += l * t[1]; Bl += l * t[2];
        }
        const hi = acc[3 * n + i], lo = acc[4 * n + i];
        if (hi) { const l = glow(hi); R += l * PINK[0]; G += l * PINK[1]; Bl += l * PINK[2]; }
        if (lo) { const l = glow(lo); R += l * BLUE[0]; G += l * BLUE[1]; Bl += l * BLUE[2]; }
        const q = i * 4; p[q] = R; p[q + 1] = G; p[q + 2] = Bl; p[q + 3] = 255;
      }
      const w = this.work;
      if (w.width !== bw || w.height !== ph) { w.width = bw; w.height = ph; }
      w.getContext('2d').putImageData(img, 0, 0);
      g.imageSmoothingEnabled = false;
      g.drawImage(w, 0, 0, bw, ph, 0, top, cw, ph);   /* columns repeat, never blur */
    }
    graticule(g, o, cw, ch, top, ph, dpr) {
      const fs = o.fontPx ? Math.round(o.fontPx * dpr) : Math.round(Math.max(10.5 * dpr, Math.min(15 * dpr, ch / 34)));
      g.font = `400 ${fs}px ${FONT}`; g.textBaseline = 'bottom'; g.lineWidth = 1;
      const dpl = o.dpl, codeY = (code) => top + (1 - code / 255) * ph;      /* code's centre-ish on linear scales */
      const nitsY = (n) => top + ph * (1 - Math.min(1, pq(n) / pq(dpl)));
      const lineAt = (y, style) => { const yy = Math.round(y) + 0.5; g.strokeStyle = style; g.beginPath(); g.moveTo(0, yy); g.lineTo(cw, yy); g.stroke(); return yy; };
      const lab = (text, x, y, align, col) => { g.fillStyle = col || 'rgba(225,200,140,0.9)'; g.textAlign = align; g.fillText(text, x, y - 3 * dpr); };
      const fine = 'rgba(200,170,90,0.17)', main = 'rgba(200,170,90,0.4)', topc = 'rgba(240,190,90,0.75)';
      const pad = 6 * dpr;
      /* red while the picture is clipping (over 0.005 % at code 255, ~25 capture pixels), plain otherwise */
      const clipping = !!this.data && this.clipShare > 5e-5, clipCol = clipping ? 'rgba(255,110,100,0.98)' : topc;
      if (o.scale === 'code') {
        for (const v of [0, 128, 256, 384, 512, 640, 768, 896, 1023]) {
          const y = lineAt(codeY(v * 255 / 1023), v === 1023 ? topc : v % 256 ? fine : main);
          lab(v === 1023 ? '1023 · clip' : String(v), pad, y, 'left', v === 1023 ? clipCol : undefined);
        }
      } else if (o.scale === 'ire') {
        for (let v = 0; v <= 100; v += 20) {
          const y = lineAt(codeY(v * 2.55), v === 100 ? topc : v === 0 ? main : fine);
          lab(v === 100 ? '100 IRE · clip' : String(v), pad, y, 'left', v === 100 ? clipCol : undefined);
        }
      } else {
        /* nits scale: grid lines at real nits, labelled on the right; % of your peak on the left */
        const ticks = [0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100, 200, 400].filter((n) => n < dpl * 0.86);
        let lastY = 1e9;
        const peakY = nitsY(dpl);
        lineAt(top + ph, main); lab('0', cw - pad, top + ph, 'right');
        for (const n of ticks) {
          const y = nitsY(n);
          /* a short plot labels fewer lines */
          if (lastY - y < fs * (ph < 300 * dpr ? 2 : 1.25) || y > top + ph - fs * 1.2 || y - peakY < fs * 1.4) { continue; }
          const yy = lineAt(y, n === 1 || n === 10 || n === 100 ? main : fine);
          lab(n >= 1 ? String(n) : String(n).replace(/^0/, ''), cw - pad, yy, 'right'); lastY = y;
        }
        const yy = lineAt(nitsY(dpl), clipping ? 'rgba(255,96,90,0.75)' : topc);
        lab(`${Math.round(dpl)} nits`, cw - pad, yy, 'right', clipping ? 'rgba(255,150,140,0.95)' : undefined);
      }
      /* second scale: % of the display peak on the left of the nits scale, nits on the right of the signal scales */
      const tick = Math.max(40 * dpr, fs * 3);
      const mark = (y, text, left, col) => { const yy = Math.round(y) + 0.5; g.strokeStyle = 'rgba(200,170,90,0.5)'; g.beginPath();
        g.moveTo(left ? 0 : cw - tick, yy); g.lineTo(left ? tick : cw, yy); g.stroke(); lab(text, left ? pad : cw - pad, yy, left ? 'left' : 'right', col || 'rgba(225,200,140,0.85)'); };
      const right = (y, text) => mark(y, text, false);
      if (o.scale === 'nits') {
        for (const pc of [1, 10, 25, 50]) { const y = nitsY(dpl * pc / 100); if (y > top + fs * 1.5) mark(y, `${pc}%`, true); }
        mark(nitsY(dpl), '100% · clip', true, clipCol);
      } else {
        let lastY = 1e9;
        for (const n of [0.1, 1, 5, 10, 25, 50, 100, 200, 400].filter((n) => n < dpl * 0.9).concat([dpl])) {
          const y = codeY(255 * Math.pow(n / dpl, 1 / 2.2));
          if (lastY - y < fs * 1.25 && n !== dpl) continue;
          right(y, n === dpl ? `${Math.round(dpl)} nits` : `${n} nits`); lastY = y;
        }
      }
      /* anything on the top line is clipped; a faint band from 95 % shows how close the picture runs */
      {
        const y95 = o.scale === 'nits' ? nitsY(dpl * Math.pow(0.95, 2.2)) : codeY(255 * 0.95), yf = Math.round(top) + 0.5;
        g.fillStyle = clipping ? 'rgba(255,90,80,0.12)' : 'rgba(240,190,90,0.05)'; g.fillRect(0, yf, cw, Math.max(1, y95 - yf));
        g.strokeStyle = clipping ? 'rgba(255,90,80,0.95)' : topc; g.lineWidth = clipping ? Math.max(1, Math.round(2.5 * dpr)) : 1;
        g.beginPath(); g.moveTo(0, yf); g.lineTo(cw, yf); g.stroke(); g.lineWidth = 1;
      }
      /* parade dividers and bottom ticks */
      g.strokeStyle = 'rgba(200,170,90,0.28)';
      if (o.mode === 'parade') for (const f of [1 / 3, 2 / 3]) { const x = Math.round(cw * f) + 0.5; g.beginPath(); g.moveTo(x, top - 4 * dpr); g.lineTo(x, top + ph); g.stroke(); }
      const by = Math.round(top + ph + 6 * dpr) + 0.5;
      g.beginPath(); g.moveTo(0, by); g.lineTo(cw, by);
      for (let i = 0; i <= 40; i++) { const x = Math.round(cw * i / 40) + 0.5, len = (i % 4 ? 3 : 7) * dpr; g.moveTo(x, by); g.lineTo(x, by + len); }
      g.stroke();
    }
  }

  const LOCUS = [[0.1741, 0.005], [0.1738, 0.0049], [0.1733, 0.0048], [0.1726, 0.0048], [0.1714, 0.0051], [0.1689, 0.0069], [0.1644, 0.0109],
    [0.1566, 0.0177], [0.144, 0.0297], [0.1241, 0.0578], [0.1096, 0.0868], [0.0913, 0.1327], [0.0687, 0.2007], [0.0454, 0.295], [0.0235, 0.4127],
    [0.0082, 0.5384], [0.0039, 0.6548], [0.0139, 0.7502], [0.0389, 0.812], [0.0743, 0.8338], [0.1142, 0.8262], [0.1547, 0.8059], [0.1929, 0.7816],
    [0.2296, 0.7543], [0.2658, 0.7243], [0.3016, 0.6923], [0.3373, 0.6589], [0.3731, 0.6245], [0.4087, 0.5896], [0.4441, 0.5547], [0.4788, 0.5202],
    [0.5125, 0.4866], [0.5448, 0.4544], [0.5752, 0.4242], [0.6029, 0.3965], [0.627, 0.3725], [0.6658, 0.334], [0.6915, 0.3083], [0.7079, 0.292],
    [0.719, 0.2809], [0.726, 0.274], [0.73, 0.27], [0.7334, 0.2666], [0.7347, 0.2653]];
  const GAMUTS = { '709': [[0.64, 0.33], [0.3, 0.6], [0.15, 0.06]], P3: [[0.68, 0.32], [0.265, 0.69], [0.15, 0.06]], '2020': [[0.708, 0.292], [0.17, 0.797], [0.131, 0.046]] };
  /* xy -> a display colour for that chromaticity (sRGB, brightest channel 1) */
  function xyColour(x, y) {
    const X = x / y, Z = (1 - x - y) / y;
    let r = 3.2406 * X - 1.5372 - 0.4986 * Z, gg = -0.9689 * X + 1.8758 + 0.0415 * Z, b = 0.0557 * X - 0.204 + 1.057 * Z;
    const mn = Math.min(r, gg, b); if (mn < 0) { r -= mn; gg -= mn; b -= mn; }
    const mx = Math.max(r, gg, b) || 1;
    return [r / mx, gg / mx, b / mx].map((v) => Math.pow(v, 1 / 2.2));
  }
  function inLocus(x, y) {
    let inside = false;
    for (let i = 0, j = LOCUS.length - 1; i < LOCUS.length; j = i++) {
      const [xi, yi] = LOCUS[i], [xj, yj] = LOCUS[j];
      if ((yi > y) !== (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
    }
    return inside;
  }
  function sizeCanvas(cv, maxDpr) {
    const dpr = Math.min(maxDpr || 2, window.devicePixelRatio || 1);
    const w = Math.max(2, Math.round(cv.clientWidth * dpr)), h = Math.max(2, Math.round(cv.clientHeight * dpr));
    if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
    return [cv.getContext('2d'), w, h, dpr];
  }
  let locusFill = null;
  /* data: { cieW, cieH, cie: Uint16Array } */
  function drawCie(cv, data, o) {
    const [g, w, h, dpr] = sizeCanvas(cv, o.maxDpr);
    g.fillStyle = `rgb(${BG})`; g.fillRect(0, 0, w, h);
    const pad = 10 * dpr, s = Math.min((w - 2 * pad) / 0.8, (h - 2 * pad) / 0.9);
    const ox = (w - 0.8 * s) / 2, oy = h - (h - 0.9 * s) / 2;
    const X = (x) => ox + x * s, Y = (y) => oy - y * s;
    /* faint horseshoe of colour */
    if (!locusFill || locusFill.w !== w || locusFill.h !== h) {
      const c = document.createElement('canvas'); c.width = w; c.height = h;
      const cg = c.getContext('2d'), img = cg.createImageData(w, h), p = img.data, step = 2;
      for (let py = 0; py < h; py += step) for (let px = 0; px < w; px += step) {
        const x = (px - ox) / s, y = (oy - py) / s;
        if (y <= 0.002 || !inLocus(x, y)) continue;
        const col = xyColour(x, y);
        for (let dy = 0; dy < step; dy++) for (let dx = 0; dx < step; dx++) {
          const q = ((py + dy) * w + px + dx) * 4; if (q >= p.length) continue;
          p[q] = col[0] * 255; p[q + 1] = col[1] * 255; p[q + 2] = col[2] * 255; p[q + 3] = 30;
        }
      }
      cg.putImageData(img, 0, 0); locusFill = { w, h, c };
    }
    g.drawImage(locusFill.c, 0, 0);
    g.lineWidth = 1 * dpr; g.strokeStyle = 'rgba(200,170,90,0.45)'; g.beginPath();
    LOCUS.forEach(([x, y], i) => (i ? g.lineTo(X(x), Y(y)) : g.moveTo(X(x), Y(y)))); g.closePath(); g.stroke();
    /* the picture's colours */
    if (data && data.cie) {
      const cs = s * 0.8 / data.cieW, k = 0.35 * Math.pow(2, o.gain || 0);
      for (let j = 0; j < data.cieH; j++) for (let i = 0; i < data.cieW; i++) {
        const n = data.cie[j * data.cieW + i]; if (!n) continue;
        const x = (i + 0.5) / data.cieW * 0.8, y = (j + 0.5) / data.cieH * 0.9, col = xyColour(x, y), l = 0.25 + 0.75 * (1 - Math.exp(-n * k));
        g.fillStyle = `rgb(${Math.round(col[0] * 255 * l + 40 * l)},${Math.round(col[1] * 255 * l + 40 * l)},${Math.round(col[2] * 255 * l + 40 * l)})`;
        g.fillRect(X(x) - cs / 2, Y(y) - cs / 2, Math.ceil(cs), Math.ceil(cs));
      }
    }
    const tri = (pts, col, dash, lw) => { g.setLineDash(dash.map((v) => v * dpr)); g.lineWidth = lw * dpr; g.strokeStyle = col; g.beginPath(); pts.forEach(([x, y], i) => (i ? g.lineTo(X(x), Y(y)) : g.moveTo(X(x), Y(y)))); g.closePath(); g.stroke(); };
    tri(GAMUTS['709'], 'rgba(150,160,170,0.7)', [2, 3], 1);
    tri(GAMUTS.P3, 'rgba(200,205,212,0.75)', [5, 3], 1);
    tri(GAMUTS['2020'], 'rgba(235,238,242,0.85)', [], 1.2);
    g.setLineDash([]);
    g.strokeStyle = 'rgba(255,255,255,0.8)'; g.lineWidth = dpr; g.beginPath(); g.arc(X(0.3127), Y(0.329), 3 * dpr, 0, 7); g.stroke();
    g.font = `400 ${Math.round(10.5 * dpr)}px ${FONT}`; g.textAlign = 'left'; g.textBaseline = 'middle';
    g.fillStyle = 'rgba(235,238,242,0.85)'; g.fillText('2020', X(0.715), Y(0.3));
    g.fillStyle = 'rgba(200,205,212,0.8)'; g.fillText('P3', X(0.685), Y(0.345));
    g.fillStyle = 'rgba(150,160,170,0.8)'; g.fillText('709', X(0.6), Y(0.375));
    g.fillStyle = 'rgba(225,200,140,0.75)'; g.textAlign = 'left'; g.fillText('CIE 1931 xy', pad, pad + 4 * dpr);
  }
  /* BT.2020 Y'CbCr of a gamma-encoded colour */
  const ycc = (r, gg, b) => { const y = 0.2627 * r + 0.678 * gg + 0.0593 * b; return [(b - y) / 1.8814, (r - y) / 1.4746]; };
  function drawVector(cv, data, o) {
    const [g, w, h, dpr] = sizeCanvas(cv, o.maxDpr);
    g.fillStyle = `rgb(${BG})`; g.fillRect(0, 0, w, h);
    const R = Math.max(1, Math.min(w, h) / 2 - 12 * dpr), cx = w / 2, cy = h / 2, zoom = o.zoom || 1;   /* a hidden or tiny canvas must not throw */
    const P = (cb, cr) => [cx + cb * 2 * R * zoom, cy - cr * 2 * R * zoom];
    if (data && data.vec) {
      const N = data.vecN, k = 0.5 * Math.pow(2, o.gain || 0), cell = 2 * R * zoom / N;
      for (let j = 0; j < N; j++) for (let i = 0; i < N; i++) {
        const n = data.vec[j * N + i]; if (!n) continue;
        const cb = (i + 0.5) / N - 0.5, cr = (j + 0.5) / N - 0.5, [x, y] = P(cb, cr);
        if (x < 0 || y < 0 || x > w || y > h) continue;
        const yv = 0.55, rr = yv + 1.4746 * cr, bb = yv + 1.8814 * cb, gg = (yv - 0.2627 * rr - 0.0593 * bb) / 0.678;
        const l = 0.3 + 0.7 * (1 - Math.exp(-n * k)), m = Math.max(rr, gg, bb, 0.001);
        const c = [rr, gg, bb].map((v) => Math.max(0, v / m) * 0.75 + 0.25);
        g.fillStyle = `rgb(${Math.round(c[0] * 255 * l)},${Math.round(c[1] * 255 * l)},${Math.round(c[2] * 255 * l)})`;
        g.fillRect(x - cell / 2, y - cell / 2, Math.max(1, Math.ceil(cell)), Math.max(1, Math.ceil(cell)));
      }
    }
    g.lineWidth = dpr; g.strokeStyle = 'rgba(200,170,90,0.35)';
    g.beginPath(); g.arc(cx, cy, R, 0, 7); g.stroke();
    g.strokeStyle = 'rgba(200,170,90,0.16)';
    g.beginPath(); g.moveTo(cx - R, cy); g.lineTo(cx + R, cy); g.moveTo(cx, cy - R); g.lineTo(cx, cy + R); g.stroke();
    g.beginPath(); g.arc(cx, cy, R / 2, 0, 7); g.stroke();
    /* skin tone line */
    const sk = ycc(0.78, 0.56, 0.45), ang = Math.atan2(sk[1], sk[0]);
    g.strokeStyle = 'rgba(225,200,140,0.35)'; g.setLineDash([4 * dpr, 4 * dpr]); g.beginPath(); g.moveTo(cx, cy);
    g.lineTo(cx + Math.cos(ang) * R, cy - Math.sin(ang) * R); g.stroke(); g.setLineDash([]);
    /* 75 % and 100 % targets */
    g.font = `400 ${Math.round(10.5 * dpr)}px ${FONT}`; g.textAlign = 'center'; g.textBaseline = 'middle';
    for (const [name, c] of [['R', [1, 0, 0]], ['Mg', [1, 0, 1]], ['B', [0, 0, 1]], ['Cy', [0, 1, 1]], ['G', [0, 1, 0]], ['Yl', [1, 1, 0]]]) {
      const [cb, cr] = ycc(...c), [x, y] = P(cb, cr), [x7, y7] = P(cb * 0.75, cr * 0.75), b = 5 * dpr;
      g.strokeStyle = 'rgba(200,170,90,0.6)'; g.strokeRect(x7 - b, y7 - b, 2 * b, 2 * b);
      g.strokeStyle = 'rgba(200,170,90,0.3)'; g.strokeRect(x - b * 0.6, y - b * 0.6, 1.2 * b, 1.2 * b);
      const lx = cx + (x - cx) * 1.0 + Math.sign(x - cx) * 12 * dpr, ly = cy + (y - cy) * 1.0 + Math.sign(y - cy) * 10 * dpr;
      g.fillStyle = 'rgba(225,200,140,0.85)'; g.fillText(name, lx, ly);
    }
    g.textAlign = 'left'; g.fillStyle = 'rgba(225,200,140,0.75)'; g.fillText('Vectorscope', 10 * dpr, 12 * dpr);
  }
  function parseScopes(buf) {
    const h = new Uint32Array(buf, 0, 6), cieW = h[0], cieH = h[1], vecN = h[2], frame = h[4] + h[5] * 4294967296;
    return { frame, cieW, cieH, vecN, cie: new Uint16Array(buf, 24, cieW * cieH), vec: new Uint16Array(buf, 24 + cieW * cieH * 2, vecN * vecN) };
  }

  const nits = (n) => (n == null || !isFinite(n) ? '–' : n >= 100 ? Math.round(n).toLocaleString('en-AU') : n >= 10 ? n.toFixed(1) : n.toFixed(2));
  const codeNits = (code, dpl) => dpl * Math.pow(code / 255, 2.2);
  /* Throttle: run f at most every ms, always running the last call. */
  /* holding a key or a ‹ ›: single steps for the first second, then 2×, 5× from 2.5 s, 10× from 4 s at most;
   * a setting of fewer than 50 steps always moves one at a time */
  const holdMult = (ms, steps) => (steps < 50 || !(ms >= 1500) ? 1 : ms < 3000 ? 2 : ms < 5000 ? 5 : 10);
  /* k steps of st from v, landing on a multiple of k·st (round values at the bigger steps) */
  function stepBy(v, d, st, k) {
    const g = st * (k || 1);
    return +(d > 0 ? Math.floor(v / g + 1e-9) * g + g : Math.ceil(v / g - 1e-9) * g - g).toPrecision(12);
  }
  /* SDR video on screen (/api/info): something plays, or the HDMI input carries a steady signal, and it is not HDR10, HDR10+, HLG or Dolby Vision */
  function sdrPlaying(i) {
    const h = i && i.hdmi;
    if (h && h.on) return !!h.stable && !/HDR|HLG|ST2084|Dolby|Vision|LLDV/i.test(h.hdr || '');
    return !!(i && i.playing) && !/HDR|HLG|ST2084|Dolby|Vision|LLDV/i.test((i.input && i.input.hdr) || '');
  }
  function throttle(f, ms) {
    let last = 0, timer = null, args = null;
    return function (...a) {
      args = a;
      const now = performance.now(), wait = last + ms - now;
      if (wait <= 0) { last = now; f(...args); }
      else if (!timer) timer = setTimeout(() => { timer = null; last = performance.now(); f(...args); }, wait);
    };
  }

  /* geometry: where the picture lands on the screen (fractions, 0..1), as geometry.c and mesh.c shape it */
  const gclamp = (v, a, b) => Math.min(b, Math.max(a, v));
  function geoCorners(p, skip) {
    const c = [[0, 0], [1, 0], [1, 1], [0, 1]], v = (+p.geo_kv || 0) / 200, h = (+p.geo_kh || 0) / 200;
    if (v > 0) { c[0][0] += v; c[1][0] -= v; } else { c[3][0] -= v; c[2][0] += v; }
    if (h > 0) { c[0][1] += h; c[3][1] -= h; } else { c[1][1] -= h; c[2][1] += h; }
    for (let i = 0; i < 4; i++) if (i !== skip) { c[i][0] = gclamp(c[i][0] + (+p[`geo_c${i}x`] || 0) / 100, 0, 1); c[i][1] = gclamp(c[i][1] + (+p[`geo_c${i}y`] || 0) / 100, 0, 1); }
    return c;
  }
  function geoShaper(p) {
    const pin = (+p.geo_pin || 0) / 100, cur = (+p.geo_curve || 0) / 100, cx = 8, cy = 4.5, hd = Math.hypot(cx, cy);
    const sh = (x, y) => { const X = (x - 0.5) * 16, Y = (y - 0.5) * 9, u = X / hd, w = Y / hd, k = 1 + pin * (u * u + w * w), q = X / cx; return [X * k, Y * k * (1 + cur * q * q)]; };
    let e = 0;
    for (let i = 0; i <= 32; i++) for (const [x, y] of [[i / 32, 0], [i / 32, 1], [0, i / 32], [1, i / 32]]) { const [a, b] = sh(x, y); e = Math.max(e, Math.abs(a) / cx, Math.abs(b) / cy); }
    const f = e > 0 ? 1 / e : 1;
    return (x, y) => { if (!pin && !cur) return [x, y]; const [a, b] = sh(x, y); return [a * f / 16 + 0.5, b * f / 9 + 0.5]; };
  }
  const geoQuad = (c, x, y) => [0, 1].map((k) => (c[0][k] * (1 - x) + c[1][k] * x) * (1 - y) + (c[3][k] * (1 - x) + c[2][k] * x) * y);
  function geoGrid(p) { const n = Math.round(+p.geo_grid || 0); return n ? [n, n === 4 ? 3 : n] : [0, 0]; }
  function geoBase(p, i) {
    const [gw, gh] = geoGrid(p), sh = geoShaper(p), c = geoCorners(p);
    const [x, y] = sh((i % gw) / (gw - 1), Math.floor(i / gw) / (gh - 1)); return geoQuad(c, x, y);
  }
  function geoOutline(p, n) {
    const sh = geoShaper(p), c = geoCorners(p), pts = [];
    for (const [x0, y0, x1, y1] of [[0, 0, 1, 0], [1, 0, 1, 1], [1, 1, 0, 1], [0, 1, 0, 0]])
      for (let i = 0; i < n; i++) pts.push(geoQuad(c, ...sh(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n)));
    return pts;
  }
  const geo = { corners: geoCorners, shaper: geoShaper, quad: geoQuad, grid: geoGrid, base: geoBase, outline: geoOutline };

  return { sdrPlaying, holdMult, stepBy, pq, ipq, kneeNits, curveNits, targetRaw, frameLevels, liveCurve, effective, peakKey, peakNits, setPqSink, GRID, Wave, drawCie, drawVector, parseScopes, nits, codeNits, throttle, FONT, geo };
})();

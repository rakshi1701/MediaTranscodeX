// Media_TranscodeX Web - FFmpeg.wasm Engine & Client-Side Transcoder

let ffmpeg = null;
let sourceFile = null;
let externalAudio = [];
let externalSubtitles = [];
let includeOriginalAudio = true;
let mediaDuration = 0;
let sourceAudioTracks = []; // [{streamIndex, lang, codec, enabled}] — filled by probe
let probeCapture = null;    // when non-null, FFmpeg log lines go here instead of terminal

// DOM Elements
const dropzone = document.getElementById('dropzone');
const fileInput = document.getElementById('fileInput');
const infoTree = document.getElementById('infoTree');
const outputFileNameInput = document.getElementById('outputFileNameInput');
const formatSelect = document.getElementById('formatSelect');
const resSelect = document.getElementById('resSelect');
const customResGroup = document.getElementById('customResGroup');
const widthInput = document.getElementById('widthInput');
const heightInput = document.getElementById('heightInput');
const bitrateSelect = document.getElementById('bitrateSelect');
const presetSelect = document.getElementById('presetSelect');
const audioBitrateSelect = document.getElementById('audioBitrateSelect');
const muteAudioCheck = document.getElementById('muteAudioCheck');
const trackList = document.getElementById('trackList');
const addAudioBtn = document.getElementById('addAudioBtn');
const addSubBtn = document.getElementById('addSubBtn');
const audioFileInput = document.getElementById('audioFileInput');
const subFileInput = document.getElementById('subFileInput');
const startBtn = document.getElementById('startBtn');
const cancelBtn = document.getElementById('cancelBtn');
const downloadBtn = document.getElementById('downloadBtn');
const progressBarFill = document.getElementById('progressBarFill');
const progressText = document.getElementById('progressText');
const statusMessage = document.getElementById('statusMessage');
const terminalLog = document.getElementById('terminalLog');
const outputVideo = document.getElementById('outputVideo');

// Logging helper
function log(msg, isError = false) {
    const div = document.createElement('div');
    div.textContent = `[${new Date().toLocaleTimeString()}] ${msg}`;
    if (isError) div.style.color = '#f43f5e';
    terminalLog.appendChild(div);
    terminalLog.scrollTop = terminalLog.scrollHeight;
}

// Single-threaded FFmpeg.wasm core. A multi-threaded core-mt build was tried and
// reverted: it hung indefinitely mid-transcode (stream mapping completed, but no
// frame= progress ever printed) — a known category of issue with ffmpeg.wasm's
// nested pthread-worker model across browsers. This core needs no COOP/COEP
// cross-origin isolation, so it works with a plain static-file deployment.
const FFMPEG_CORE_BASE_URL = 'https://unpkg.com/@ffmpeg/core@0.12.10/dist/umd';
const FFMPEG_CORE_HASHES = {
    'ffmpeg-core.js':   'sKfkiFtvUk+vexk+0EUhEh366190/4WpgUAsUvaxEfyg7+E1Zt5Y5hrsU808g8Q9',
    'ffmpeg-core.wasm': 'U1VDhkPYrM3wTCT4/vjSpSsKqG/UjljYrYCI4hBSJ02svbCkxuCi6U6u/peg5vpW',
};

// Fetches a core file and verifies its SHA-384 digest against the pinned hash above
// before handing it back as a blob: URL. The core is loaded dynamically at runtime
// (not via a <script> tag), so this is the only way to get SRI-equivalent protection
// against a tampered/compromised CDN response for it.
async function fetchVerifiedBlobURL(fileName, mimeType) {
    const url = `${FFMPEG_CORE_BASE_URL}/${fileName}`;
    const res = await fetch(url);
    if (!res.ok) throw new Error(`Failed to fetch ${url}: HTTP ${res.status}`);
    const buf = await res.arrayBuffer();
    const digest = await crypto.subtle.digest('SHA-384', buf);
    const actualHash = btoa(String.fromCharCode(...new Uint8Array(digest)));
    const expectedHash = FFMPEG_CORE_HASHES[fileName];
    if (actualHash !== expectedHash) {
        throw new Error(`Integrity check failed for ${fileName} (expected ${expectedHash}, got ${actualHash})`);
    }
    return URL.createObjectURL(new Blob([buf], { type: mimeType }));
}

// Initialize FFmpeg WebAssembly Module (single-threaded core, zero CORS restrictions)
async function initFFmpeg() {
    if (ffmpeg && ffmpeg.loaded) return true;
    try {
        log('Loading WebAssembly core modules (FFmpeg.wasm core)...');
        statusMessage.textContent = 'Loading WebAssembly Core...';

        // Fail fast with an actionable message if either vendored bundle didn't load
        // (e.g. a transient CDN/edge propagation delay after deploy), instead of a
        // cryptic destructure error surfacing later during probe/transcode.
        if (!window.FFmpegWASM || !window.FFmpegWASM.FFmpeg) {
            throw new Error('FFmpegWASM failed to load from vendor/ffmpeg.js — check the Network tab, or try a hard refresh.');
        }
        if (!window.FFmpegUtil || !window.FFmpegUtil.fetchFile) {
            throw new Error('FFmpegUtil failed to load from vendor/ffmpeg-util.js — check the Network tab, or try a hard refresh.');
        }

        const { FFmpeg } = window.FFmpegWASM;
        ffmpeg = new FFmpeg();

        ffmpeg.on('log', ({ message }) => {
            if (!message) return;
            if (probeCapture !== null) {
                // During probe: capture every line directly here, split on newlines
                message.split('\n').forEach(l => probeCapture.push(l));
            } else {
                log(message);
            }
        });

        ffmpeg.on('progress', ({ progress }) => {
            if (typeof progress === 'number') {
                const pct = Math.min(100, Math.max(0, Math.round(progress * 100)));
                progressBarFill.style.width = `${pct}%`;
                progressText.textContent = `${pct}%`;
                statusMessage.textContent = `Transcoding in progress... (${pct}%)`;
            }
        });

        await ffmpeg.load({
            coreURL: await fetchVerifiedBlobURL('ffmpeg-core.js', 'text/javascript'),
            wasmURL: await fetchVerifiedBlobURL('ffmpeg-core.wasm', 'application/wasm'),
        });

        log('⚡ WebAssembly core successfully loaded and ready!');
        statusMessage.textContent = 'WebAssembly Engine Ready.';
        return true;
    } catch (err) {
        const errorText = (err && (err.message || err.toString())) ? (err.message || err.toString()) : String(err);
        log(`Failed to load WebAssembly core: ${errorText}`, true);
        statusMessage.textContent = `Error loading WebAssembly: ${errorText}`;
        console.error('FFmpeg Load Error:', err);
        return false;
    }
}

// Event Listeners for File Selection
dropzone.addEventListener('click', () => fileInput.click());
fileInput.addEventListener('change', (e) => handleFileSelect(e.target.files[0]));

dropzone.addEventListener('dragover', (e) => {
    e.preventDefault();
    dropzone.classList.add('dragover');
});

dropzone.addEventListener('dragleave', () => dropzone.classList.remove('dragover'));

dropzone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropzone.classList.remove('dragover');
    if (e.dataTransfer.files.length > 0) {
        handleFileSelect(e.dataTransfer.files[0]);
    }
});

// Resolution Select Dropdown Toggle
resSelect.addEventListener('change', () => {
    if (resSelect.value === 'custom') {
        customResGroup.style.display = 'flex';
    } else {
        customResGroup.style.display = 'none';
    }
});

// Format Select Dropdown Change -> Auto Update Default Custom Output Name
formatSelect.addEventListener('change', () => {
    updateDefaultOutputFileName();
});

function updateDefaultOutputFileName() {
    if (!sourceFile) return;
    const baseName = sourceFile.name.substring(0, sourceFile.name.lastIndexOf('.')) || 'converted_media';
    if (!outputFileNameInput.value || outputFileNameInput.dataset.autoGenerated === 'true') {
        outputFileNameInput.value = `${baseName}_converted`;
        outputFileNameInput.dataset.autoGenerated = 'true';
    }
}

outputFileNameInput.addEventListener('input', () => {
    outputFileNameInput.dataset.autoGenerated = 'false';
});

// Handle Source File Inspection
async function handleFileSelect(file) {
    if (!file) return;
    sourceFile = file;
    log(`Selected file: ${file.name} (${(file.size / (1024 * 1024)).toFixed(2)} MB)`);

    // Set Default Output File Name
    outputFileNameInput.dataset.autoGenerated = 'true';
    updateDefaultOutputFileName();

    // Reset State & UI
    externalAudio = [];
    externalSubtitles = [];
    includeOriginalAudio = true;
    sourceAudioTracks = [];
    infoTree.innerHTML = '';
    downloadBtn.style.display = 'none';
    outputVideo.style.display = 'none';
    startBtn.disabled = false;

    renderTrackList();

    // Inspect Media using HTML5 Element & URL
    const url = URL.createObjectURL(file);
    const tempVideo = document.createElement('video');
    tempVideo.preload = 'metadata';
    tempVideo.src = url;

    tempVideo.onloadedmetadata = () => {
        mediaDuration = tempVideo.duration || 0;
        const width = tempVideo.videoWidth || 0;
        const height = tempVideo.videoHeight || 0;

        renderTreeItem('File Name', file.name);
        renderTreeItem('File Size', `${(file.size / (1024 * 1024)).toFixed(2)} MB`);
        renderTreeItem('Duration', `${mediaDuration.toFixed(2)} seconds`);

        if (width > 0 && height > 0) {
            renderTreeItem('Resolution', `${width} x ${height}`);
            renderTreeItem('Aspect Ratio', `${(width / height).toFixed(2)}:1`);
        }

        statusMessage.textContent = `Inspected: ${file.name}. Ready for transcode.`;
    };

    // Load FFmpeg then probe for audio tracks (runs in background, updates track list when done)
    initFFmpeg().then(async (loaded) => {
        if (!loaded || !ffmpeg || !ffmpeg.loaded) return;
        // Only probe the file that was selected (guard against fast file switching)
        if (sourceFile !== file) return;
        sourceAudioTracks = await probeAudioTracks(file);
        if (sourceFile === file) renderTrackList(); // update UI with detected tracks
    });
}

function renderTreeItem(key, val) {
    const div = document.createElement('div');
    div.className = 'tree-item';
    div.innerHTML = `<span class="tree-key">${key}:</span><span class="tree-val">${val}</span>`;
    infoTree.appendChild(div);
}

// Probe source file for audio streams using ffmpeg -i (exits non-zero intentionally)
async function probeAudioTracks(file) {
    try {
        const { fetchFile } = window.FFmpegUtil;
        const ext = file.name.split('.').pop() || 'mp4';
        const vfsName = `probe_tmp.${ext}`;
        await ffmpeg.writeFile(vfsName, await fetchFile(file));

        // Dual-capture: intercept both the FFmpeg.wasm logger callback (probeCapture)
        // AND console.log/error — FFmpeg.wasm routes output to both paths, and the
        // delivery timing of each can differ.
        probeCapture = [];
        const consoleCaptured = [];
        const origConsoleLog   = console.log;
        const origConsoleError = console.error;
        const origConsoleWarn  = console.warn;
        const intercept = (...args) => {
            const msg = args.map(a => (a != null ? String(a) : '')).join(' ');
            if (msg) consoleCaptured.push(msg);
        };
        console.log   = intercept;
        console.error = intercept;
        console.warn  = intercept;

        try { await ffmpeg.exec(['-i', vfsName]); } catch (_) {}

        // Wait 300 ms: logger postMessages from the Web Worker are macrotasks that
        // can arrive well after exec() rejects.
        await new Promise(r => setTimeout(r, 300));

        const loggerCaptured = [...probeCapture];
        probeCapture = null;

        // Restore console before any further work
        console.log   = origConsoleLog;
        console.error = origConsoleError;
        console.warn  = origConsoleWarn;

        try { await ffmpeg.deleteFile(vfsName); } catch (_) {}

        // Merge both capture paths; split each message on '\n'; deduplicate
        const seen = new Set();
        const lines = [
            ...loggerCaptured.flatMap(m => m.split('\n')),
            ...consoleCaptured.flatMap(m => m.split('\n'))
        ].filter(l => { if (seen.has(l)) return false; seen.add(l); return true; });

        const audioLineCount = lines.filter(l => l.includes('Audio:')).length;
        log(`Probe: ${loggerCaptured.length} logger msg(s) + ${consoleCaptured.length} console msg(s) → ${audioLineCount} audio line(s).`);

        const tracks = [];
        lines.forEach(line => {
            if (!line.includes('Stream #') || !line.includes('Audio:')) return;
            const idxM   = line.match(/Stream #\d+:(\d+)/);
            const langM  = line.match(/Stream #\d+:\d+\((\w+)\)/);
            const codecM = line.match(/Audio:\s*([^\s,(]+)/);
            tracks.push({
                streamIndex: idxM   ? parseInt(idxM[1]) : tracks.length,
                lang:        langM  ? langM[1]           : '',
                codec:       codecM ? codecM[1]          : 'audio',
                enabled: true
            });
        });

        log(`Probe complete: found ${tracks.length} audio track(s) in ${file.name}.` +
            (tracks.length > 0 ? ' ' + tracks.map(t =>
                `#${t.streamIndex}${t.lang ? '(' + t.lang + ')' : ''}[${t.codec}]`
            ).join(', ') : ''));

        return tracks;
    } catch (e) {
        probeCapture = null;
        log(`Audio probe error: ${e.message}`, true);
        return [];
    }
}

// Returns FFmpeg -map args for all currently enabled internal audio tracks.
// Falls back to includeOriginalAudio flag when no probe data is available.
function getEnabledInternalAudioMaps() {
    if (sourceAudioTracks.length > 0) {
        return sourceAudioTracks.filter(t => t.enabled).map(t => `0:${t.streamIndex}`);
    }
    return includeOriginalAudio ? ['0:a'] : [];
}

function renderTrackList() {
    trackList.innerHTML = '';

    // Primary Video Track
    const vDiv = document.createElement('div');
    vDiv.className = 'track-card';
    vDiv.innerHTML = `
        <div class="track-info">
            <span class="track-badge badge-video">VIDEO</span>
            <span>Primary Video Stream</span>
        </div>
    `;
    trackList.appendChild(vDiv);

    // Internal Audio Tracks
    if (!muteAudioCheck.checked) {
        if (sourceAudioTracks.length > 0) {
            // Per-track display when probe data is available (Bug 1 fix)
            sourceAudioTracks.forEach((track, idx) => {
                const label = `Internal Audio Stream #${track.streamIndex}` +
                    (track.lang  ? ` [${track.lang}]`  : '') +
                    (track.codec ? ` (${track.codec})` : '');
                const div = document.createElement('div');
                div.className = 'track-card';
                if (!track.enabled) div.style.opacity = '0.45';
                div.innerHTML = `
                    <div class="track-info">
                        <span class="track-badge badge-audio">AUDIO</span>
                        <span>${label}</span>
                    </div>
                    <button class="btn btn-secondary" style="padding: 4px 10px; font-size: 11px; color: ${track.enabled ? 'var(--accent-rose)' : 'var(--accent-emerald)'};"
                        onclick="toggleSourceAudio(${idx})">${track.enabled ? 'Remove' : 'Restore'}</button>
                `;
                trackList.appendChild(div);
            });
        } else {
            // Fallback before probe finishes — single toggle card (Bug 2 fix)
            const aDiv = document.createElement('div');
            aDiv.className = 'track-card';
            if (!includeOriginalAudio) aDiv.style.opacity = '0.45';
            aDiv.innerHTML = `
                <div class="track-info">
                    <span class="track-badge badge-audio">AUDIO</span>
                    <span>Primary Internal Audio Stream</span>
                </div>
                <button class="btn btn-secondary" style="padding: 4px 8px; font-size: 11px; color: ${includeOriginalAudio ? 'var(--accent-rose)' : 'var(--accent-emerald)'};"
                    onclick="toggleOriginalAudio()">${includeOriginalAudio ? 'Remove' : 'Restore'}</button>
            `;
            trackList.appendChild(aDiv);
        }
    }

    // External Audio Tracks
    externalAudio.forEach((item, index) => {
        const div = document.createElement('div');
        div.className = 'track-card';
        div.innerHTML = `
            <div class="track-info">
                <span class="track-badge badge-audio">AUDIO</span>
                <span>Ext: ${item.file.name} (+${item.delaySec}s delay${item.maxDurationSec > 0 ? `, max ${item.maxDurationSec}s` : ''})</span>
            </div>
            <button class="btn btn-secondary" style="padding: 4px 8px; font-size: 11px; color: var(--accent-rose);" onclick="removeExternalAudio(${index})">Delete</button>
        `;
        trackList.appendChild(div);
    });

    // External Subtitle Tracks
    externalSubtitles.forEach((item, index) => {
        const div = document.createElement('div');
        div.className = 'track-card';
        div.innerHTML = `
            <div class="track-info">
                <span class="track-badge badge-subtitle">SUBTITLE</span>
                <span>Ext: ${item.file.name}</span>
            </div>
            <button class="btn btn-secondary" style="padding: 4px 8px; font-size: 11px; color: var(--accent-rose);" onclick="removeExternalSubtitle(${index})">Delete</button>
        `;
        trackList.appendChild(div);
    });
}

// Toggle individual probed audio track (Bug 1 fix)
function toggleSourceAudio(idx) {
    sourceAudioTracks[idx].enabled = !sourceAudioTracks[idx].enabled;
    const t = sourceAudioTracks[idx];
    log(`${t.enabled ? 'Restored' : 'Removed'} internal audio Stream #${t.streamIndex} from output.`);
    renderTrackList();
}

// Toggle fallback single-audio card when probe data isn't available (Bug 2 fix)
function toggleOriginalAudio() {
    includeOriginalAudio = !includeOriginalAudio;
    log(includeOriginalAudio ? 'Restored original audio track.' : 'Removed original audio from output.');
    renderTrackList();
}

function removeExternalAudio(index) {
    externalAudio.splice(index, 1);
    renderTrackList();
    log('Removed external audio track.');
}

function removeExternalSubtitle(index) {
    externalSubtitles.splice(index, 1);
    renderTrackList();
    log('Removed external subtitle track.');
}

muteAudioCheck.addEventListener('change', () => {
    renderTrackList();
    if (muteAudioCheck.checked) {
        log('Mute Audio (-an) enabled: All audio tracks will be removed.');
    }
});

// Add External Audio
addAudioBtn.addEventListener('click', () => audioFileInput.click());
audioFileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const delayStr = prompt(`Enter Start Offset Delay (seconds) for external audio "${file.name}":`, '0');
    const delaySec = parseFloat(delayStr) || 0;
    const durStr = prompt(`Max Duration (seconds) for "${file.name}" — enter 0 to match main video length:`, '0');
    const maxDurationSec = Math.max(0, parseFloat(durStr) || 0);

    externalAudio.push({ file, delaySec, maxDurationSec });
    renderTrackList();
    log(`Added external audio track: ${file.name} (delay: ${delaySec}s${maxDurationSec > 0 ? `, max: ${maxDurationSec}s` : ''})`);
});

// Add External Subtitle
addSubBtn.addEventListener('click', () => subFileInput.click());
subFileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;

    externalSubtitles.push({ file });
    renderTrackList();
    log(`Added external subtitle track: ${file.name}`);
});

// Start Transcoding & Multiplexing Workflow
startBtn.addEventListener('click', async () => {
    if (!sourceFile) return;

    const isLoaded = await initFFmpeg();
    if (!isLoaded || !ffmpeg) {
        alert('WebAssembly engine failed to load. Check browser console or network connection.');
        return;
    }

    // If probe was still running, stop capturing its output before we start transcoding
    probeCapture = null;

    startBtn.disabled = true;
    cancelBtn.style.display = 'inline-flex';  // show cancel button
    progressBarFill.style.width = '0%';
    progressText.textContent = '0%';
    statusMessage.textContent = 'Preparing virtual filesystem and loading media...';

    try {
        const { fetchFile } = window.FFmpegUtil;

        // 1. Write Primary Source File to FFmpeg VFS with isolated name
        const inputExt = sourceFile.name.split('.').pop() || 'mp4';
        const inputVfsName = `vfs_input_source.${inputExt}`;
        log(`Writing ${sourceFile.name} to WebAssembly memory...`);
        await ffmpeg.writeFile(inputVfsName, await fetchFile(sourceFile));

        // 2. Prepare Output VFS Name & Options
        const targetExt = formatSelect.value;
        const customName = (outputFileNameInput.value || 'converted_media').trim().replace(/[^a-zA-Z0-9._-]/g, '_');
        const downloadFileName = `${customName}.${targetExt}`;
        const outputVfsName = `vfs_output_converted.${targetExt}`;

        // Audio codec varies by container: WebM requires Opus, others use AAC
        const audioCodec = targetExt === 'webm' ? 'libopus' : 'aac';

        const args = ['-y', '-i', inputVfsName];

        let currentInputIndex = 1;
        const audioInputIndices = [];

        // 3. Handle External Audio Tracks
        // Use input-level -t to enforce max duration (matches Qt's maxDurationSec behaviour)
        for (let i = 0; i < externalAudio.length; i++) {
            const extAudio = externalAudio[i];
            const extAudioExt = extAudio.file.name.split('.').pop() || 'mp3';
            const extAudioVfsName = `vfs_ext_audio_${i}.${extAudioExt}`;
            await ffmpeg.writeFile(extAudioVfsName, await fetchFile(extAudio.file));
            if (extAudio.maxDurationSec > 0) args.push('-t', extAudio.maxDurationSec.toFixed(3));
            args.push('-i', extAudioVfsName);
            audioInputIndices.push(currentInputIndex);
            currentInputIndex++;
        }

        // 4. Handle External Subtitle Tracks
        const subInputIndices = [];
        for (let j = 0; j < externalSubtitles.length; j++) {
            const extSub = externalSubtitles[j];
            const extSubExt = extSub.file.name.split('.').pop() || 'srt';
            const extSubVfsName = `vfs_ext_sub_${j}.${extSubExt}`;
            await ffmpeg.writeFile(extSubVfsName, await fetchFile(extSub.file));
            args.push('-i', extSubVfsName);
            subInputIndices.push(currentInputIndex);
            currentInputIndex++;
        }

        // Audio & Stream Mapping Logic
        const isAudioOnly = ['mp3', 'wav', 'flac'].includes(targetExt);

        if (isAudioOnly) {
            args.push('-vn'); // Audio-only extraction
            if (targetExt === 'mp3') {
                args.push('-c:a', 'libmp3lame', '-b:a', audioBitrateSelect.value);
            } else if (targetExt === 'wav') {
                args.push('-c:a', 'pcm_s16le');
            } else if (targetExt === 'flac') {
                args.push('-c:a', 'flac');
            }
        } else {
            // Video Codec per Container — Fix: WebM uses VP9 not VP8
            if (['mp4', 'mkv', 'mov', 'ts', 'flv', 'avi'].includes(targetExt)) {
                args.push('-c:v', 'libx264', '-preset', presetSelect.value, '-pix_fmt', 'yuv420p');
                args.push('-b:v', bitrateSelect.value);
            } else if (targetExt === 'webm') {
                args.push('-c:v', 'libvpx-vp9', '-b:v', bitrateSelect.value);
            }

            // Video Resolution Scaling Filter
            let scaleFilter = '';
            if (resSelect.value === 'custom') {
                const w = widthInput.value || 1920;
                const h = heightInput.value || 1080;
                scaleFilter = `scale=${w}:${h}`;
            } else if (resSelect.value !== 'source') {
                const [w, h] = resSelect.value.split('x');
                scaleFilter = `scale=${w}:${h}`;
            }

            if (scaleFilter) {
                args.push('-vf', scaleFilter);
            }
        }

        // Audio Multiplexing & Mapping Logic
        if (muteAudioCheck.checked) {
            args.push('-an');
            log('Audio muted / removed (-an)');
        } else if (externalAudio.length > 0) {
            // Build adelay filter_complex only for external tracks that have a delay offset
            const delayFilters = [];
            externalAudio.forEach((ea, i) => {
                if (ea.delaySec > 0) {
                    const delayMs = Math.round(ea.delaySec * 1000);
                    delayFilters.push(`[${i + 1}:a]adelay=${delayMs}|${delayMs}[ea${i}]`);
                }
            });
            if (delayFilters.length > 0) {
                args.push('-filter_complex', delayFilters.join(';'));
            }

            if (!isAudioOnly) args.push('-map', '0:v:0');

            // Map enabled internal audio tracks (uses per-track data when available)
            const internalMaps = getEnabledInternalAudioMaps();
            internalMaps.forEach(m => args.push('-map', m));

            // Each external audio file becomes its own separate audio track
            externalAudio.forEach((ea, i) => {
                if (ea.delaySec > 0) {
                    args.push('-map', `[ea${i}]`);
                } else {
                    args.push('-map', `${i + 1}:a`);
                }
            });

            args.push('-c:a', audioCodec, '-b:a', audioBitrateSelect.value);
            log(internalMaps.length > 0
                ? 'Multiplexing external audio as separate track(s) alongside original...'
                : 'Replacing internal audio with external audio track(s)...');
        } else {
            // Standard single video transcode
            if (!isAudioOnly) args.push('-map', '0:v:0');
            const internalMaps = getEnabledInternalAudioMaps();
            if (internalMaps.length > 0) {
                internalMaps.forEach(m => args.push('-map', m));
            } else if (sourceAudioTracks.length === 0) {
                // No probe data — use optional map as fallback
                args.push('-map', '0:a?');
            }
            // If probe found tracks but all are disabled → no audio maps → effectively muted
            if (!isAudioOnly) args.push('-c:a', audioCodec, '-b:a', audioBitrateSelect.value);
        }

        // Subtitle Mapping — Fix: removed invalid '?' after stream index (s:0? → s:0)
        subInputIndices.forEach(idx => {
            args.push('-map', `${idx}:s:0`);
        });

        // Auto-match duration if external tracks added
        if (externalAudio.length > 0) {
            args.push('-shortest');
        }

        // Output file in VFS
        args.push(outputVfsName);

        log(`Running FFmpeg WebAssembly Command: ffmpeg ${args.join(' ')}`);
        statusMessage.textContent = 'Transcoding via WebAssembly engine...';

        // Execute FFmpeg Transcode Command
        await ffmpeg.exec(args);

        // 7. Read Converted Output File from VFS
        log('Transcoding complete! Reading converted file from WebAssembly memory...');
        const data = await ffmpeg.readFile(outputVfsName);
        const mimeTypes = {
            mp4: 'video/mp4', mkv: 'video/x-matroska', webm: 'video/webm',
            avi: 'video/x-msvideo', mov: 'video/quicktime', flv: 'video/x-flv',
            ts: 'video/mp2t', mp3: 'audio/mp3', wav: 'audio/wav', flac: 'audio/flac'
        };

        // Create Blob using Uint8Array view directly
        const blob = new Blob([data], { type: mimeTypes[targetExt] || 'video/mp4' });
        const downloadUrl = URL.createObjectURL(blob);

        // Update Download Link & Filename
        downloadBtn.href = downloadUrl;
        downloadBtn.download = downloadFileName;
        downloadBtn.style.display = 'inline-flex';
        downloadBtn.onclick = () => {
            log(`Downloading "${downloadFileName}"...`);
        };

        if (!isAudioOnly) {
            outputVideo.src = downloadUrl;
            outputVideo.style.display = 'block';
        }

        progressBarFill.style.width = '100%';
        progressText.textContent = '100%';
        statusMessage.innerHTML = `🎉 Transcoding completed! <a href="${downloadUrl}" download="${downloadFileName}" style="color:var(--primary-cyan); text-decoration:underline;">Click to Download ${downloadFileName}</a>`;
        log(`🎉 File successfully converted & saved as "${downloadFileName}"! Click Download Result button to save.`);

        // Cleanup VFS
        try {
            await ffmpeg.deleteFile(inputVfsName);
            await ffmpeg.deleteFile(outputVfsName);
        } catch (e) {}

    } catch (err) {
        log(`Transcoding error: ${err.message}`, true);
        statusMessage.textContent = 'Transcoding failed. Check terminal log.';
    } finally {
        startBtn.disabled = false;
        cancelBtn.style.display = 'none';  // always hide cancel when done
    }
});

// Cancel transcoding — terminates the FFmpeg WebWorker; engine reloads on next transcode
cancelBtn.addEventListener('click', () => {
    try { ffmpeg.terminate(); } catch (e) {}
    ffmpeg = null;
    startBtn.disabled = false;
    cancelBtn.style.display = 'none';
    progressBarFill.style.width = '0%';
    progressText.textContent = '0%';
    statusMessage.textContent = 'Cancelled. Engine will reload on next transcode.';
    log('Transcode cancelled by user.', true);
});

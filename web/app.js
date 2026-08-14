// Media_TranscodeX Web - FFmpeg.wasm Engine & Client-Side Transcoder

let ffmpeg = null;
let sourceFile = null;
let externalAudio = [];
let externalSubtitles = [];
let includeOriginalAudio = true;
let mediaDuration = 0;

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
const audioBitrateSelect = document.getElementById('audioBitrateSelect');
const muteAudioCheck = document.getElementById('muteAudioCheck');
const trackList = document.getElementById('trackList');
const addAudioBtn = document.getElementById('addAudioBtn');
const addSubBtn = document.getElementById('addSubBtn');
const audioFileInput = document.getElementById('audioFileInput');
const subFileInput = document.getElementById('subFileInput');
const startBtn = document.getElementById('startBtn');
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

// Initialize FFmpeg WebAssembly Module (Zero CORS restrictions)
async function initFFmpeg() {
    if (ffmpeg && ffmpeg.isLoaded()) return true;
    try {
        log('Loading WebAssembly core modules (FFmpeg.wasm v0.11.6)...');
        statusMessage.textContent = 'Loading WebAssembly Core...';

        if (typeof SharedArrayBuffer === 'undefined') {
            log('⚠️ SharedArrayBuffer is disabled by your browser security policy.', true);
            log('💡 Run "python3 server.py" in your terminal to enable Cross-Origin Isolation headers (COOP/COEP).', true);
        }

        const { createFFmpeg } = window.FFmpeg || FFmpeg;
        ffmpeg = createFFmpeg({
            corePath: 'https://unpkg.com/@ffmpeg/core@0.11.0/dist/ffmpeg-core.js',
            log: true,
            logger: m => {
                if (m && m.message) {
                    log(m.message);
                }
            },
            progress: p => {
                if (p && typeof p.ratio === 'number') {
                    const pct = Math.min(100, Math.max(0, Math.round(p.ratio * 100)));
                    progressBarFill.style.width = `${pct}%`;
                    progressText.textContent = `${pct}%`;
                    statusMessage.textContent = `Transcoding in progress... (${pct}%)`;
                }
            }
        });

        await ffmpeg.load();

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

    // Pre-load FFmpeg WebAssembly module
    initFFmpeg();
}

function renderTreeItem(key, val) {
    const div = document.createElement('div');
    div.className = 'tree-item';
    div.innerHTML = `<span class="tree-key">${key}:</span><span class="tree-val">${val}</span>`;
    infoTree.appendChild(div);
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

    // Primary Audio Track (with toggle remove option)
    if (!muteAudioCheck.checked && includeOriginalAudio) {
        const aDiv = document.createElement('div');
        aDiv.className = 'track-card';
        aDiv.innerHTML = `
            <div class="track-info">
                <span class="track-badge badge-audio">AUDIO</span>
                <span>Primary Internal Audio Stream</span>
            </div>
            <button class="btn btn-secondary" style="padding: 4px 8px; font-size: 11px; color: var(--accent-rose);" onclick="removeOriginalAudio()">Remove</button>
        `;
        trackList.appendChild(aDiv);
    }

    // External Audio Tracks
    externalAudio.forEach((item, index) => {
        const div = document.createElement('div');
        div.className = 'track-card';
        div.innerHTML = `
            <div class="track-info">
                <span class="track-badge badge-audio">AUDIO</span>
                <span>Ext: ${item.file.name} (+${item.delaySec}s delay)</span>
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

function removeOriginalAudio() {
    includeOriginalAudio = false;
    renderTrackList();
    log('Removed original audio track from transcode output.');
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

    externalAudio.push({ file, delaySec });
    renderTrackList();
    log(`Added external audio track: ${file.name} with ${delaySec}s offset`);
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

    startBtn.disabled = true;
    progressBarFill.style.width = '0%';
    progressText.textContent = '0%';
    statusMessage.textContent = 'Preparing virtual filesystem and loading media...';

    try {
        const { fetchFile } = window.FFmpeg || FFmpeg;

        // 1. Write Primary Source File to FFmpeg VFS with isolated name
        const inputExt = sourceFile.name.split('.').pop() || 'mp4';
        const inputVfsName = `vfs_input_source.${inputExt}`;
        log(`Writing ${sourceFile.name} to WebAssembly memory...`);
        ffmpeg.FS('writeFile', inputVfsName, await fetchFile(sourceFile));

        // 2. Prepare Output VFS Name & Options
        const targetExt = formatSelect.value;
        const customName = (outputFileNameInput.value || 'converted_media').trim().replace(/[^a-zA-Z0-9._-]/g, '_');
        const downloadFileName = `${customName}.${targetExt}`;
        const outputVfsName = `vfs_output_converted.${targetExt}`;

        const args = ['-y', '-i', inputVfsName];

        let currentInputIndex = 1;
        const audioInputIndices = [];

        // 3. Handle External Audio Tracks
        for (let i = 0; i < externalAudio.length; i++) {
            const extAudio = externalAudio[i];
            const extAudioExt = extAudio.file.name.split('.').pop() || 'mp3';
            const extAudioVfsName = `vfs_ext_audio_${i}.${extAudioExt}`;
            ffmpeg.FS('writeFile', extAudioVfsName, await fetchFile(extAudio.file));
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
            ffmpeg.FS('writeFile', extSubVfsName, await fetchFile(extSub.file));
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
            // Video Codec per Container
            if (['mp4', 'mkv', 'mov', 'ts', 'flv', 'avi'].includes(targetExt)) {
                args.push('-c:v', 'libx264', '-preset', 'ultrafast', '-pix_fmt', 'yuv420p');
                args.push('-b:v', bitrateSelect.value);
            } else if (targetExt === 'webm') {
                args.push('-c:v', 'libvpx', '-b:v', bitrateSelect.value);
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

            if (includeOriginalAudio) {
                args.push('-map', '0:a');  // keep original as its own audio track
            }

            // Each external audio file becomes its own separate audio track
            externalAudio.forEach((ea, i) => {
                if (ea.delaySec > 0) {
                    args.push('-map', `[ea${i}]`);
                } else {
                    args.push('-map', `${i + 1}:a`);
                }
            });

            args.push('-c:a', 'aac', '-b:a', audioBitrateSelect.value);
            log(includeOriginalAudio
                ? 'Multiplexing external audio as separate track(s) alongside original...'
                : 'Replacing internal audio with external audio track(s)...');
        } else {
            // Standard single video transcode
            if (!isAudioOnly) args.push('-map', '0:v:0');
            args.push('-map', '0:a?');
            if (!isAudioOnly) args.push('-c:a', 'aac', '-b:a', audioBitrateSelect.value);
        }

        // Subtitle Mapping
        subInputIndices.forEach(idx => {
            args.push('-map', `${idx}:s:0?`);
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
        await ffmpeg.run(...args);

        // 7. Read Converted Output File from VFS
        log('Transcoding complete! Reading converted file from WebAssembly memory...');
        const data = ffmpeg.FS('readFile', outputVfsName);
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
            ffmpeg.FS('unlink', inputVfsName);
            ffmpeg.FS('unlink', outputVfsName);
        } catch (e) {}

    } catch (err) {
        log(`Transcoding error: ${err.message}`, true);
        statusMessage.textContent = 'Transcoding failed. Check terminal log.';
    } finally {
        startBtn.disabled = false;
    }
});

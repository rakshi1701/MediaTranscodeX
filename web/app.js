// Media_TranscodeX Web - FFmpeg.wasm Engine & Client-Side Transcoder

let ffmpeg = null;
let sourceFile = null;
let externalAudio = [];
let externalSubtitles = [];
let mediaDuration = 0;

// DOM Elements
const dropzone = document.getElementById('dropzone');
const fileInput = document.getElementById('fileInput');
const infoTree = document.getElementById('infoTree');
const formatSelect = document.getElementById('formatSelect');
const resSelect = document.getElementById('resSelect');
const customResGroup = document.getElementById('customResGroup');
const widthInput = document.getElementById('widthInput');
const heightInput = document.getElementById('heightInput');
const bitrateSelect = document.getElementById('bitrateSelect');
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

// Initialize FFmpeg WebAssembly Module
async function initFFmpeg() {
    if (ffmpeg) return;
    try {
        log('Loading WebAssembly core modules (FFmpeg.wasm)...');
        const { FFmpeg } = FFmpegWASM;
        const { toBlobURL } = FFmpegUtil;

        ffmpeg = new FFmpeg();

        ffmpeg.on('log', ({ message }) => {
            if (message.includes('frame=') || message.includes('Duration') || message.includes('Stream')) {
                log(message);
            }
        });

        ffmpeg.on('progress', ({ progress, time }) => {
            const pct = Math.min(100, Math.max(0, Math.round(progress * 100)));
            progressBarFill.style.width = `${pct}%`;
            progressText.textContent = `${pct}%`;
            statusMessage.textContent = `Transcoding in progress... (${pct}%)`;
        });

        const baseURL = 'https://cdn.jsdelivr.net/npm/@ffmpeg/core@0.12.6/dist/umd';
        await ffmpeg.load({
            coreURL: await toBlobURL(`${baseURL}/ffmpeg-core.js`, 'text/javascript'),
            wasmURL: await toBlobURL(`${baseURL}/ffmpeg-core.wasm`, 'application/wasm'),
        });

        log('⚡ WebAssembly core successfully loaded and ready!');
    } catch (err) {
        log(`Failed to load WebAssembly core: ${err.message}`, true);
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

// Handle Source File Inspection
async function handleFileSelect(file) {
    if (!file) return;
    sourceFile = file;
    log(`Selected file: ${file.name} (${(file.size / (1024 * 1024)).toFixed(2)} MB)`);

    // Reset UI
    infoTree.innerHTML = '';
    trackList.innerHTML = '';
    downloadBtn.style.display = 'none';
    outputVideo.style.display = 'none';
    startBtn.disabled = false;

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

            // Add Primary Video Track to Track Manager
            addTrackCard('VIDEO', `Primary Video Stream (${width}x${height})`);
        }

        // Add Primary Audio Track to Track Manager
        addTrackCard('AUDIO', 'Primary Internal Audio Stream');

        statusMessage.textContent = `Inspected: ${file.name}. Ready for transcode.`;
    };

    // Initialize FFmpeg in background
    initFFmpeg();
}

function renderTreeItem(key, val) {
    const div = document.createElement('div');
    div.className = 'tree-item';
    div.innerHTML = `<span class="tree-key">${key}:</span><span class="tree-val">${val}</span>`;
    infoTree.appendChild(div);
}

function addTrackCard(type, label) {
    const div = document.createElement('div');
    div.className = 'track-card';
    const badgeClass = type === 'VIDEO' ? 'badge-video' : (type === 'AUDIO' ? 'badge-audio' : 'badge-subtitle');
    div.innerHTML = `
        <div class="track-info">
            <span class="track-badge ${badgeClass}">${type}</span>
            <span>${label}</span>
        </div>
    `;
    trackList.appendChild(div);
}

// Add External Audio
addAudioBtn.addEventListener('click', () => audioFileInput.click());
audioFileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const delayStr = prompt(`Enter Start Offset Delay (seconds) for external audio "${file.name}":`, '0');
    const delaySec = parseFloat(delayStr) || 0;

    externalAudio.push({ file, delaySec });
    addTrackCard('AUDIO', `External Audio: ${file.name} (+${delaySec}s delay)`);
    log(`Added external audio track: ${file.name} with ${delaySec}s offset`);
});

// Add External Subtitle
addSubBtn.addEventListener('click', () => subFileInput.click());
subFileInput.addEventListener('change', (e) => {
    const file = e.target.files[0];
    if (!file) return;

    externalSubtitles.push({ file });
    addTrackCard('SUBTITLE', `External Subtitle: ${file.name}`);
    log(`Added external subtitle track: ${file.name}`);
});

// Start Transcoding & Multiplexing Workflow
startBtn.addEventListener('click', async () => {
    if (!sourceFile) return;

    await initFFmpeg();
    if (!ffmpeg) return;

    startBtn.disabled = true;
    progressBarFill.style.width = '0%';
    progressText.textContent = '0%';
    statusMessage.textContent = 'Preparing virtual filesystem and loading media...';

    try {
        const { fetchFile } = FFmpegUtil;

        // 1. Write Primary Source File to FFmpeg VFS
        const inputName = `input_${sourceFile.name.replace(/[^a-zA-Z0-9._-]/g, '_')}`;
        log(`Writing ${sourceFile.name} to WebAssembly memory...`);
        await ffmpeg.writeFile(inputName, await fetchFile(sourceFile));

        // 2. Prepare Output Extension & Options
        const targetExt = formatSelect.value;
        const outputName = `output_${Date.now()}.${targetExt}`;
        const args = ['-i', inputName];

        // 3. Handle External Audio Tracks
        for (let i = 0; i < externalAudio.length; i++) {
            const extAudio = externalAudio[i];
            const extAudioName = `audio_ext_${i}_${extAudio.file.name.replace(/[^a-zA-Z0-9._-]/g, '_')}`;
            await ffmpeg.writeFile(extAudioName, await fetchFile(extAudio.file));

            if (extAudio.delaySec > 0) {
                args.push('-ss', extAudio.delaySec.toString());
            }
            args.push('-i', extAudioName);
        }

        // 4. Handle External Subtitle Tracks
        for (let j = 0; j < externalSubtitles.length; j++) {
            const extSub = externalSubtitles[j];
            const extSubName = `sub_ext_${j}_${extSub.file.name.replace(/[^a-zA-Z0-9._-]/g, '_')}`;
            await ffmpeg.writeFile(extSubName, await fetchFile(extSub.file));
            args.push('-i', extSubName);
        }

        // 5. Apply Video Resolution Scaling Filter
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

        // 6. Apply Video Bitrate & Container Codec Rules
        if (['mp3', 'wav', 'flac'].includes(targetExt)) {
            args.push('-vn'); // Audio-only extraction
        } else {
            args.push('-b:v', bitrateSelect.value);
        }

        // Auto-match duration if external tracks added
        if (externalAudio.length > 0) {
            args.push('-shortest');
        }

        // Output file
        args.push(outputName);

        log(`Running FFmpeg WebAssembly Command: ffmpeg ${args.join(' ')}`);
        statusMessage.textContent = 'Transcoding via WebAssembly engine...';

        // Execute FFmpeg Transcode Command
        await ffmpeg.exec(args);

        // 7. Read Output File from VFS
        log('Transcoding complete! Reading converted file from WebAssembly memory...');
        const data = await ffmpeg.readFile(outputName);
        const mimeTypes = {
            mp4: 'video/mp4', mkv: 'video/x-matroska', webm: 'video/webm',
            avi: 'video/x-msvideo', mov: 'video/quicktime', flv: 'video/x-flv',
            ts: 'video/mp2t', mp3: 'audio/mp3', wav: 'audio/wav', flac: 'audio/flac'
        };

        const blob = new Blob([data.buffer], { type: mimeTypes[targetExt] || 'video/mp4' });
        const downloadUrl = URL.createObjectURL(blob);

        // Update UI
        downloadBtn.href = downloadUrl;
        downloadBtn.download = `converted_${sourceFile.name.substring(0, sourceFile.name.lastIndexOf('.')) || 'video'}.${targetExt}`;
        downloadBtn.style.display = 'inline-flex';

        if (!['mp3', 'wav', 'flac'].includes(targetExt)) {
            outputVideo.src = downloadUrl;
            outputVideo.style.display = 'block';
        }

        progressBarFill.style.width = '100%';
        progressText.textContent = '100%';
        statusMessage.textContent = '🎉 Transcoding completed successfully!';
        log('🎉 Video successfully converted! Click Download Result to save file.');

        // Cleanup VFS
        await ffmpeg.deleteFile(inputName);
        await ffmpeg.deleteFile(outputName);

    } catch (err) {
        log(`Transcoding error: ${err.message}`, true);
        statusMessage.textContent = 'Transcoding failed. Check terminal log.';
    } finally {
        startBtn.disabled = false;
    }
});

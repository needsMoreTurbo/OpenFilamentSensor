import { initWifiPatcher } from './wifiPatcher.js';

const selectors = {
    boardSelect: document.getElementById('boardSelect'),
    boardStatus: document.getElementById('boardStatus'),
    boardVersion: document.getElementById('boardVersion'),
    boardRelease: document.getElementById('boardRelease'),
    fileList: document.getElementById('fileList'),
    notesList: document.getElementById('notesList'),
    releaseNotesTitle: document.getElementById('releaseNotesTitle'),
    installButton: document.getElementById('installButton'),
    flashTrigger: document.getElementById('flashTrigger'),
    logStream: document.getElementById('logStream'),
    copyLogBtn: document.getElementById('copyLogBtn'),
    clearLogBtn: document.getElementById('clearLogBtn'),
    heroLabel: document.getElementById('activeFirmwareLabel'),
    heroDate: document.getElementById('activeFirmwareDate'),
    wifiForm: document.getElementById('wifiPatchForm'),
    wifiStatus: document.getElementById('wifiPatchStatus'),
    wifiAcceptBtn: document.getElementById('wifiAcceptBtn'),
    wifiPatchDialog: document.getElementById('wifiPatchDialog'),
    boardCount: document.getElementById('boardCount')
};

const state = {
    boards: [],
    selected: null,
    capturing: false,
    dialogOpen: false,
    dialogLogOverlay: null,
    dialogLogStream: null,
    dialogSlot: null,
    logHistoryLimit: 400,
    consoleOriginals: {},
    wifiPatcher: null
};

const formatDate = (value) => {
    if (!value) return '—';
    const date = new Date(value);
    if (Number.isNaN(date.getTime())) return value;
    return new Intl.DateTimeFormat('en-US', { month: 'short', day: 'numeric', year: 'numeric' }).format(date);
};

const resolveAssetUrl = (path) => {
    if (!path) return '';
    return new URL(path, import.meta.url).href;
};

const appendLog = (message, level = 'info') => {
    const entry = document.createElement('p');
    entry.className = `log-entry ${level}`;
    const time = new Date().toLocaleTimeString();
    entry.textContent = `[${time}] ${message}`;
    selectors.logStream.appendChild(entry);
    selectors.logStream.scrollTop = selectors.logStream.scrollHeight;
    if (state.dialogLogStream) {
        const mirror = entry.cloneNode(true);
        state.dialogLogStream.appendChild(mirror);
        if (state.dialogLogStream.children.length > state.logHistoryLimit) {
            state.dialogLogStream.removeChild(state.dialogLogStream.firstChild);
        }
        state.dialogLogStream.scrollTop = state.dialogLogStream.scrollHeight;
    }
};

const installConsoleCapture = () => {
    ['log', 'info', 'warn', 'error'].forEach((level) => {
        const original = console[level].bind(console); // eslint-disable-line no-console
        state.consoleOriginals[level] = original;
        console[level] = (...args) => { // eslint-disable-line no-console
            if (state.capturing && args.length) {
                appendLog(args.map((item) => {
                    if (typeof item === 'string') return item;
                    try {
                        return JSON.stringify(item);
                    } catch (err) {
                        return String(item);
                    }
                }).join(' '), level === 'log' ? 'info' : level);
            }
            original(...args);
        };
    });
};

const startCapture = (boardLabel) => {
    if (!state.capturing) {
        state.capturing = true;
        toggleDialogLogOverlay(true);
        appendLog(`Starting flashing session for ${boardLabel}`, 'info');
    }
};

const stopCapture = (reason = 'Installer closed') => {
    if (!state.capturing) return;
    appendLog(reason, 'info');
    state.capturing = false;
    toggleDialogLogOverlay(false);
};

const renderBoards = () => {
    selectors.boardSelect.innerHTML = '';
    state.boards.forEach((board) => {
        const option = document.createElement('option');
        option.value = board.id;
        option.textContent = `${board.variant} · ${board.chipFamily}`;
        selectors.boardSelect.appendChild(option);
    });
};

const renderFiles = (files = []) => {
    selectors.fileList.innerHTML = '';
    files.forEach((file) => {
        const item = document.createElement('li');
        item.textContent = file;
        selectors.fileList.appendChild(item);
    });
};

const renderNotes = (notes = []) => {
    selectors.notesList.innerHTML = '';
    if (!notes.length) {
        const fallback = document.createElement('li');
        fallback.textContent = 'No release notes provided for this build.';
        selectors.notesList.appendChild(fallback);
        return;
    }
    notes.forEach((note) => {
        const li = document.createElement('li');
        li.textContent = note;
        selectors.notesList.appendChild(li);
    });
};

const toggleDialogLogOverlay = (visible) => {
    const overlay = state.dialogLogOverlay;
    if (!overlay) return;
    overlay.classList.toggle('visible', visible);
    overlay.classList.toggle('hidden', !visible);
};

const createDialogLogOverlay = () => {
    if (state.dialogLogOverlay) return;
    const overlay = document.createElement('section');
    overlay.id = 'install-dialog-log-overlay';
    overlay.className = 'install-dialog-log-overlay hidden';

    const header = document.createElement('div');
    header.className = 'install-dialog-log-header';
    header.innerHTML = '<span>Installer console</span>';

    const closeBtn = document.createElement('button');
    closeBtn.type = 'button';
    closeBtn.className = 'install-dialog-log-close';
    closeBtn.setAttribute('aria-label', 'Hide installer console');
    closeBtn.textContent = '×';
    closeBtn.addEventListener('click', () => toggleDialogLogOverlay(false));
    header.appendChild(closeBtn);

    const stream = document.createElement('div');
    stream.className = 'log-stream dialog';
    stream.setAttribute('role', 'log');
    stream.setAttribute('aria-live', 'polite');
    stream.setAttribute('aria-relevant', 'additions');
    stream.innerHTML = '<p class="muted">Installer console output will appear here.</p>';

    const panel = document.createElement('div');
    panel.className = 'install-dialog-log-panel';
    panel.appendChild(header);
    panel.appendChild(stream);

    const slot = document.createElement('div');
    slot.className = 'install-dialog-slot';
    panel.appendChild(slot);

    overlay.appendChild(panel);

    state.dialogSlot = slot;
    document.body.appendChild(overlay);

    state.dialogLogOverlay = overlay;
    state.dialogLogStream = stream;
};

const hydrateBoardDetails = (board) => {
    if (!board) return;
    state.selected = board;
    selectors.boardStatus.textContent = board.status || 'unknown';
    selectors.boardStatus.dataset.state = board.status || 'unknown';
    selectors.boardVersion.textContent = board.version || '—';
    selectors.boardRelease.textContent = formatDate(board.released);
    selectors.releaseNotesTitle.textContent = `${board.variant} release notes`;
    selectors.heroLabel.textContent = board.variant;
    selectors.heroDate.textContent = formatDate(board.released);
    renderFiles(board.files);
    renderNotes(board.notes);
    const manifestUrl = resolveAssetUrl(board.manifest);
    if (manifestUrl) {
        selectors.installButton?.setAttribute('manifest', manifestUrl);
        state.wifiPatcher?.updateBaseManifest(manifestUrl);
    } else {
        selectors.installButton?.removeAttribute('manifest');
        state.wifiPatcher?.updateBaseManifest('');
    }
};

const fetchBoards = async () => {
    try {
        const response = await fetch('./firmware/boards.json', { cache: 'no-store' });
        if (!response.ok) throw new Error(`failed to load board list (${response.status})`);
        const data = await response.json();
        state.boards = data.boards || [];
        selectors.boardCount.textContent = state.boards.length;
        renderBoards();
        const fallback = data.defaultBoard || state.boards[0]?.id;
        selectors.boardSelect.value = fallback || '';
        const board = state.boards.find((item) => item.id === selectors.boardSelect.value);
        hydrateBoardDetails(board);
    } catch (error) {
        appendLog(`Unable to load board list: ${error.message}`, 'error');
        selectors.boardSelect.disabled = true;
        selectors.flashTrigger.disabled = true;
    }
};

const attachEvents = () => {
    selectors.boardSelect.addEventListener('change', (event) => {
        const board = state.boards.find((item) => item.id === event.target.value);
        hydrateBoardDetails(board);
        appendLog(`Switched to ${board?.variant || 'unknown board'}`, 'info');
    });

    selectors.flashTrigger.addEventListener('click', () => {
        if (!state.selected) {
            appendLog('Select a board before flashing.', 'warn');
            return;
        }
        startCapture(state.selected.variant);
    });

    selectors.copyLogBtn.addEventListener('click', async () => {
        const text = selectors.logStream.innerText.trim();
        if (!text) return;
        try {
            await navigator.clipboard.writeText(text);
            appendLog('Log copied to clipboard.', 'info');
        } catch (error) {
            appendLog(`Clipboard copy failed: ${error.message}`, 'error');
        }
    });

    selectors.clearLogBtn.addEventListener('click', () => {
        selectors.logStream.innerHTML = '<p class="muted">Log cleared.</p>';
        if (state.dialogLogStream) {
            state.dialogLogStream.innerHTML = '<p class="muted">Log cleared.</p>';
        }
    });
};

const observeInstallerDialog = () => {
    const observer = new MutationObserver(() => {
        const dialog = document.querySelector('ewt-install-dialog');
        if (dialog && !state.dialogOpen) {
            state.dialogOpen = true;
            toggleDialogLogOverlay(true);
            if (state.dialogSlot && dialog.parentElement !== state.dialogSlot) {
                state.dialogSlot.appendChild(dialog);
            }
            appendLog('Installer dialog opened. Follow the prompts to select the serial port.', 'info');
        } else if (!dialog && state.dialogOpen) {
            state.dialogOpen = false;
            stopCapture('Installer dialog closed.');
        }
    });
    observer.observe(document.body, { childList: true, subtree: true });
};

const init = async () => {
    createDialogLogOverlay();
    installConsoleCapture();
    attachEvents();
    observeInstallerDialog();
    state.wifiPatcher = initWifiPatcher({
        installButton: selectors.installButton,
        openButton: selectors.wifiAcceptBtn,
        dialog: selectors.wifiPatchDialog,
        form: selectors.wifiForm,
        statusEl: selectors.wifiStatus,
        log: appendLog
    });
    await fetchBoards();
    if (!('serial' in navigator)) {
        appendLog('Web Serial API is unavailable in this browser.', 'warn');
    }
};

document.addEventListener('DOMContentLoaded', init);

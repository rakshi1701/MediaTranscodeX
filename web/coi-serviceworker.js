/*! coi-serviceworker v0.1.7 - Guido Zufelt (MIT) */
if (typeof window === 'undefined') {
    self.addEventListener('install', () => self.skipWaiting());
    self.addEventListener('activate', (event) => event.waitUntil(self.clients.claim()));

    self.addEventListener('fetch', (event) => {
        if (event.request.cache === 'only-if-cached' && event.request.mode !== 'same-origin') return;

        event.respondWith(
            fetch(event.request)
                .then((response) => {
                    if (response.status === 0) return response;

                    const newHeaders = new Headers(response.headers);
                    newHeaders.set('Cross-Origin-Embedder-Policy', 'require-corp');
                    newHeaders.set('Cross-Origin-Opener-Policy', 'same-origin');

                    return new Response(response.body, {
                        status: response.status,
                        statusText: response.statusText,
                        headers: newHeaders,
                    });
                })
                .catch((e) => console.error(e))
        );
    });
} else {
    (() => {
        // Capture the service worker script's own URL before any async code runs.
        // window.location.href gives the PAGE url (index.html), not this JS file —
        // registering that would fail on GitHub Pages subdirectory deployments.
        const swScriptURL = document.currentScript
            ? document.currentScript.src
            : './coi-serviceworker.js';

        const reloadedBySelf = window.sessionStorage.getItem('coiReloadedBySelf');
        window.sessionStorage.removeItem('coiReloadedBySelf');
        const coepCredentialless = false;

        const coep = coepCredentialless ? 'credentialless' : 'require-corp';

        if (!window.crossOriginIsolated && !reloadedBySelf) {
            window.sessionStorage.setItem('coiReloadedBySelf', 'true');
            if ('serviceWorker' in navigator) {
                navigator.serviceWorker.register(swScriptURL).then(
                    (registration) => {
                        registration.addEventListener('updatefound', () => {
                            window.location.reload();
                        });
                        if (registration.active && !navigator.serviceWorker.controller) {
                            window.location.reload();
                        }
                    },
                    (err) => console.error('COI Service Worker registration failed:', err)
                );
            }
        }
    })();
}

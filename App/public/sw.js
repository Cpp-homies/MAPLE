self.addEventListener('install', function (event) {
    event.waitUntil(
        caches.open('my-cache').then(function (cache) {
            return cache.addAll([
                '/',
                '/index.html',
                '/styles.css',
                '/main.js',
                '/media/maple-logo-dalle-generated.webp',
                '/media/maple-logo-dalle-generated-192.webp',
                '/media/maple-logo-dalle-generated-512.webp',
                '/media/background-dalle-generated.webp'
            ]);
        })
    );
});

self.addEventListener('fetch', function (event) {
    event.respondWith(
        caches.match(event.request).then(function (response) {
            return response || fetch(event.request);
        })
    );
});

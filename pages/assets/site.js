/* ════════════════════════════════════════════════════════════════════
   FR_Math docs — shared site chrome.
   Plain vanilla JS, no framework. Injects the header nav and footer
   into every page so there's exactly one source of truth for the
   site title, version, menu, and legal line.

   Page skeleton expected in each HTML file:

       <header id="site-header"></header>
       <main class="page-content"><div class="wrapper">...</div></main>
       <footer id="site-footer"></footer>
       <script src="...assets/site.js"></script>

   Path hack: pages in docs/guide/ are one level deeper than pages in
   docs/, so nav hrefs need "../". We detect this from the URL.
   ════════════════════════════════════════════════════════════════════ */

(function () {
    var FR_VERSION = 'v2.0.1';

    // Detect whether we're a top-level page or inside guide/.
    // Works for both file:// and http(s):// because we look for the
    // literal "/guide/" segment anywhere in the path.
    var path   = window.location.pathname;
    var inSub  = /\/guide\//.test(path);
    var prefix = inSub ? '../' : '';

    // Figure out the current file name (for active-link highlighting).
    // Treat an empty tail (e.g. pathname ending in "/") as "index.html".
    var current = path.split('/').pop();
    if (!current) { current = 'index.html'; }

    // Nav items: [href relative to docs/ root, label, basename for match]
    // `basename` is what we compare against `current` to set .active.
    var nav = [
        ['index.html',                    'Home',            'index.html'],
        ['guide/getting-started.html',    'Getting Started', 'getting-started.html'],
        ['guide/fixed-point-primer.html', 'Primer',          'fixed-point-primer.html'],
        ['guide/api-reference.html',      'API',             'api-reference.html'],
        ['guide/examples.html',           'Examples',        'examples.html'],
        ['guide/building.html',           'Build &amp; Test','building.html'],
        ['releases.html',                 'Releases',        'releases.html']
    ];

    // -----------------------------------------------------------------
    // Build header
    // -----------------------------------------------------------------
    var headerEl = document.getElementById('site-header');
    if (headerEl) {
        var navHtml = '';
        for (var i = 0; i < nav.length; i++) {
            var href       = prefix + nav[i][0];
            var label      = nav[i][1];
            var basename   = nav[i][2];
            var activeAttr = (current === basename) ? ' class="active"' : '';
            navHtml += '<a href="' + href + '"' + activeAttr + '>' + label + '</a>';
        }

        headerEl.className = 'site-header';
        headerEl.innerHTML =
            '<div class="wrapper">' +
              '<a class="site-title" href="' + prefix + 'index.html">' +
                'FR_Math <span class="site-version">' + FR_VERSION + '</span>' +
              '</a>' +
              '<nav class="site-nav">' + navHtml + '</nav>' +
            '</div>';
    }

    // -----------------------------------------------------------------
    // Build footer
    // -----------------------------------------------------------------
    var footerEl = document.getElementById('site-footer');
    if (footerEl) {
        footerEl.className = 'site-footer';
        footerEl.innerHTML =
            '<div class="wrapper">' +
              '<span>FR_Math &mdash; BSD-2-Clause &mdash; ' +
                '&copy; 2000&ndash;2026 M. A. Chatterjee</span>' +
              '<span>' +
                '<a href="https://github.com/deftio/fr_math">GitHub</a>' +
                ' &middot; ' +
                '<a href="' + prefix + 'releases.html">Releases</a>' +
              '</span>' +
            '</div>';
    }

    // -----------------------------------------------------------------
    // Lazy-load highlight.js from CDN for code blocks. Doing this here
    // (instead of in every HTML file) means one JS include per page.
    // -----------------------------------------------------------------
    var HLJS_BASE = 'https://cdnjs.cloudflare.com/ajax/libs/highlight.js/11.9.0';

    // Only load if there is at least one <pre><code> on the page.
    if (document.querySelector('pre code')) {
        var link = document.createElement('link');
        link.rel  = 'stylesheet';
        link.href = HLJS_BASE + '/styles/github.min.css';
        document.head.appendChild(link);

        var script = document.createElement('script');
        script.src = HLJS_BASE + '/highlight.min.js';
        script.onload = function () {
            if (window.hljs) { window.hljs.highlightAll(); }
        };
        document.body.appendChild(script);
    }
})();

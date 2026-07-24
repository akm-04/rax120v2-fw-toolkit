// --- NordVPN Setup popup page (opened via window.open() from vpn_client.htm,
// matching the same OEM pattern as show_traffic.htm's click_status()) -------
// Talks only to nordvpn_gen.mod -- never calls api.nordvpn.com directly from
// the browser. The token itself is never requested by this page at any
// point -- only the tri-state token_status, so re-opening this popup can
// never display a previously-saved key.

var nordvpnCountries = [];
var nordvpnPingObserver = null;
var nordvpnKeyFieldOpen = false;

// Tracks every in-flight ping request so Connect can hard-cancel all of
// them instantly, plus a suspend flag so no NEW ping can start while a
// connect is in progress -- the router's CGI handling isn't meaningfully
// concurrent, so a page full of in-flight pings genuinely delays a
// generate request queued behind them.
var nordvpnActivePingControllers = [];
var nordvpnPingsSuspended = false;

// Sort state for the country table.
var nordvpnSortMode = 'name';   // 'name' | 'ping'
var nordvpnSortDir = 1;         // 1 = ascending, -1 = descending

// loadvalue() is this firmware's own convention for "page init entrypoint"
// (same name used by vpn_client.htm and show_traffic.htm, each defining
// their own) -- called once at the bottom of nordvpn_setup.htm.
function loadvalue() {
    document.getElementById('nordvpn_toggle_key').onclick = nordvpnToggleKeyField;
    document.getElementById('nordvpn_reset_link').onclick = nordvpnResetToken;
    document.getElementById('nordvpn_apply_btn').onclick = nordvpnApplyToken;
    document.getElementById('nordvpn_search').oninput = function(e) { nordvpnFilterCountries(e.target.value); };
    document.getElementById('nordvpn_token_input').onkeydown = function(e) {
        if (e.key === 'Enter') { e.preventDefault(); nordvpnApplyToken(); }
    };
    document.getElementById('nordvpn_sort_name').onclick = function() { nordvpnSortRows('name'); };
    document.getElementById('nordvpn_sort_ping').onclick = function() { nordvpnSortRows('ping'); };
    nordvpnCheckStatus();
}

// --- API key status / collapsible field ------------------------------------

function nordvpnSetBadge(status) {
    var badge = document.getElementById('nordvpn_status_badge');
    badge.className = (status === 'verified') ? 'case_green_font'
                     : (status === 'invalid') ? 'case_red_font'
                     : 'gray_choice_font';
    badge.textContent = (status === 'verified') ? 'Verified'
                       : (status === 'invalid') ? 'Invalid'
                       : (status === 'not_set') ? 'Not Set'
                       : 'Checking...';
}

function nordvpnCheckStatus() {
    nordvpnSetBadge('unknown');
    fetch('/cgi-bin/nordvpn_gen.mod?action=token_status')
        .then(function(r) { return r.json(); })
        .then(function(d) {
            var status = (d.status === 'ok') ? d.token_status : 'not_set';
            nordvpnSetBadge(status);
            document.getElementById('nordvpn_reset_link').style.display = (status === 'not_set') ? 'none' : '';
            if (status === 'verified') {
                document.getElementById('nordvpn_country_section').style.display = '';
                nordvpnLoadCountries();
            } else {
                document.getElementById('nordvpn_country_section').style.display = 'none';
            }
        })
        .catch(function() { nordvpnSetBadge('unknown'); });
}

// Field starts collapsed and is ALWAYS cleared before being shown -- there
// is no code path that puts a previously-saved value into this input,
// since the backend never sends one to begin with (defense in depth: the
// value is cleared here too, not just left "never populated").
function nordvpnCloseKeyField() {
    nordvpnKeyFieldOpen = false;
    document.getElementById('nordvpn_key_field').style.display = 'none';
    document.getElementById('nordvpn_token_input').value = '';
    document.getElementById('nordvpn_token_status').innerHTML = '';
}

function nordvpnToggleKeyField() {
    if (nordvpnKeyFieldOpen) {
        nordvpnCloseKeyField();
        return;
    }
    nordvpnKeyFieldOpen = true;
    document.getElementById('nordvpn_token_input').value = '';   // belt-and-braces, see above
    document.getElementById('nordvpn_key_field').style.display = '';
    document.getElementById('nordvpn_token_input').focus();
}

function nordvpnApplyToken() {
    var token = document.getElementById('nordvpn_token_input').value.trim();
    if (!token) { alert('Paste your NordVPN access token first.'); return; }

    var statusEl = document.getElementById('nordvpn_token_status');
    statusEl.innerHTML = 'Verifying and saving...';
    statusEl.style.color = 'orange';

    var body = 'action=set_token&token=' + encodeURIComponent(token);
    fetch('/cgi-bin/nordvpn_gen.mod', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: body })
        .then(function(r) { return r.json(); })
        .then(function(d) {
            if (d.status === 'ok') {
                statusEl.innerHTML = 'Saved.';
                statusEl.style.color = 'green';
                setTimeout(function() {
                    nordvpnCloseKeyField();
                    nordvpnCheckStatus();
                }, 700);
            } else {
                // includes rejection reasons from the backend's own gate,
                // e.g. "token has an unexpected length" / "contains
                // non-hex characters" / "token rejected" -- surfaced
                // verbatim rather than a generic message, since this is
                // an interactive wizard, not a fire-and-forget upload.
                statusEl.innerHTML = d.error || 'Token rejected.';
                statusEl.style.color = 'red';
            }
        })
        .catch(function() {
            statusEl.innerHTML = 'Network error contacting the router.';
            statusEl.style.color = 'red';
        });
}

// Removes the API key from NVRAM (both the encrypted value and the
// verified flag, via clear_token) and resets every piece of this popup's
// own client-side state back to a clean slate -- otherwise a stale
// country table with old ping numbers would keep showing under a key
// that no longer exists. Deliberately does NOT touch the separate
// vpn_client_wg_* NVRAM keys (the actual active WireGuard peer config) --
// resetting the API key shouldn't silently tear down a tunnel you're
// already connected through.
function nordvpnResetToken() {
    if (!confirm('Remove the saved NordVPN key from this router?')) { return; }

    nordvpnCancelAllPings();

    fetch('/cgi-bin/nordvpn_gen.mod', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: 'action=clear_token' })
        .then(function(r) { return r.json(); })
        .then(function() { nordvpnResetLocalState(); })
        .catch(function() { nordvpnResetLocalState(); });
}

function nordvpnResetLocalState() {
    document.getElementById('nordvpn_country_tbody').innerHTML = '';
    document.getElementById('nordvpn_search').value = '';
    document.getElementById('nordvpn_country_status').innerHTML = '';
    document.getElementById('nordvpn_connect_status').innerHTML = '';
    nordvpnCountries = [];
    nordvpnSortMode = 'name';
    nordvpnSortDir = 1;
    nordvpnResumePings();
    nordvpnCheckStatus();
}

// --- country table -----------------------------------------------------------

function nordvpnLoadCountries() {
    var statusEl = document.getElementById('nordvpn_country_status');
    var tbody = document.getElementById('nordvpn_country_tbody');
    statusEl.innerHTML = 'Loading countries...';
    statusEl.style.color = '#666';
    tbody.innerHTML = '';

    fetch('/cgi-bin/nordvpn_gen.mod?action=list_countries')
        .then(function(r) { return r.json(); })
        .then(function(d) {
            if (d.status !== 'ok' || !d.countries || !d.countries.length) {
                statusEl.innerHTML = d.error || 'Could not load country list.';
                statusEl.style.color = 'red';
                return;
            }
            statusEl.innerHTML = '';
            nordvpnCountries = d.countries.slice().sort(function(a, b) { return a.name.localeCompare(b.name); });
            nordvpnSortMode = 'name';
            nordvpnSortDir = 1;
            nordvpnRenderCountryRows(nordvpnCountries);
            nordvpnUpdateSortHeaderLabels();
        })
        .catch(function() {
            statusEl.innerHTML = 'Network error fetching country list.';
            statusEl.style.color = 'red';
        });
}

function nordvpnRenderCountryRows(countries) {
    var tbody = document.getElementById('nordvpn_country_tbody');
    tbody.innerHTML = '';

    countries.forEach(function(c, idx) {
        var row = document.createElement('tr');
        row.className = (idx % 2 === 0) ? 'odd_line' : 'even_line';
        row.setAttribute('data-country-id', c.id);
        row.setAttribute('data-country-name', c.name.toLowerCase());

        var nameCell = document.createElement('td');
        nameCell.textContent = c.name;

        var pingCell = document.createElement('td');
        pingCell.className = 'linktype';
        pingCell.style.cursor = 'pointer';
        pingCell.style.fontFamily = 'monospace';
        pingCell.textContent = '—';
        pingCell.title = 'Click to ping';
        pingCell.onclick = function() { nordvpnPingRow(row, pingCell); };

        var connectCell = document.createElement('td');
        var connectBtn = document.createElement('input');
        connectBtn.type = 'button';
        connectBtn.className = 'short_common_bt';
        connectBtn.value = 'Connect';
        connectBtn.onclick = function() { nordvpnConnect(c.id, c.name, connectBtn); };
        connectCell.appendChild(connectBtn);

        row.appendChild(nameCell);
        row.appendChild(pingCell);
        row.appendChild(connectCell);
        tbody.appendChild(row);
    });

    nordvpnSetupPingObserver();
}

function nordvpnFilterCountries(query) {
    query = (query || '').toLowerCase();
    var rows = document.getElementById('nordvpn_country_tbody').children;
    for (var i = 0; i < rows.length; i++) {
        var row = rows[i];
        var match = row.getAttribute('data-country-name').indexOf(query) !== -1;
        row.style.display = match ? '' : 'none';
    }
}

// --- sorting -------------------------------------------------------------

function nordvpnRowPingValue(row) {
    var cell = row.querySelector('.linktype');
    if (!cell) return null;
    var m = cell.textContent.match(/^(\d+)ms$/);
    return m ? parseInt(m[1], 10) : null;
}

function nordvpnSortRows(mode) {
    nordvpnSortDir = (mode === nordvpnSortMode) ? -nordvpnSortDir : 1;
    nordvpnSortMode = mode;

    var tbody = document.getElementById('nordvpn_country_tbody');
    var rows = Array.prototype.slice.call(tbody.children);

    rows.sort(function(a, b) {
        if (mode === 'name') {
            return nordvpnSortDir * a.getAttribute('data-country-name').localeCompare(b.getAttribute('data-country-name'));
        }
        var ap = nordvpnRowPingValue(a);
        var bp = nordvpnRowPingValue(b);
        // Unmeasured/failed ("n/a" or "—") rows always sink to the bottom
        // regardless of sort direction -- toggling direction should
        // never make "unknown" outrank a real measurement.
        if (ap === null && bp === null) return 0;
        if (ap === null) return 1;
        if (bp === null) return -1;
        return nordvpnSortDir * (ap - bp);
    });

    rows.forEach(function(row, i) {
        row.className = (i % 2 === 0) ? 'odd_line' : 'even_line';
        tbody.appendChild(row);
    });

    nordvpnUpdateSortHeaderLabels();
}

function nordvpnUpdateSortHeaderLabels() {
    var nameEl = document.getElementById('nordvpn_sort_name');
    var pingEl = document.getElementById('nordvpn_sort_ping');
    var arrow = nordvpnSortDir === 1 ? ' \u25B2' : ' \u25BC';
    nameEl.textContent = 'Country' + (nordvpnSortMode === 'name' ? arrow : '');
    pingEl.textContent = 'Ping' + (nordvpnSortMode === 'ping' ? arrow : '');
}

// --- pinging: lazy only, no bulk sweep ---------------------------------------
//
// Ping fires once per row, either when it scrolls into view within this
// popup window's own viewport, or when the ping cell is clicked directly.
// There is deliberately no "ping everything with a progress bar" mode --
// real-world testing showed a meaningful fraction of countries (uncommon
// ones especially: Bangladesh, Ecuador, Cayman Islands, etc.) reliably
// come back n/a regardless of retries, which made "progress toward 100%"
// a misleading thing to show. Whatever pings successfully shows a real
// number; whatever doesn't just stays n/a. Sorting (nordvpnSortRows) is
// a separate, always-instant operation over whatever ping data currently
// exists -- it never triggers pinging and never waits on anything, so
// there's no timing dependency between the two anymore.

function nordvpnSetupPingObserver() {
    if (nordvpnPingObserver) { nordvpnPingObserver.disconnect(); }

    if (typeof IntersectionObserver === 'undefined') {
        return; // no observer support -- click-to-ping still works
    }

    nordvpnPingObserver = new IntersectionObserver(function(entries) {
        entries.forEach(function(entry) {
            if (entry.isIntersecting) {
                var row = entry.target;
                nordvpnPingObserver.unobserve(row);
                var pingCell = row.querySelector('.linktype');
                if (pingCell) { nordvpnPingRow(row, pingCell); }
            }
        });
    }, { rootMargin: '150px 0px', threshold: 0.01 });

    var rows = document.getElementById('nordvpn_country_tbody').children;
    for (var i = 0; i < rows.length; i++) {
        nordvpnPingObserver.observe(rows[i]);
    }
}

// onSettled (optional) is called exactly once the ping finishes, whether
// it succeeded, failed, or was aborted -- used by the bulk-sweep runner to
// know when a slot has freed up.
function nordvpnPingRow(row, pingCell, onSettled) {
    if (nordvpnPingsSuspended) {
        if (onSettled) onSettled();
        return;
    }
    if (pingCell.getAttribute('data-pinged') === '1') {
        if (onSettled) onSettled();
        return;
    }
    pingCell.setAttribute('data-pinged', '1');
    pingCell.textContent = '...';
    pingCell.style.color = '#999';

    var controller = (typeof AbortController !== 'undefined') ? new AbortController() : null;
    if (controller) { nordvpnActivePingControllers.push(controller); }

    var countryId = row.getAttribute('data-country-id');
    var fetchOpts = controller ? { signal: controller.signal } : {};

    fetch('/cgi-bin/nordvpn_gen.mod?action=ping_country&country_id=' + encodeURIComponent(countryId), fetchOpts)
        .then(function(r) { return r.json(); })
        .then(function(d) {
            if (controller) { nordvpnActivePingControllers.splice(nordvpnActivePingControllers.indexOf(controller), 1); }
            if (d.status === 'ok' && d.ping_ms) {
                pingCell.textContent = Math.round(d.ping_ms) + 'ms';
                pingCell.title = '';
            } else {
                pingCell.textContent = 'n/a';
                pingCell.title = d.error || '';
            }
            pingCell.style.color = '#333';
            pingCell.style.cursor = 'default';
            if (onSettled) onSettled();
        })
        .catch(function(err) {
            if (controller) { nordvpnActivePingControllers.splice(nordvpnActivePingControllers.indexOf(controller), 1); }
            if (err && err.name === 'AbortError') {
                // Cancelled by Connect, not a real failure -- reset back
                // to pingable rather than showing a false "n/a".
                pingCell.removeAttribute('data-pinged');
                pingCell.textContent = '—';
                pingCell.style.color = '';
                pingCell.style.cursor = 'pointer';
            } else {
                pingCell.textContent = 'n/a';
                pingCell.style.color = '#333';
            }
            if (onSettled) onSettled();
        });
}

// Hard-stops all pinging: aborts every in-flight request and disconnects
// the lazy observer so nothing new starts either. Called the instant
// Connect is pressed.
function nordvpnCancelAllPings() {
    nordvpnPingsSuspended = true;
    if (nordvpnPingObserver) { nordvpnPingObserver.disconnect(); }
    nordvpnActivePingControllers.forEach(function(c) { c.abort(); });
    nordvpnActivePingControllers = [];
}

function nordvpnResumePings() {
    nordvpnPingsSuspended = false;
    nordvpnSetupPingObserver();
}

// --- connect -----------------------------------------------------------------

function nordvpnConnect(countryId, countryName, button) {
    // Focus entirely on generating the config -- stop every pending and
    // in-flight ping immediately rather than letting them keep queuing
    // CGI requests behind this one.
    nordvpnCancelAllPings();

    var statusEl = document.getElementById('nordvpn_connect_status');
    button.disabled = true;
    var originalLabel = button.value;
    button.value = 'Connecting...';
    statusEl.innerHTML = 'Generating config for ' + countryName + '...';
    statusEl.style.color = '#666';

    var body = 'action=generate&country_id=' + encodeURIComponent(countryId);
    fetch('/cgi-bin/nordvpn_gen.mod', { method: 'POST', headers: {'Content-Type':'application/x-www-form-urlencoded'}, body: body })
        .then(function(r) { return r.json(); })
        .then(function(d) {
            if (d.status === 'ok') {
                button.value = 'Configured';
                statusEl.innerHTML = 'NordVPN configured to ' + countryName +
                    '. Please close this window, switch protocol to WireGuard on the VPN page, and press Connect.';
                statusEl.style.color = 'green';
                // NVRAM now reflects this config exactly as if uploaded
                // manually -- refresh the opener so it picks up the new
                // endpoint/state, but do NOT auto-close this window; the
                // message above is the whole point of staying open.
                if (window.opener && !window.opener.closed) {
                    window.opener.location.reload();
                }
                nordvpnResumePings();
            } else {
                button.disabled = false;
                button.value = originalLabel;
                statusEl.innerHTML = d.error || 'Failed to generate config.';
                statusEl.style.color = 'red';
                nordvpnResumePings();
            }
        })
        .catch(function() {
            button.disabled = false;
            button.value = originalLabel;
            statusEl.innerHTML = 'Network error contacting the router.';
            statusEl.style.color = 'red';
            nordvpnResumePings();
        });
}

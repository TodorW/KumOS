#ifndef BROWSER_H
#define BROWSER_H

/* Text-mode HTML rendering for the GUI's kubrowser window - real DNS/TCP/
   TLS fetch (reuses the from-scratch TLS 1.2 stack, see tls.c) followed
   by a tag-stripping HTML-to-plain-text pass. No layout/CSS/images/JS -
   this reads a page, it doesn't render one. */

#define BROWSER_MAX_LINES 500
#define BROWSER_LINE_LEN  96

/* Fetches url ("http://host[:port]/path" or "https://..." - a scheme-less
   url is treated as https://). Follows at most one redirect. On return,
   browser_line_count()/browser_line_at() hold the rendered page text,
   browser_title() the page's <title> (or the url if none), and
   browser_status() a short human-readable result ("Loaded", "DNS failed",
   etc). Returns 0 on success, -1 on failure (status still gets a reason). */
int browser_navigate(const char *url);

int         browser_line_count(void);
const char *browser_line_at(int i);
const char *browser_title(void);
const char *browser_status(void);
const char *browser_url(void);

#endif

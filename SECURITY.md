# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 1.x.x   | ✓ Active support  |
| < 1.0   | ✗ Not supported   |

## Reporting a Vulnerability

**Do not open a public GitHub issue for security vulnerabilities.**

Please report security issues via email:

- **Email:** security@stdiobus.com
- **Subject:** `[SECURITY] stdiobus-cpp: <brief description>`

### What to Include

1. Description of the vulnerability
2. Steps to reproduce
3. Potential impact assessment
4. Suggested fix (if any)

### Response Timeline

- **Acknowledgment:** Within 48 hours
- **Initial assessment:** Within 7 days
- **Fix or mitigation:** Within 30 days for critical issues

### Process

1. We acknowledge receipt of your report
2. We investigate and confirm the vulnerability
3. We develop and test a fix
4. We release a patched version
5. We publicly disclose after the fix is available (coordinated disclosure)

## What Qualifies as a Security Issue

- Memory corruption (buffer overflow, use-after-free, double-free)
- Denial of service through crafted input
- Information disclosure through uninitialized memory reads
- Process escape or privilege escalation via worker management
- Undefined behavior exploitable by malicious input

## What Does NOT Qualify

- Crashes from intentionally invalid API usage (e.g., null pointer dereference after documented precondition violation)
- Performance issues
- Feature requests
- Issues in development/test code only

## Threat Model

stdiobus manages child processes and routes messages between them. The primary trust boundary is between the host application and worker processes. The SDK assumes:

- The host application is trusted
- Worker process binaries are trusted (configured by the host)
- Messages from workers are untrusted data (parsed, not executed)
- Configuration files are trusted (loaded at startup by the host)

## Security Practices

- All CI builds run with AddressSanitizer and UndefinedBehaviorSanitizer
- Input validation on all external data (message sizes, format checks)
- No dynamic memory execution
- No network access from the SDK itself (workers handle networking)
- Bounded buffer sizes to prevent memory exhaustion

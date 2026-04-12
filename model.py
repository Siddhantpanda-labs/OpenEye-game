import json
import os
import urllib.request

# KEY = os.environ.get("GEMINI_API_KEY", "").strip()
# if not KEY:
#     raise SystemExit("Set GEMINI_API_KEY")
KEY = "AIzaSyCAfkMKQdojEBIj_WoM3Gl8yQ_y9S2kymw"
url = f"https://generativelanguage.googleapis.com/v1beta/models?key={KEY}"
req = urllib.request.Request(url, method="GET")
with urllib.request.urlopen(req, timeout=30) as resp:
    data = json.load(resp)

for m in data.get("models", []):
    name = m.get("name", "")
    methods = m.get("supportedGenerationMethods") or []
    if "generateContent" in methods:
        print(name, "|", ", ".join(methods))
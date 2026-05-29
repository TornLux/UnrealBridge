# Pagination follow-up submit checklist

This checklist is retained as the historical split-follow-up checklist. The
active submit path folds these changes into `TornLux/UnrealBridge#2`; do not
use this document to open a separate PR for the current work.

## Historical validation

```powershell
python tools\smoke_mcp_all.py
git diff --check
```

Expected result for the no-editor portion:

- all no-editor MCP checks pass;
- no whitespace errors;
- MCP stdio schema-shape checks pass;
- common stdio client probes, error probes, response-envelope checks, and clean-shutdown checks pass;
- notification no-response probes pass;
- invalid-params notification no-response probes pass;
- required tool-argument validation probes pass;
- unknown tool-argument validation probes pass;
- pagination second-page / `MaxResults` script-shape checks pass;
- pagination helper failure-path tests pass;
- all-smoke runner self-checks pass;
- exposed tool-list negative tests pass;
- workflow path-filter negative tests pass;
- follow-up scope checker negative tests pass;
- Windows line-ending notices are acceptable.

The active combined PR body lives in `docs/plans/upstream-pr-description.md`.
